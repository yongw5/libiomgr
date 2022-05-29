#include "iomgr/http/http_client.h"

#include <memory>
#include <vector>

#include "iomgr/http/http_request.h"
#include "iomgr/io_buffer.h"
#include "iomgr/ref_counted.h"
#include "iomgr/tcp/inet_address.h"
#include "iomgr/tcp/tcp_client.h"
#include "threading/task_runner.h"
#include "util/http_parser.h"

namespace iomgr {

static const int kBufferSize = 1024;

class InternalRequest {
 public:
  InternalRequest(const InetAddress& remote, const Slice& request_text,
                  HTTPResponse* response, HTTPClient::RequestCb on_done)
      : remote_(remote),
        parser_(response),
        on_done_(on_done),
        incoming_(MakeRefCounted<GrowableIOBuffer>()),
        outgoing_(MakeRefCounted<DrainableIOBuffer>(
            MakeRefCounted<StringIOBuffer>(
                std::string(request_text.data(), request_text.size())),
            request_text.size())) {
    incoming_->SetCapacity(kBufferSize);
    outgoing_->SetOffset(0);

    TaskRunner::Get()->PostTask(std::bind(&InternalRequest::DoConnect, this));
  }

 private:
  void DoConnect();
  void OnConnectCompleted(Status status);

  void DoWriteLoop();
  void OnWriteCompleted(StatusOr<int> write_or);

  void DoReadLoop();
  void OnReadCompleted(StatusOr<int> read_or);
  bool HandleReadResult(StatusOr<int> read_or);

  void Finish(Status status);

  InetAddress remote_;
  HTTPParser<HTTPResponse> parser_;
  std::unique_ptr<TCPClient> tcp_;
  bool have_read_byte_;
  HTTPClient::RequestCb on_done_;
  RefPtr<GrowableIOBuffer> incoming_;
  RefPtr<DrainableIOBuffer> outgoing_;
};

void InternalRequest::DoConnect() {
  Status status =
      TCPClient::Connect(remote_, TCPClient::Options(),
                         std::bind(&InternalRequest::OnConnectCompleted, this,
                                   std::placeholders::_1),
                         nullptr, &tcp_);
  if (status.ok()) {
    DoWriteLoop();
  } else if (!status.IsTryAgain()) {
    Finish(status);
  }
}

void InternalRequest::OnConnectCompleted(Status status) {
  if (!status.ok()) {
    Finish(status);
    return;
  }

  DoWriteLoop();
}

void InternalRequest::DoWriteLoop() {
  while (outgoing_->BytesRemaining() > 0) {
    StatusOr<int> write_or =
        tcp_->Write(outgoing_.get(), outgoing_->BytesRemaining(),
                    std::bind(&InternalRequest::OnWriteCompleted, this,
                              std::placeholders::_1));
    if (write_or.ok()) {
      outgoing_->DidConsume(write_or.value());
    } else if (!write_or.status().IsTryAgain()) {
      Finish(write_or.status());
      return;
    }
  }

  if (outgoing_->BytesRemaining() == 0) {
    DoReadLoop();
  }
}

void InternalRequest::OnWriteCompleted(StatusOr<int> write_or) {
  if (!write_or.ok()) {
    Finish(write_or.status());
    return;
  }
  DoWriteLoop();
}

void InternalRequest::DoReadLoop() {
  bool read_more;
  do {
    if (incoming_->RemainingCapacity() == 0) {
      incoming_->SetCapacity(incoming_->capacity() * 2);
    }

    StatusOr<int> read_or =
        tcp_->Read(incoming_.get(), incoming_->RemainingCapacity(),
                   std::bind(&InternalRequest::OnReadCompleted, this,
                             std::placeholders::_1));
    if (read_or.status().IsTryAgain()) {
      return;
    }
    read_more = HandleReadResult(read_or);
  } while (read_more);
}

void InternalRequest::OnReadCompleted(StatusOr<int> read_or) {
  if (HandleReadResult(read_or)) {
    DoReadLoop();
  }
}

bool InternalRequest::HandleReadResult(StatusOr<int> read_or) {
  if (!read_or.ok()) {
    Finish(read_or.status());
    return false;
  }

  size_t bytes = read_or.value();
  if (bytes > 0) {
    if (!parser_.Parse(Slice(incoming_->data(), bytes), nullptr)) {
      Finish(Status::IOError("Failed to parse incoming data"));
      return false;
    }
  } else {
    // recieved EOF
    // TODO Check length
    Status status;
    if (!parser_.RecievedAllHeaders()) {
      status = Status::IOError("Failed to recieve all headers");
    }
    Finish(status);
    return false;
  }
  return true;  // read more data;
}

void InternalRequest::Finish(Status status) {
  if (on_done_) {
    on_done_(status);
  }
  delete this;
}

void HTTPClient::IssueRequest(const InetAddress& remote,
                              const HTTPRequest& request, RequestCb on_done,
                              HTTPResponse* response) {
  new InternalRequest(remote, request.ToString(), response, on_done);
}

}  // namespace iomgr

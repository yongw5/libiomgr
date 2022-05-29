#include "iomgr/http/http_server.h"

#include "iomgr/http/http_request.h"
#include "iomgr/http/http_response.h"
#include "iomgr/io_buffer.h"
#include "iomgr/tcp/inet_address.h"
#include "iomgr/tcp/tcp_client.h"
#include "iomgr/tcp/tcp_server.h"
#include "threading/task_runner.h"
#include "util/http_parser.h"

namespace iomgr {

static const int kBufferSize = 1024;

class InternalResponse {
 public:
  InternalResponse(std::unique_ptr<TCPClient> tcp,
                   HTTPServer::Delegate* delegate);

 private:
  void Start() { DoReadLoop(); }

  void DoReadLoop();
  void OnReadCompleted(StatusOr<int> read_or);
  // Return true if need to read more data
  bool HandleReadResult(StatusOr<int> read_or);

  void DoWriteLoop();
  void OnWriteCompleted(StatusOr<int> write_or);

  void SendResponse(const HTTPResponse& response);

  void Finish(Status status);

  std::unique_ptr<TCPClient> tcp_;
  HTTPRequest request_;
  HTTPParser<HTTPRequest> parser_;
  HTTPServer::Delegate* const delegate_;
  size_t received_body_bytes_;
  RefPtr<GrowableIOBuffer> incoming_;
  RefPtr<DrainableIOBuffer> outgoint_;
};

InternalResponse::InternalResponse(std::unique_ptr<TCPClient> tcp,
                                   HTTPServer::Delegate* delegate)
    : tcp_(std::move(tcp)),
      parser_(&request_),
      delegate_(CHECK_NOTNULL(delegate)),
      received_body_bytes_(-1),
      incoming_(MakeRefCounted<GrowableIOBuffer>()) {
  incoming_->SetCapacity(kBufferSize);

  TaskRunner::Get()->PostTask(std::bind(&InternalResponse::DoReadLoop, this));
}

void InternalResponse::DoReadLoop() {
  bool read_more;
  do {
    if (incoming_->RemainingCapacity() == 0) {
      incoming_->SetCapacity(incoming_->capacity() * 2);
    }

    StatusOr<int> read_or =
        tcp_->Read(incoming_.get(), incoming_->RemainingCapacity(),
                   std::bind(&InternalResponse::OnReadCompleted, this,
                             std::placeholders::_1));
    if (read_or.status().IsTryAgain()) {
      return;
    }
    read_more = HandleReadResult(read_or);
  } while (read_more);
}

void InternalResponse::OnReadCompleted(StatusOr<int> read_or) {
  if (HandleReadResult(read_or)) {
    DoReadLoop();
  }
}

bool InternalResponse::HandleReadResult(StatusOr<int> read_or) {
  if (!read_or.ok()) {
    return false;
  }

  size_t bytes = read_or.value();

  if (bytes > 0) {
    size_t start_of_body = 0;
    if (!parser_.Parse(Slice(incoming_->data(), bytes), &start_of_body)) {
      Finish(Status::IOError("Failed to parse incomming data"));
      return false;
    }
    if (start_of_body != 0) {
      received_body_bytes_ = bytes - start_of_body;
    }
    if (parser_.RecievedAllHeaders()) {
      if (request_.content_length() == -1 ||
          received_body_bytes_ > request_.content_length()) {
        SendResponse(HTTPResponse::BadRequest());
        return false;
      } else if (received_body_bytes_ == request_.content_length()) {
        HTTPResponse response;
        delegate_->OnHTTPRequest(request_, response);
        SendResponse(response);
        return false;
      }
    }
  } else if (bytes == 0) {
    SendResponse(HTTPResponse::BadRequest());
    return false;
  }
  return true;
}

void InternalResponse::DoWriteLoop() {
  outgoint_->SetOffset(0);
  while (outgoint_->BytesRemaining()) {
    StatusOr<int> write_or =
        tcp_->Write(outgoint_.get(), outgoint_->BytesRemaining(),
                    std::bind(&InternalResponse::OnWriteCompleted, this,
                              std::placeholders::_1));
    if (write_or.ok()) {
      outgoint_->DidConsume(write_or.value());
      continue;
    }
    if (write_or.status().IsTryAgain()) {
      return;
    } else {
      Finish(write_or.status());
    }
  }
  if (outgoint_->BytesRemaining() == 0) {
    Finish(Status());
  }
}

void InternalResponse::OnWriteCompleted(StatusOr<int> write_or) {
  if (write_or.ok()) {
    DoWriteLoop();
  } else {
    Finish(write_or.status());
  }
}

void InternalResponse::SendResponse(const HTTPResponse& response) {
  std::string msg = response.ToString();
  outgoint_ = MakeRefCounted<DrainableIOBuffer>(
      MakeRefCounted<StringIOBuffer>(msg), msg.size());
  DoWriteLoop();
}

void InternalResponse::Finish(Status status) {
  if (!status.ok()) {
    LOG(ERROR) << status.ToString();
  }
  delete this;
}

HTTPServer::HTTPServer(const InetAddress& addr, HTTPServer::Delegate* delegate)
    : delegate_(CHECK_NOTNULL(delegate)) {
  Status status =
      TCPServer::Listen(addr, iomgr::TCPServer::Options(), &server_);
  DCHECK(status.ok());

  TaskRunner::Get()->PostTask(std::bind(&HTTPServer::DoAcceptLoop, this));
}

HTTPServer::~HTTPServer() = default;

void HTTPServer::DoAcceptLoop() {
  bool accept_more;
  do {
    Status status = server_->Accept(
        &accepted_socket_,
        std::bind(&HTTPServer::OnAcceptCompleted, this, std::placeholders::_1));
    if (status.ok()) {
      accept_more = HandleAcceptResult(status);
    } else if (status.IsTryAgain()) {
      return;
    } else {
      return;
    }
  } while (accept_more);
}

void HTTPServer::OnAcceptCompleted(Status status) {
  if (HandleAcceptResult(status)) {
    DoAcceptLoop();
  }
}

bool HTTPServer::HandleAcceptResult(Status status) {
  if (!status.ok()) {
    return false;
  }
  InternalResponse* response =
      new InternalResponse(std::move(accepted_socket_), delegate_);
  return true;
}

}  // namespace iomgr

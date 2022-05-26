#include "io/tcp_server_impl.h"

#include "io/tcp_client_impl.h"
#include "util/os_error.h"
#include "util/socket_op.h"

namespace iomgr {

TCPServer::TCPServer() = default;

TCPServer::~TCPServer() = default;

Status TCPServer::Listen(const InetAddress& local, const Options& options,
                         std::unique_ptr<TCPServer>* server) {
  DCHECK(server);

  SockaddrStorage address(local);
  if (!address.IsValid()) {
    return Status::InvalidArg("Address to listen is invalid");
  }

  std::unique_ptr<TCPServerImpl> socket(new TCPServerImpl);
  if (!socket) {
    return Status::OutOfMemory("Failed to allocate TCPServer");
  }

  Status status;
  if (!(status = socket->Open(address.address_family())).ok()) {
    return status;
  }
  if (!(status = socket->Bind(local)).ok()) {
    return status;
  }
  if (options.reuse_address && !(status = socket->AllowAddressReuse()).ok()) {
    return status;
  }
  if (!(status = socket->Listen(options.backlog)).ok()) {
    return status;
  }

  server->reset(socket.release());
  return status;
}

TCPServerImpl::TCPServerImpl()
    : socket_fd_(-1),
      local_address_(),
      accept_socket_controller_(),
      accept_callback_(),
      accept_socket_(nullptr),
      remote_(nullptr),
      accepted_address_(),
      pending_accept_(false) {}

TCPServerImpl::~TCPServerImpl() = default;

Status TCPServerImpl::Open(int family) {
  DCHECK_EQ(-1, socket_fd_);
  DCHECK(family == InetAddress::kIPv4 || family == InetAddress::kIPv6);

  StatusOr<int> ret = SocketOp::socket(family, SOCK_STREAM, 0);
  if (ret.ok()) {
    Status status = FileOp::set_non_blocking(ret.value());
    if (!status.ok()) {
      LOG(ERROR) << "Failed to set nonblocking";
      FileOp::close(ret.value());
      return status;
    }
    socket_fd_.reset(ret.value());
  }
  return ret.status();
}

Status TCPServerImpl::Bind(const InetAddress& local) {
  DCHECK_NE(-1, socket_fd_);

  SockaddrStorage address(local);
  if (!address.IsValid()) {
    LOG(ERROR) << "Address to be binded is invalid";
    return Status::InvalidArg("Address is invalid");
  }

  Status status = SocketOp::bind(socket_fd_, address.addr, address.addr_len);
  if (!status.ok()) {
    LOG(ERROR) << "Failed to bind address: " << status.ToString();
    return status;
  }
  if (::getsockname(socket_fd_, address.addr, &address.addr_len) == 0) {
    local_address_.reset(new SockaddrStorage(address));
  }

  return status;
}

Status TCPServerImpl::Listen(int backlog) {
  DCHECK_NE(-1, socket_fd_);
  DCHECK_LT(0, backlog);

  Status status = SocketOp::listen(socket_fd_, backlog);

  SockaddrStorage address;

  if (!status.ok()) {
    LOG(ERROR) << "listen() failed";
    return status;
  }
  return status;
}

Status TCPServerImpl::Accept(std::unique_ptr<TCPClient>* socket,
                             TCPAcceptCb callback) {
  return Accept(socket, std::move(callback), nullptr);
}

Status TCPServerImpl::Accept(std::unique_ptr<TCPClient>* socket,
                             TCPAcceptCb callback, InetAddress* remote) {
  DCHECK(socket);
  DCHECK(callback);

  if (pending_accept_) {
    DCHECK(false) << "UNEXPECTED ERROR";
    return Status::Corruption("UNEXPECTED ERROR");
  }
  Status status = DoAccept(socket, remote);
  if (!status.IsTryAgain()) {
    return status;
  } else {
    pending_accept_ = true;
  }

  if (!IOWatcher::WatchFileDescriptor(socket_fd_, IOWatcher::kWatchRead, this,
                                      &accept_socket_controller_)) {
    LOG(ERROR) << "WatchFileIO failed on accept";
    return MapSystemError(errno);
  }

  accept_callback_ = std::move(callback);
  accept_socket_ = socket;
  remote_ = remote;
  return Status::TryAgain("ACCEPT PENDING");
}

Status TCPServerImpl::GetLocalAddress(InetAddress* local) const {
  DCHECK_NE(-1, socket_fd_);
  DCHECK(local);

  if (local_address_) {
    *local = local_address_->ToInetAddress();
    return Status();
  }

  SockaddrStorage address;
  if (::getsockname(socket_fd_, address.addr, &address.addr_len) < 0) {
    return MapSystemError(errno);
  }
  *local = address.ToInetAddress();
  local_address_.reset(new SockaddrStorage(address));
  return Status();
}

Status TCPServerImpl::AllowAddressReuse() {
  DCHECK_NE(-1, socket_fd_);

  Status status = SocketOp::set_reuse_addr(socket_fd_, true);
  if (!status.ok()) {
    LOG(ERROR) << "Failed to set SO_REUSEADDR on fd(" << socket_fd_ << "), "
               << status.ToString();
  }
  return status;
}

Status TCPServerImpl::DoAccept(std::unique_ptr<TCPClient>* socket,
                               InetAddress* remote) {
  SockaddrStorage remote_address;
  StatusOr<int> new_socket = SocketOp::accept(socket_fd_, remote_address.addr,
                                              &remote_address.addr_len);
  if (!new_socket.ok()) {
    return new_socket.status();
  }

  std::unique_ptr<TCPClientImpl> accepted_socket(new TCPClientImpl);
  Status status = accepted_socket->AdoptConnectedSocket(
      new_socket.value(), remote_address.ToInetAddress());
  if (!status.ok()) {
    return status;
  }

  if (remote) {
    *remote = remote_address.ToInetAddress();
  }
  socket->reset(accepted_socket.release());
  return status;
}

void TCPServerImpl::OnFileReadable(int fd) {
  DCHECK(accept_socket_);
  Status status = DoAccept(accept_socket_, remote_);
  if (status.IsTryAgain()) {
    return;
  }

  bool ok = accept_socket_controller_.StopWatching();
  DCHECK(ok);
  accept_socket_ = nullptr;
  remote_ = nullptr;
  pending_accept_ = false;
  std::move(accept_callback_)(status);
}

void TCPServerImpl::OnFileWritable(int fd) {
  DCHECK(false) << "UNEXPECTED ERROR";
}

}  // namespace iomgr
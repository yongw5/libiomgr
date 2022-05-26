#include "io/tcp_client_impl.h"

#include "iomgr/io_buffer.h"
#include "util/file_op.h"
#include "util/os_error.h"
#include "util/sockaddr_storage.h"
#include "util/socket_op.h"

namespace iomgr {

TCPClient::TCPClient() = default;

TCPClient::~TCPClient() = default;

Status TCPClient::Connect(const InetAddress& remote, const Options& options,
                          TCPConnectCb callback, const InetAddress* local,
                          std::unique_ptr<TCPClient>* client) {
  DCHECK(callback);
  DCHECK(client);

  SockaddrStorage address(remote);
  if (!address.IsValid()) {
    return Status::InvalidArg("Remote address is invalid");
  }

  std::unique_ptr<TCPClientImpl> socket(new TCPClientImpl);
  if (!socket) {
    return Status::OutOfMemory("Failed to allocate tcp client");
  }

  Status status;
  if (!(status = socket->Open(address.address_family())).ok()) {
    return status;
  }
  if (local && !(status = socket->Bind(*local)).ok()) {
    return status;
  }
  if (options.no_delay && !(status = socket->SetNoDelay(true)).ok()) {
    return status;
  }
  if (options.keep_alive.first &&
      !(status = socket->SetKeepAlive(true, options.keep_alive.second)).ok()) {
    return status;
  }
  if (options.receive_buffer_size > 0 &&
      !(status = socket->SetReceiveBufferSize(options.receive_buffer_size))
           .ok()) {
    return status;
  }
  if (options.send_buffer_size > 0 &&
      !(status = socket->SetSendBufferSize(options.send_buffer_size)).ok()) {
    return status;
  }

  status = socket->Connect(remote, std::move(callback));
  client->reset(socket.release());
  return status;
}

TCPClientImpl::TCPClientImpl()
    : socket_fd_(-1),
      connect_socket_controller_(),
      connect_callback_(),
      connect_state_(kNone),
      read_socket_controller_(),
      read_buf_(),
      read_buf_len_(0),
      read_callback_(),
      read_if_ready_callback_(),
      write_socket_controller_(),
      write_buf_(),
      write_buf_len_(0),
      write_callback_(),
      local_address_(),
      remote_address_() {}

TCPClientImpl::~TCPClientImpl() { Disconnect(); }

Status TCPClientImpl::Open(int family) {
  DCHECK_EQ(-1, socket_fd_);
  DCHECK(family == InetAddress::kIPv4 || family == InetAddress::kIPv6);

  StatusOr<int> ret = SocketOp::socket(family, SOCK_STREAM, 0);
  if (ret.ok()) {
    socket_fd_.reset(ret.value());
    Status status = FileOp::set_non_blocking(socket_fd_);
    if (!status.ok()) {
      LOG(ERROR) << "Failed to set nonblocking";
      FileOp::close(socket_fd_);
      socket_fd_.release();
      return status;
    }
  }
  return ret.status();
}

Status TCPClientImpl::Bind(const InetAddress& local) {
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

  local_address_.reset(new SockaddrStorage(address));
  // TODO(WUYONG) Update when port is 0
  return status;
}

Status TCPClientImpl::AdoptConnectedSocket(int socket,
                                           const InetAddress& remote) {
  DCHECK_EQ(-1, socket_fd_);
  DCHECK_NE(-1, socket);
  DCHECK(!remote_address_);

  SockaddrStorage address(remote);
  if (!address.IsValid()) {
    return Status::InvalidArg("Address is invalid");
  }

  Status status = FileOp::set_non_blocking(socket);
  if (!status.ok()) {
    FileOp::close(socket);
    return status;
  }

  socket_fd_.reset(socket);
  remote_address_.reset(new SockaddrStorage(address));
  connect_state_ = kConnected;
  return status;
}

Status TCPClientImpl::Connect(const InetAddress& remote,
                              TCPConnectCb callback) {
  DCHECK(callback);
  DCHECK_NE(-1, socket_fd_);
  DCHECK(!connect_callback_);
  DCHECK(!remote_address_);
  DCHECK_EQ(kNone, connect_state_);

  SockaddrStorage address(remote);
  if (!address.IsValid()) {
    LOG(ERROR) << "Connect to invalid address";
    return Status::InvalidArg("remote address is invalid");
  }

  connect_state_ = kConnecting;
  remote_address_.reset(new SockaddrStorage(address));
  Status status = DoConnect();
  if (!status.IsTryAgain()) {
    if (status.ok()) {
      connect_state_ = kConnected;
    } else {
      connect_state_ = kNone;
    }
    return status;
  }

  if (!IOWatcher::WatchFileDescriptor(socket_fd_, IOWatcher::kWatchWrite, this,
                                      &connect_socket_controller_)) {
    LOG(ERROR) << "WatchFileIO failed on connect";
    connect_state_ = kNone;
    return MapSystemError(errno);
  }

  connect_callback_ = std::move(callback);
  return Status::TryAgain("CONNECT PENDING");
}

StatusOr<int> TCPClientImpl::Read(IOBuffer* buf, int buf_len,
                                  TCPReadCb callback) {
  StatusOr<int> ret = ReadIfReady(
      buf, buf_len,
      std::bind(&TCPClientImpl::RetryRead, this, std::placeholders::_1));
  if (ret.status().IsTryAgain()) {
    read_buf_ = buf;
    read_buf_len_ = buf_len;
    read_callback_ = std::move(callback);
  }
  return ret;
}

StatusOr<int> TCPClientImpl::ReadIfReady(IOBuffer* buf, int buf_len,
                                         TCPReadCb callback) {
  DCHECK_NE(-1, socket_fd_);
  DCHECK_EQ(kConnected, connect_state_);  // connect done
  DCHECK(!read_if_ready_callback_);       // no read pending
  DCHECK(callback);                       // callback is valid
  DCHECK_LE(0, buf_len);                  // buf_len is valid

  StatusOr<int> ret = DoRead(buf, buf_len);
  if (!ret.status().IsTryAgain()) {
    return ret;
  }

  if (!IOWatcher::WatchFileDescriptor(socket_fd_, IOWatcher::kWatchRead, this,
                                      &read_socket_controller_)) {
    LOG(ERROR) << "WatchFileIO failed on read";
    return StatusOr<int>(MapSystemError(errno));
  }

  read_if_ready_callback_ = std::move(callback);
  return StatusOr<int>(Status::TryAgain("READ PENDING"));
}

Status TCPClientImpl::CancelReadIfReady() {
  DCHECK(read_if_ready_callback_);

  bool ok = read_socket_controller_.StopWatching();
  DCHECK(ok);

  read_if_ready_callback_ = nullptr;
  return Status();
}

StatusOr<int> TCPClientImpl::Write(IOBuffer* buf, int buf_len,
                                   TCPWriteCb callback) {
  DCHECK_NE(-1, socket_fd_);
  DCHECK_EQ(kConnected, connect_state_);  // connect done
  DCHECK(!write_callback_);               // No writing pending
  DCHECK(callback);                       // callback is valid
  DCHECK_LT(0, buf_len);                  // buf_len is valid

  StatusOr<int> ret = DoWrite(buf, buf_len);
  if (ret.status().IsTryAgain()) {
    ret = WriteWhenReady(buf, buf_len, std::move(callback));
  }
  return ret;
}

StatusOr<int> TCPClientImpl::WriteWhenReady(IOBuffer* buf, int buf_len,
                                            TCPWriteCb callback) {
  DCHECK_NE(-1, socket_fd_);
  DCHECK_EQ(kConnected, connect_state_);  // connect done
  DCHECK(!write_callback_);               // no writing pending
  DCHECK(callback);                       // callback is valid
  DCHECK_LT(0, buf_len);                  // buf_len is valid

  if (!IOWatcher::WatchFileDescriptor(socket_fd_, IOWatcher::kWatchWrite, this,
                                      &write_socket_controller_)) {
    LOG(ERROR) << "WatchFileIO failed on write";
    return StatusOr<int>(MapSystemError(errno));
  }

  write_buf_ = buf;
  write_buf_len_ = buf_len;
  write_callback_ = std::move(callback);
  return StatusOr<int>(Status::TryAgain("WRITE PENDING"));
}

Status TCPClientImpl::Disconnect() {
  bool ok = connect_socket_controller_.StopWatching();
  DCHECK(ok);
  ok = read_socket_controller_.StopWatching();
  DCHECK(ok);
  ok = write_socket_controller_.StopWatching();
  DCHECK(ok);
  if (socket_fd_ != -1) {
    socket_fd_.reset();  // close socket
  }

  if (connect_callback_) {
    connect_callback_ = nullptr;
  }

  if (read_callback_) {
    read_buf_.reset();
    read_buf_len_ = 0;
    read_callback_ = nullptr;
  }

  read_if_ready_callback_ = nullptr;

  if (!write_callback_) {
    write_buf_.reset();
    write_buf_len_ = 0;
    write_callback_ = nullptr;
  }

  connect_state_ = kNone;
  local_address_.reset();
  remote_address_.reset();
  return Status();
}

bool TCPClientImpl::IsConnected() const {
  if (socket_fd_ == -1 || connect_state_ != kConnected) {
    return false;
  }

  // Check if connection is alive
  char c;
  StatusOr<int> ret = SocketOp::recv(socket_fd_, &c, sizeof(c), MSG_PEEK);
  if ((ret.ok() && ret.value() != sizeof(c)) || !ret.status().IsTryAgain()) {
    return false;
  }
  return true;
}

Status TCPClientImpl::GetLocalAddress(InetAddress* local) const {
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

Status TCPClientImpl::GetRemoteAddress(InetAddress* remote) const {
  DCHECK_NE(-1, socket_fd_);
  DCHECK(remote);
  if (connect_state_ != kConnected) {
    return Status::Corruption("Socket not connected");
  }

  *remote = remote_address_->ToInetAddress();
  return Status();
}

Status TCPClientImpl::SetKeepAlive(bool enable, int delay) {
  DCHECK_NE(-1, socket_fd_);

  Status status = SocketOp::set_keep_alive(socket_fd_, enable, delay);
  if (!status.ok()) {
    LOG(ERROR) << "Failed to set SO_KEEPALIVE on fd(" << socket_fd_ << "), "
               << status.ToString();
  }
  return status;
}

Status TCPClientImpl::SetNoDelay(bool on_delay) {
  DCHECK_NE(-1, socket_fd_);

  Status status = SocketOp::set_nodelay(socket_fd_, on_delay);
  if (!status.ok()) {
    LOG(ERROR) << "Failed to set TCP_NODELAY on fd(" << socket_fd_ << "), "
               << status.ToString();
  }
  return status;
}

Status TCPClientImpl::SetReceiveBufferSize(int size) {
  DCHECK_NE(-1, socket_fd_);

  Status status = SocketOp::set_receive_buffer_size(socket_fd_, size);
  if (!status.ok()) {
    LOG(ERROR) << "Failed to set SO_RCVBUF on fd(" << socket_fd_ << "), "
               << status.ToString();
  }
  return status;
}

Status TCPClientImpl::SetSendBufferSize(int size) {
  DCHECK_NE(-1, socket_fd_);

  Status status = SocketOp::set_send_buffer_size(socket_fd_, size);
  if (!status.ok()) {
    LOG(ERROR) << "Failed to set SO_RNDBUF on fd(" << socket_fd_ << "), "
               << status.ToString();
  }
  return status;
}

Status TCPClientImpl::DoConnect() {
  return SocketOp::connect(socket_fd_, remote_address_->addr,
                           remote_address_->addr_len);
}
StatusOr<int> TCPClientImpl::DoRead(IOBuffer* buf, int buf_len) {
  return FileOp::read(socket_fd_, buf->data(), buf_len);
}
StatusOr<int> TCPClientImpl::DoWrite(IOBuffer* buf, int buf_len) {
  return FileOp::write(socket_fd_, buf->data(), buf_len);
}

void TCPClientImpl::RetryRead(StatusOr<int> ret) {
  DCHECK(read_callback_);
  DCHECK(read_buf_);
  DCHECK_LT(0, read_buf_len_);

  if (ret.ok()) {
    ret = ReadIfReady(
        read_buf_.get(), read_buf_len_,
        std::bind(&TCPClientImpl::RetryRead, this, std::placeholders::_1));
    if (ret.status().IsTryAgain()) {
      return;
    }
  }

  read_buf_ = nullptr;
  read_buf_len_ = 0;
  TCPReadCb read_callback;
  std::swap(read_callback, read_callback_);
  read_callback(ret);
}

void TCPClientImpl::OnConnectDone() {
  // Get the error that connect() completed with.
  int os_error = 0;
  socklen_t len = sizeof(os_error);
  if (getsockopt(socket_fd_, SOL_SOCKET, SO_ERROR, &os_error, &len) == 0) {
    errno = os_error;
  }

  Status status = MapSocketConnectError(errno);
  if (status.IsTryAgain()) {
    return;
  }

  bool ok = connect_socket_controller_.StopWatching();
  DCHECK(ok);
  connect_state_ = kConnected;
  TCPConnectCb connect_callback(nullptr);
  std::swap(connect_callback, connect_callback_);
  connect_callback(status);
}

void TCPClientImpl::OnReadDone() {
  DCHECK(read_if_ready_callback_);

  bool ok = read_socket_controller_.StopWatching();
  DCHECK(ok);
  TCPReadCb read_if_ready_callback(nullptr);
  std::swap(read_if_ready_callback, read_if_ready_callback_);
  read_if_ready_callback(StatusOr<int>(0));
}

void TCPClientImpl::OnWriteDone() {
  StatusOr<int> ret =
      FileOp::write(socket_fd_, write_buf_.get(), write_buf_len_);
  if (ret.status().IsTryAgain()) {
    return;
  }

  bool ok = write_socket_controller_.StopWatching();
  DCHECK(ok);
  write_buf_.reset();
  write_buf_len_ = 0;
  TCPWriteCb write_callback(nullptr);
  std::swap(write_callback, write_callback_);
  write_callback(ret);
}

void TCPClientImpl::OnFileReadable(int fd) {
  DCHECK(read_if_ready_callback_);
  OnReadDone();
}

void TCPClientImpl::OnFileWritable(int fd) {
  DCHECK_NE(kNone, connect_state_);
  if (connect_state_ == kConnecting) {
    OnConnectDone();
  } else {
    OnWriteDone();
  }
}

}  // namespace iomgr

#include "io/tcp_client_impl.h"

#include <glog/logging.h>
#include <gtest/gtest.h>

#include "io/test/async_test_callback.h"
#include "iomgr/io_buffer.h"
#include "iomgr/tcp/inet_address.h"
#include "iomgr/tcp/tcp_server.h"

namespace iomgr {

const int kListenBacklog = 5;
const TCPServer::Options options(true, kListenBacklog);
const InetAddress local_host("127.0.0.1", 0);

class TCPClientImplTest : public testing::Test {
 protected:
  void SetUp() {
    Status listen_result =
        TCPServer::Listen(local_host, options, &server_socket_);
    DCHECK(server_socket_);
    EXPECT_TRUE(server_socket_->GetLocalAddress(&server_address_).ok());
  }

  void TearDown() {}

  void CreateConnectedSockets(std::unique_ptr<TCPClient>* accepted_socket,
                              std::unique_ptr<TCPClient>* client_socket,
                              const InetAddress* bind_address) {
    StatusResultCallback connect_callback;
    Status connect_result = TCPClient::Connect(
        server_address_, TCPClient::Options(), connect_callback.callback(),
        bind_address, client_socket);

    StatusResultCallback accept_callback;
    Status accept_result =
        server_socket_->Accept(accepted_socket, accept_callback.callback());
    accept_result = accept_callback.GetResult(accept_result);
    EXPECT_TRUE(accept_result.ok());

    EXPECT_TRUE(connect_callback.GetResult(connect_result).ok());

    EXPECT_TRUE((*client_socket)->IsConnected());
    EXPECT_TRUE((*accepted_socket)->IsConnected());
  }

  std::unique_ptr<TCPServer> server_socket_;
  InetAddress server_address_;
};

TEST_F(TCPClientImplTest, BindLoopback) {
  std::unique_ptr<TCPClient> accepted_socket;
  std::unique_ptr<TCPClient> connectint_sokcet;

  CreateConnectedSockets(&accepted_socket, &connectint_sokcet, &local_host);
  accepted_socket->Disconnect();
  connectint_sokcet->Disconnect();
  EXPECT_FALSE(accepted_socket->IsConnected());
  EXPECT_FALSE(connectint_sokcet->IsConnected());
}

TEST_F(TCPClientImplTest, AdoptConnectedSocket) {
  std::unique_ptr<TCPClient> accepted_socket;
  std::unique_ptr<TCPClient> connectint_sokcet;

  CreateConnectedSockets(&accepted_socket, &connectint_sokcet, &local_host);
  InetAddress accepted_address;
  EXPECT_TRUE(accepted_socket->GetLocalAddress(&accepted_address).ok());
  std::unique_ptr<TCPClientImpl> tmp(
      dynamic_cast<TCPClientImpl*>(accepted_socket.release()));
  DCHECK(tmp);
  std::unique_ptr<TCPClientImpl> socket_(new TCPClientImpl);
  EXPECT_TRUE(socket_
                  ->AdoptConnectedSocket(tmp->ReleaseSocketFdForTesting(),
                                         accepted_address)
                  .ok());

  InetAddress adopted_address;
  EXPECT_TRUE(socket_->GetLocalAddress(&adopted_address).ok());
  EXPECT_EQ(accepted_address, adopted_address);
}

TEST_F(TCPClientImplTest, ReadWrite) {
  std::unique_ptr<TCPClient> accepted_socket;
  std::unique_ptr<TCPClient> connectint_sokcet;

  CreateConnectedSockets(&accepted_socket, &connectint_sokcet, &local_host);

  const std::string byte = "a";

  RefPtr<StringIOBuffer> write_buffer = MakeRefCounted<StringIOBuffer>(byte);
  StatusOrResultCallback write_callback;
  StatusOr<int> write_result = accepted_socket->Write(
      write_buffer.get(), write_buffer->size(), write_callback.callback());
  write_result = write_callback.GetResult(write_result);
  EXPECT_TRUE(write_result.ok());
  EXPECT_EQ(byte.size(), write_result.value());

  RefPtr<IOBufferWithSize> read_buffer =
      MakeRefCounted<IOBufferWithSize>(byte.size());
  StatusOrResultCallback read_callback;
  StatusOr<int> read_result = connectint_sokcet->Read(
      read_buffer.get(), read_buffer->size(), read_callback.callback());
  read_result = read_callback.GetResult(read_result);
  EXPECT_TRUE(read_result.ok());
  EXPECT_EQ(byte.size(), read_result.value());

  const char received = *read_buffer->data();
  EXPECT_EQ(byte.front(), received);
}

TEST_F(TCPClientImplTest, MultiRead) {
  std::unique_ptr<TCPClient> accepted_socket;
  std::unique_ptr<TCPClient> connectint_sokcet;

  CreateConnectedSockets(&accepted_socket, &connectint_sokcet, &local_host);

  const std::string message = "test message";
  std::vector<char> buffer(message.size());

  RefPtr<StringIOBuffer> write_buffer = MakeRefCounted<StringIOBuffer>(message);
  StatusOrResultCallback write_callback;
  StatusOr<int> write_result = accepted_socket->Write(
      write_buffer.get(), write_buffer->size(), write_callback.callback());
  write_result = write_callback.GetResult(write_result);
  CHECK(write_result.ok());

  size_t bytes_read = 0;
  while (bytes_read < write_result.value()) {
    RefPtr<IOBufferWithSize> read_buffer = MakeRefCounted<IOBufferWithSize>(1);
    StatusOrResultCallback read_callback;
    StatusOr<int> read_result = connectint_sokcet->Read(
        read_buffer.get(), read_buffer->size(), read_callback.callback());
    read_result = read_callback.GetResult(read_result);
    EXPECT_TRUE(read_result.ok());
    EXPECT_TRUE(read_result.value() >= 0);
    memmove(&buffer[bytes_read], read_buffer->data(), read_result.value());
    bytes_read += read_result.value();
  }

  std::string received_message(buffer.begin(), buffer.end());
  EXPECT_EQ(message, received_message);
}

TEST_F(TCPClientImplTest, MultiWrite) {
  std::unique_ptr<TCPClient> accepted_socket;
  std::unique_ptr<TCPClient> connectint_sokcet;

  CreateConnectedSockets(&accepted_socket, &connectint_sokcet, &local_host);

  const std::string message("test message");
  std::vector<char> buffer(message.size());

  size_t bytes_written = 0;
  while (bytes_written < message.size()) {
    RefPtr<IOBufferWithSize> write_buffer = MakeRefCounted<IOBufferWithSize>(1);
    memmove(write_buffer->data(), message.data() + bytes_written, 1);
    StatusOrResultCallback write_callback;
    StatusOr<int> write_result = accepted_socket->Write(
        write_buffer.get(), write_buffer->size(), write_callback.callback());
    write_result = write_callback.GetResult(write_result);
    EXPECT_TRUE(write_result.ok());
    EXPECT_TRUE(write_result.value() >= 0);
    bytes_written += write_result.value();
  }

  size_t bytes_read = 0;
  while (bytes_read < message.size()) {
    RefPtr<IOBufferWithSize> read_buffer =
        MakeRefCounted<IOBufferWithSize>(message.size() - bytes_read);
    StatusOrResultCallback read_callback;
    StatusOr<int> read_result = connectint_sokcet->Read(
        read_buffer.get(), read_buffer->size(), read_callback.callback());
    read_result = read_callback.GetResult(read_result);
    EXPECT_TRUE(read_result.ok());
    EXPECT_TRUE(read_result.value() >= 0);
    memmove(&buffer[bytes_read], read_buffer->data(), read_result.value());
    bytes_read += read_result.value();
  }

  std::string received_message(buffer.begin(), buffer.end());
  EXPECT_EQ(message, received_message);
}

TEST_F(TCPClientImplTest, ReadIfReady) {
  std::unique_ptr<TCPClient> accepted_socket;
  std::unique_ptr<TCPClient> connectint_sokcet;

  CreateConnectedSockets(&accepted_socket, &connectint_sokcet, &local_host);

  const std::string message("test message");
  std::vector<char> buffer(message.size());

  // try to read from the socket, but never write anything to the other end.
  RefPtr<IOBufferWithSize> read_buffer =
      MakeRefCounted<IOBufferWithSize>(message.size());
  StatusResultCallback read_callback;
  StatusOr<int> read_result = connectint_sokcet->ReadIfReady(
      read_buffer.get(), read_buffer->size(), read_callback.callback());
  EXPECT_TRUE(read_result.status().IsTryAgain());
  // connectint_sokcet->CancelReadIfReady();

  // Send data to socket
  size_t bytes_written = 0;
  while (bytes_written < message.size()) {
    RefPtr<IOBufferWithSize> write_buffer =
        MakeRefCounted<IOBufferWithSize>(message.size() - bytes_written);
    memmove(write_buffer->data(), message.data() + bytes_written,
            message.size() - bytes_written);

    StatusOrResultCallback write_callback;
    StatusOr<int> write_result = accepted_socket->Write(
        write_buffer.get(), write_buffer->size(), write_callback.callback());
    write_result = write_callback.GetResult(write_result);
    EXPECT_TRUE(write_result.ok());
    EXPECT_TRUE(write_result.value() >= 0);
    bytes_written += write_result.value();
  }

  EXPECT_TRUE(read_callback.GetResult(Status::TryAgain("")).ok());

  read_result = connectint_sokcet->ReadIfReady(
      read_buffer.get(), read_buffer->size(), read_callback.callback());
  EXPECT_TRUE(read_result.ok());
  std::string received_message(read_buffer->data(), read_result.value());
  EXPECT_EQ(message, received_message);
}

class IOBufferWithDestructionCheck : public IOBufferWithSize {
 public:
  IOBufferWithDestructionCheck() : IOBufferWithSize(1024) {}

  static bool dtor_called() { return dtor_called_; }
  static void reset() { dtor_called_ = false; }

 protected:
  ~IOBufferWithDestructionCheck() override { dtor_called_ = true; }

  static bool dtor_called_;
};

bool IOBufferWithDestructionCheck::dtor_called_;

TEST_F(TCPClientImplTest, DestroyWithPendingRead) {
  {
    std::unique_ptr<TCPClient> accepted_socket;
    std::unique_ptr<TCPClient> connectint_sokcet;

    CreateConnectedSockets(&accepted_socket, &connectint_sokcet, &local_host);

    IOBufferWithDestructionCheck::reset();
    RefPtr<IOBufferWithDestructionCheck> read_buffer =
        MakeRefCounted<IOBufferWithDestructionCheck>();
    StatusOrResultCallback read_callback;
    StatusOr<int> read_result = connectint_sokcet->Read(
        read_buffer.get(), read_buffer->size(), read_callback.callback());
    EXPECT_FALSE(read_result.ok());
    EXPECT_TRUE(read_result.status().IsTryAgain());
  }
  EXPECT_TRUE(IOBufferWithDestructionCheck::dtor_called());
}

TEST_F(TCPClientImplTest, DestroyWithPendingWrite) {
  {
    std::unique_ptr<TCPClient> accepted_socket;
    std::unique_ptr<TCPClient> connectint_sokcet;

    CreateConnectedSockets(&accepted_socket, &connectint_sokcet, &local_host);

    // Repeatedly write to the socket until an operation does not complete
    // synchronously.
    IOBufferWithDestructionCheck::reset();
    RefPtr<IOBufferWithDestructionCheck> write_buffer =
        MakeRefCounted<IOBufferWithDestructionCheck>();
    memset(write_buffer->data(), '1', write_buffer->size());
    StatusOrResultCallback write_callback;
    while (true) {
      StatusOr<int> write_result = connectint_sokcet->Write(
          write_buffer.get(), write_buffer->size(), write_callback.callback());
      if (!write_result.ok()) {
        EXPECT_TRUE(write_result.status().IsTryAgain());
        break;
      }
    }
  }
  EXPECT_TRUE(IOBufferWithDestructionCheck::dtor_called());
}

TEST_F(TCPClientImplTest, CancelPendingReadIfReady) {
  {
    std::unique_ptr<TCPClient> accepted_socket;
    std::unique_ptr<TCPClient> connectint_sokcet;

    CreateConnectedSockets(&accepted_socket, &connectint_sokcet, &local_host);

    IOBufferWithDestructionCheck::reset();
    RefPtr<IOBufferWithDestructionCheck> read_buffer =
        MakeRefCounted<IOBufferWithDestructionCheck>();
    StatusOrResultCallback read_callback;
    StatusOr<int> read_result = connectint_sokcet->ReadIfReady(
        read_buffer.get(), read_buffer->size(), read_callback.callback());
    EXPECT_FALSE(read_result.ok());
    EXPECT_TRUE(read_result.status().IsTryAgain());

    connectint_sokcet->CancelReadIfReady();
  }
  EXPECT_TRUE(IOBufferWithDestructionCheck::dtor_called());
}

TEST_F(TCPClientImplTest, IsConnected) {
  std::unique_ptr<TCPClient> connectint_sokcet;
  StatusResultCallback connect_callback;
  Status connect_result = TCPClient::Connect(
      server_address_, TCPClient::Options(), connect_callback.callback(),
      &local_host, &connectint_sokcet);
  EXPECT_FALSE(connectint_sokcet->IsConnected());

  std::unique_ptr<TCPClient> accepted_socket;
  StatusResultCallback accept_callback;
  Status accept_result =
      server_socket_->Accept(&accepted_socket, accept_callback.callback());
  accept_result = accept_callback.GetResult(accept_result);
  EXPECT_TRUE(accept_result.ok());
  EXPECT_TRUE(accepted_socket->IsConnected());

  EXPECT_TRUE(connect_callback.GetResult(connect_result).ok());
  EXPECT_TRUE(connectint_sokcet->IsConnected());
}

TEST_F(TCPClientImplTest, DisConnect) {
  std::unique_ptr<TCPClient> accepted_socket;
  std::unique_ptr<TCPClient> connectint_sokcet;

  CreateConnectedSockets(&accepted_socket, &connectint_sokcet, &local_host);

  const std::string byte = "abcdef";

  RefPtr<StringIOBuffer> write_buffer = MakeRefCounted<StringIOBuffer>(byte);
  StatusOrResultCallback write_callback;
  StatusOr<int> write_result = accepted_socket->Write(
      write_buffer.get(), write_buffer->size(), write_callback.callback());
  write_result = write_callback.GetResult(write_result);
  EXPECT_TRUE(write_result.ok());
  EXPECT_EQ(byte.size(), write_result.value());

  // Disconnect
  accepted_socket->Disconnect();

  RefPtr<IOBufferWithSize> read_buffer =
      MakeRefCounted<IOBufferWithSize>(byte.size());
  StatusOrResultCallback read_callback;
  StatusOr<int> read_result = connectint_sokcet->Read(
      read_buffer.get(), read_buffer->size(), read_callback.callback());
  read_result = read_callback.GetResult(read_result);
  EXPECT_TRUE(read_result.ok());
  EXPECT_EQ(byte.size(), read_result.value());

  const char received = *read_buffer->data();
  EXPECT_EQ(byte.front(), received);

  // Read again will return EOF
  read_result = connectint_sokcet->Read(read_buffer.get(), read_buffer->size(),
                                        read_callback.callback());
  read_result = read_callback.GetResult(read_result);
  EXPECT_TRUE(read_result.ok());
  EXPECT_EQ(0, read_result.value());
}

}  // namespace iomgr

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
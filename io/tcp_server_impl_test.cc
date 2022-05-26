#include "io/tcp_server_impl.h"

#include <glog/logging.h>
#include <gtest/gtest.h>

#include "io/test/async_test_callback.h"
#include "iomgr/io_buffer.h"
#include "iomgr/ref_counted.h"
#include "iomgr/tcp/tcp_client.h"
#include "iomgr/tcp/tcp_server.h"

namespace iomgr {

const int kListenBacklog = 5;
const TCPServer::Options options(true, kListenBacklog);
const InetAddress local_host("127.0.0.1", 0);

class TCPServerImplTest : public testing::Test {
 protected:
  void SetUp() {
    EXPECT_TRUE(TCPServer::Listen(local_host, options, &socket_).ok());
    DCHECK(socket_);
    EXPECT_TRUE(socket_->GetLocalAddress(&server_address_).ok());
  }

  void TearDown() override {}

  static InetAddress GetRemoteAddress(TCPClient* socket) {
    InetAddress remote;
    EXPECT_TRUE(socket->GetRemoteAddress(&remote).ok());
    return remote;
  }

  std::unique_ptr<TCPServer> socket_;
  InetAddress server_address_;
};

TEST_F(TCPServerImplTest, Accept) {
  StatusResultCallback connect_callback;
  std::unique_ptr<TCPClient> connecting_socket;
  Status connect_result = TCPClient::Connect(
      server_address_, TCPClient::Options(), connect_callback.callback(),
      nullptr, &connecting_socket);

  StatusResultCallback accept_callback;
  std::unique_ptr<TCPClient> accepted_socket;
  InetAddress accepted_address;
  Status accept_result = socket_->Accept(
      &accepted_socket, accept_callback.callback(), &accepted_address);

  EXPECT_TRUE(accept_callback.GetResult(accept_result).ok());
  EXPECT_TRUE(accepted_socket);
  EXPECT_EQ(accepted_address.ip(), local_host.ip());
  InetAddress client = GetRemoteAddress(accepted_socket.get());
  EXPECT_EQ(client, accepted_address);
  EXPECT_TRUE(connect_callback.GetResult(connect_result).ok());
}

// Test Accept() callback
TEST_F(TCPServerImplTest, AcceptAsync) {
  StatusResultCallback accept_callback;
  std::unique_ptr<TCPClient> accepted_socket;
  InetAddress accepted_address;

  Status status = socket_->Accept(&accepted_socket, accept_callback.callback(),
                                  &accepted_address);
  EXPECT_TRUE(status.IsTryAgain());

  StatusResultCallback connect_callback;
  std::unique_ptr<TCPClient> connecting_socket;
  Status connect_result = TCPClient::Connect(
      server_address_, TCPClient::Options(), connect_callback.callback(),
      nullptr, &connecting_socket);
  EXPECT_TRUE(connect_callback.GetResult(connect_result).ok());

  EXPECT_TRUE(accept_callback.WaitForResult().ok());

  EXPECT_TRUE(accepted_socket);

  EXPECT_EQ(accepted_address.ip(), local_host.ip());
  EXPECT_EQ(GetRemoteAddress(accepted_socket.get()).ip(), local_host.ip());
}

// Test Accept() when client disconnects right after trying to connect
TEST_F(TCPServerImplTest, AcceptClientDisconnectAfterConnect) {
  StatusResultCallback connect_callback;
  std::unique_ptr<TCPClient> connecting_socket;
  Status connect_result = TCPClient::Connect(
      server_address_, TCPClient::Options(), connect_callback.callback(),
      nullptr, &connecting_socket);

  StatusResultCallback accept_callback;
  std::unique_ptr<TCPClient> accepted_socket;
  InetAddress accepted_address;
  Status accept_result = socket_->Accept(
      &accepted_socket, accept_callback.callback(), &accepted_address);

  connecting_socket->Disconnect();

  EXPECT_TRUE(accept_callback.GetResult(accept_result).ok());
  // EXPECT_TRUE(connect_callback.GetResult(connect_result).ok());
  EXPECT_TRUE(accepted_socket);
  EXPECT_EQ(accepted_address.ip(), local_host.ip());
}

// Accept two connections simultaneously
TEST_F(TCPServerImplTest, Accept2Connection) {
  StatusResultCallback accept_callback;
  std::unique_ptr<TCPClient> accepted_socket;
  InetAddress accepted_address;

  Status accept_result = socket_->Accept(
      &accepted_socket, accept_callback.callback(), &accepted_address);
  EXPECT_TRUE(accept_result.IsTryAgain());

  StatusResultCallback connect_callback;
  std::unique_ptr<TCPClient> connecting_socket;
  Status connect_result = TCPClient::Connect(
      server_address_, TCPClient::Options(), connect_callback.callback(),
      nullptr, &connecting_socket);

  StatusResultCallback connect_callback2;
  std::unique_ptr<TCPClient> connecting_socket2;
  Status connect_result2 = TCPClient::Connect(
      server_address_, TCPClient::Options(), connect_callback2.callback(),
      nullptr, &connecting_socket2);

  EXPECT_TRUE(accept_callback.WaitForResult().ok());

  StatusResultCallback accept_calback2;
  std::unique_ptr<TCPClient> accepted_socket2;
  InetAddress accepted_address2;
  Status accept_result2 = socket_->Accept(
      &accepted_socket2, accept_calback2.callback(), &accepted_address2);
  EXPECT_TRUE(accept_calback2.GetResult(accept_result2).ok());
  EXPECT_TRUE(accept_result2.ok());

  EXPECT_TRUE(connect_callback.GetResult(connect_result).ok());
  EXPECT_TRUE(connect_callback2.GetResult(connect_result2).ok());

  EXPECT_TRUE(accepted_socket);
  EXPECT_TRUE(accepted_socket2);
  EXPECT_NE(accepted_socket.get(), accepted_socket2.get());

  EXPECT_EQ(accepted_address.ip(), local_host.ip());
  EXPECT_EQ(GetRemoteAddress(accepted_socket.get()).ip(), local_host.ip());
  EXPECT_EQ(accepted_address.ip(), local_host.ip());
  EXPECT_EQ(GetRemoteAddress(accepted_socket2.get()).ip(), local_host.ip());
}

TEST_F(TCPServerImplTest, AcceptIO) {
  StatusResultCallback connect_callback;
  std::unique_ptr<TCPClient> connecting_socket;
  Status connect_result = TCPClient::Connect(
      server_address_, TCPClient::Options(), connect_callback.callback(),
      nullptr, &connecting_socket);

  StatusResultCallback accept_callback;
  std::unique_ptr<TCPClient> accepted_socket;
  InetAddress accepted_address;
  Status accept_result = socket_->Accept(
      &accepted_socket, accept_callback.callback(), &accepted_address);

  EXPECT_TRUE(accept_callback.GetResult(accept_result).ok());
  EXPECT_TRUE(accepted_socket);
  EXPECT_EQ(accepted_address.ip(), local_host.ip());
  InetAddress client = GetRemoteAddress(accepted_socket.get());
  EXPECT_EQ(client, accepted_address);
  EXPECT_TRUE(connect_callback.GetResult(connect_result).ok());

  const std::string message("test msg");
  std::vector<char> buffer(message.size());

  size_t bytes_written = 0;
  while (bytes_written < message.size()) {
    RefPtr<IOBufferWithSize> write_buffer =
        MakeRefCounted<IOBufferWithSize>(message.size() - bytes_written);
    memmove(write_buffer->data(), message.data(), message.size());

    StatusOrResultCallback write_callback;
    StatusOr<int> write_result = accepted_socket->Write(
        write_buffer.get(), write_buffer->size(), write_callback.callback());
    write_result = write_callback.GetResult(write_result);
    EXPECT_GE(write_result.value(), 0);
    EXPECT_LE(bytes_written + write_result.value(), message.size());
    bytes_written += write_result.value();
  }

  size_t bytes_read = 0;
  while (bytes_read < message.size()) {
    RefPtr<IOBufferWithSize> read_buffer =
        MakeRefCounted<IOBufferWithSize>(message.size() - bytes_read);
    StatusOrResultCallback read_callback;
    StatusOr<int> read_result = connecting_socket->Read(
        read_buffer.get(), read_buffer->size(), read_callback.callback());
    read_result = read_callback.GetResult(read_result);
    EXPECT_GE(read_result.value(), 0);
    EXPECT_LE(bytes_read + read_result.value(), message.size());
    memmove(&buffer[bytes_read], read_buffer->data(), read_result.value());
    bytes_read += read_result.value();
  }

  std::string received_message(buffer.begin(), buffer.end());
  EXPECT_EQ(message, received_message);
}

}  // namespace iomgr

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
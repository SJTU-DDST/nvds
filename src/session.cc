#include "session.h"

namespace nvds {

void Session::Start() {
  AsyncRecvMessage(std::make_shared<Message>());
}

void Session::AsyncRecvMessage(std::shared_ptr<Message> msg) {
  // Avoid out-of-range of this session
  auto self = shared_from_this();
  auto body_handler = [this, self, msg](
      const boost::system::error_code& err,
      size_t bytes_transferred) {
    if (!err) {
      recv_msg_handler_(self, msg);
    } else {
      NVDS_ERR(err.message().c_str());
    }
  };
  auto header_handler = [this, self, msg, body_handler](
      const boost::system::error_code& err,
      size_t bytes_transferred) {
    if (!err) {
      assert(bytes_transferred >= Message::kHeaderSize);

      // Read message body
      size_t len = Message::kHeaderSize + msg->body_len() - bytes_transferred;
      msg->body().resize(len);
      boost::asio::async_read(conn_sock_,
          boost::asio::buffer(&msg->body()[0], msg->body_len()),
          body_handler);
    } else {
      NVDS_ERR(err.message().c_str());
    }
  };
  
  // Read message header
  boost::asio::async_read(conn_sock_,
      boost::asio::buffer(&msg->header(), Message::kHeaderSize),
      header_handler);
}

void Session::AsyncSendMessage(std::shared_ptr<Message> msg) {
  // Avoid out-of-range of this session
  auto self = shared_from_this();
  auto body_handler = [this, self, msg](
      const boost::system::error_code& err,
      size_t bytes_transferred) {
    if (!err) {
      send_msg_handler_(self, msg);
    } else {
      NVDS_ERR(err.message().c_str());
    }
  };
  auto header_handler = [this, self, msg, body_handler](
      const boost::system::error_code& err,
      size_t bytes_transferred) {
    if (!err) {
      assert(bytes_transferred >= Message::kHeaderSize);

      // Send message body
      boost::asio::async_write(conn_sock_,
          boost::asio::buffer(msg->body()), body_handler);
    } else {
      NVDS_ERR(err.message().c_str());
    }
  };
  
  // Read message header
  boost::asio::async_write(conn_sock_,
      boost::asio::buffer(&msg->header(), Message::kHeaderSize),
      header_handler);
}

void Session::SendMessage(const Message& msg) {
  boost::asio::write(conn_sock_,
      boost::asio::buffer(&msg.header(), Message::kHeaderSize));
  boost::asio::write(conn_sock_, boost::asio::buffer(msg.body()));
}

Message Session::RecvMessage() {
  Message msg;
  boost::asio::read(conn_sock_,
      boost::asio::buffer(&msg.header(), Message::kHeaderSize));
  msg.body().resize(msg.body_len());
  boost::asio::read(conn_sock_,
      boost::asio::buffer(&msg.body()[0], msg.body_len()));
  return msg;
}

} // namespace nvds

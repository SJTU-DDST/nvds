#include "session.h"

namespace nvds {

void Session::Start() {
  RecvMessage();
}

void Session::RecvMessage() {
  // Avoid out-of-range of this session
  auto self = shared_from_this();
  auto body_handler = [this, self](
      const boost::system::error_code& err,
      size_t bytes_transferred) {
    if (!err) {
      // TODO(wgtdkp): handle this message
      msg_handler_(*this);
    } else {
      NVDS_ERR(err.message().c_str());
    }
  };
  auto header_handler = [this, self, body_handler](
      const boost::system::error_code& err,
      size_t bytes_transferred) {
    if (!err) {
      assert(bytes_transferred >= Message::kHeaderSize);
      
      auto header = *reinterpret_cast<Message::Header*>(raw_data_);
      // Read message body
      size_t len = Message::kHeaderSize + header.body_len - bytes_transferred;
      assert(len + bytes_transferred <= kRawDataSize);
      boost::asio::async_read(conn_sock_,
          boost::asio::buffer(raw_data_ + bytes_transferred, len),
          body_handler);
    } else {
      NVDS_ERR(err.message().c_str());
    }
  };
  
  // Read message header
  size_t len = Message::kHeaderSize;
  assert(len <= kRawDataSize);
  boost::asio::async_read(conn_sock_,
      boost::asio::buffer(raw_data_, len), header_handler);
}

void Session::SendMessage() {

}

} // namespace nvds

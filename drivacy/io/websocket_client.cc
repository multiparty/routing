// Copyright 2020 multiparty.org

// The real client-side web-socket interface.
//
// This is only used when running an independent (not a local simulation)
// c++ client, which will use this file to communicate with the
// first frontend server via websockets.

#include "drivacy/io/websocket_client.h"

#include "absl/functional/bind_front.h"
#include "absl/strings/str_format.h"

namespace drivacy {
namespace io {
namespace socket {

WebSocketClient::WebSocketClient(uint32_t party_id, uint32_t machine_id,
                                 const types::Configuration &config,
                                 SocketListener *listener)
    : AbstractSocket(party_id, machine_id, config, listener) {
  this->queries_sent_count_ = 0;

  // Find address of first frontend.
  const auto &network_config = config.network().at(1).machines().at(machine_id);
  uint32_t port = network_config.webserver_port();
  const std::string &ip = network_config.ip();
  std::string address = absl::StrFormat("ws://%s:%d", ip, port);

  // Create socket.
  this->socket_ = easywsclient::WebSocket::from_url(address);
  assert(this->socket_);
}

void WebSocketClient::Listen() {
  // Block until a response is heard.
  while (this->queries_sent_count_ > 0 &&
         this->socket_->getReadyState() != easywsclient::WebSocket::CLOSED) {
    this->socket_->poll(-1);
    this->socket_->dispatchBinary(
        absl::bind_front(&WebSocketClient::HandleResponse, this));
  }
}

void WebSocketClient::SendQuery(const types::ForwardQuery &query) {
  this->queries_sent_count_++;
  const unsigned char *buffer = query;
  std::string msg(reinterpret_cast<const char *>(buffer),
                  this->outgoing_query_msg_size_);
  this->socket_->sendBinary(msg);
  this->socket_->poll();
}

void WebSocketClient::HandleResponse(const std::vector<uint8_t> &msg) {
  this->queries_sent_count_--;
  const unsigned char *buffer = &(msg[0]);
  this->listener_->OnReceiveResponse(buffer);
}

}  // namespace socket
}  // namespace io
}  // namespace drivacy

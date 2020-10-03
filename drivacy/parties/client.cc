// Copyright 2020 multiparty.org

// This file defines the "Client" class, which represents
// a single client in our protocol. The client is responsible for creating
// queries and handling responses.
//
// When deployed, this client communicates with the appropriate (server) parties
// on the wire. In a simulated enviornment, the client and parties are run
// locally in the same process, and they interact directly via the enclosing
// simulation code.

#include "drivacy/parties/client.h"

#include "drivacy/protocol/client.h"

namespace drivacy {
namespace parties {

namespace {
// Useless function that should never be called
void Unused(const types::IncomingQuery &_) { assert(false); }
}  // namespace

Client::Client(const types::Configuration &config,
               io::socket::SocketFactory socket_factory)
    : config_(config) {
  this->socket_ = socket_factory(
      0, Unused, absl::bind_front(&Client::OnReceiveResponse, this), config);
}

void Client::MakeQuery(uint64_t value) {
  // Create query via client protocol.
  types::OutgoingQuery query =
      protocol::client::CreateQuery(value, this->config_);
  // Store state to use for when reconstructing corresponding response.
  this->state_.queries.push_back(value);
  this->state_.preshares.push_back(query.query_state());
  // Send via socket.
  this->socket_->SendQuery(query);
}

void Client::OnReceiveResponse(const types::Response &response) {
  // Retrieve stored state for this response.
  uint64_t query = this->state_.queries.front();
  uint64_t preshare = this->state_.preshares.front();
  // Remove the stored state belonging to this response.
  this->state_.queries.pop_front();
  this->state_.preshares.pop_front();
  // Reconstruct response using client protocol.
  uint64_t response_value =
      protocol::client::ReconstructResponse(response, preshare);
  this->response_handler_(query, response_value);
}

}  // namespace parties
}  // namespace drivacy
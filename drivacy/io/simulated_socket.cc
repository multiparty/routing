// Copyright 2020 multiparty.org

// A (fake/simulated) socket-like interface for simulating over the wire
// communication, when running all the parties locally for testing/debugging.
//
// The parties are all run in the same process/thread locally. They are merely
// logical entities. They communicate via this file's API, which is identical
// to the API of the real over-the-wire socket API.

#include "drivacy/io/simulated_socket.h"

namespace drivacy {
namespace io {
namespace socket {

// SimulatedSocket.
SimulatedSocket::SimulatedSocket(uint32_t party_id,
                                 QueryListener query_listener,
                                 ResponseListener response_listener)
    : party_id_(party_id),
      query_listener_(query_listener),
      response_listener_(response_listener) {
  assert(SimulatedSocket::sockets_.count(party_id) == 0);
  SimulatedSocket::sockets_.insert({party_id, this});
}

SimulatedSocket::~SimulatedSocket() {
  SimulatedSocket::sockets_.erase(this->party_id_);
}

void SimulatedSocket::SendQuery(uint32_t party,
                                const drivacy::types::Query &query) const {
  SimulatedSocket *socket = SimulatedSocket::sockets_.at(party);
  socket->query_listener_(this->party_id_, query);
}

void SimulatedSocket::SendResponse(
    uint32_t party, const drivacy::types::Response &response) const {
  SimulatedSocket *socket = SimulatedSocket::sockets_.at(party);
  socket->response_listener_(this->party_id_, response);
}

std::unordered_map<uint32_t, SimulatedSocket *> SimulatedSocket::sockets_ = {};

// SimulatedClientSocket.
SimulatedClientSocket::SimulatedClientSocket(uint32_t party_id,
                                             QueryListener query_listener,
                                             ResponseListener response_listener)
    : party_id_(party_id),
      query_listener_(query_listener),
      response_listener_(response_listener) {
  assert(SimulatedClientSocket::sockets_.count(party_id) == 0);
  SimulatedClientSocket::sockets_.insert({party_id, this});
}

SimulatedClientSocket::~SimulatedClientSocket() {
  SimulatedClientSocket::sockets_.erase(this->party_id_);
}

void SimulatedClientSocket::SendQuery(
    uint32_t party, const drivacy::types::Query &query) const {
  assert(this->party_id_ == 0);
  SimulatedClientSocket *socket = SimulatedClientSocket::sockets_.at(party);
  socket->query_listener_(this->party_id_, query);
}

void SimulatedClientSocket::SendResponse(
    uint32_t party, const drivacy::types::Response &response) const {
  assert(this->party_id_ > 0);
  SimulatedClientSocket *socket = SimulatedClientSocket::sockets_.at(0);
  socket->response_listener_(this->party_id_, response);
}

std::unordered_map<uint32_t, SimulatedClientSocket *>
    SimulatedClientSocket::sockets_ = {};

}  // namespace socket
}  // namespace io
}  // namespace drivacy

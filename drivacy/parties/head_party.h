// Copyright 2020 multiparty.org

// This file defines the "HeadParty" class. A specialized Party that interfaces
// with clients.
//
// All comments about deploying/running parties in simulation or deployment
// in drivacy/parties/party.h apply here too.

#ifndef DRIVACY_PARTIES_HEAD_PARTY_H_
#define DRIVACY_PARTIES_HEAD_PARTY_H_

#include <cstdint>
#include <memory>

#include "drivacy/io/abstract_socket.h"
#include "drivacy/parties/party.h"
#include "drivacy/types/config.pb.h"
#include "drivacy/types/types.h"

namespace drivacy {
namespace parties {

// HeadParty is the special first party, which is responsible for communicating
// with other parties as well as clients!
class HeadParty : public Party {
 public:
  HeadParty(uint32_t party, const types::Configuration &config,
            const types::Table &table, io::socket::SocketFactory socket_factory,
            io::socket::SocketFactory client_socket_factory,
            uint32_t batch_size)
      : Party(party, config, table, socket_factory) {
    this->client_socket_ =
        client_socket_factory(this->party_id_, this->config_, this);
    this->OnReceiveBatch(batch_size);
  }

  // Not copyable or movable!
  HeadParty(HeadParty &&other) = delete;
  HeadParty &operator=(HeadParty &&other) = delete;
  HeadParty(const HeadParty &) = delete;
  HeadParty &operator=(const HeadParty &) = delete;

  // Start listening on the sockets (blocking!)
  void Listen() override;

  // Queries are sent as usual, but after they are sent, we block the client
  // socket and start listening for responses.
  void SendQueries() override;

  // Send responses via the client socket instead of the default socket!
  void SendResponses() override;

 protected:
  std::unique_ptr<io::socket::AbstractSocket> client_socket_;
};

}  // namespace parties
}  // namespace drivacy

#endif  // DRIVACY_PARTIES_HEAD_PARTY_H_

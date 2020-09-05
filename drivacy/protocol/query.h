// Copyright 2020 multiparty.org

// This file contains the query phase of the protocol.

#ifndef DRIVACY_PROTOCOL_QUERY_H_
#define DRIVACY_PROTOCOL_QUERY_H_

#include "drivacy/types/config.pb.h"
#include "drivacy/types/messages.pb.h"
#include "drivacy/types/types.h"

namespace drivacy {
namespace protocol {
namespace query {

uint32_t QuerySize(uint32_t party_id, uint32_t party_count);

types::Query ProcessQuery(const types::Query &query,
                          const types::Configuration &config,
                          types::PartyState *state);

}  // namespace query
}  // namespace protocol
}  // namespace drivacy

#endif  // DRIVACY_PROTOCOL_QUERY_H_

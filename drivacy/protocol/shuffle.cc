// Copyright 2020 multiparty.org

// This file implements an efficient shuffling functionality
// based on knuth shuffle.
// The shuffler is incremental: it shuffles the queries as they come.
// The shuffler remembers the generated order, which can be used to
// de-shuffle responses as they come.

#include "drivacy/protocol/shuffle.h"

#include <algorithm>

namespace drivacy {
namespace protocol {

// Constructor.
Shuffler::Shuffler(uint32_t party_id, uint32_t machine_id, uint32_t party_count,
                   uint32_t parallelism)
    : party_id_(party_id), machine_id_(machine_id), parallelism_(parallelism) {
  // Message sizes.
  this->forward_query_size_ = types::ForwardQuerySize(party_id, party_count);
  this->forward_response_size_ = types::ForwardResponseSize();
  // Seed RNG.
  // TODO(babman): use proper seed.
  this->generator_ = primitives::util::SeedGenerator(party_id);
}

// Initialization for a batch.
void Shuffler::Initialize(uint32_t size) {
  // Reset indices.
  this->query_index_ = 0;
  this->response_index_ = 0;

  // Store batch size.
  this->size_ = size;
  this->total_size_ = size * this->parallelism_;
  this->shuffled_query_count_ = 0;
  this->deshuffled_response_count_ = 0;

  // Resize vectors.
  this->shuffled_queries_.resize(size);
  this->deshuffled_responses_.resize(size, types::Response(0));

  // clear maps.
  this->query_machine_ids_.clear();
  this->response_machine_ids_.clear();
  this->query_order_.clear();
  this->query_indices_.clear();
  this->response_indices_.clear();
  this->query_states_.clear();

  // Reinitialize maps with pairs.
  for (uint32_t m = 1; m <= this->parallelism_; m++) {
    this->query_indices_[m].first = 0;
    this->response_indices_[m].first = 0;
    this->query_states_[m].first = 0;
  }

  // Knuth shuffling.
  std::unordered_map<uint32_t, uint32_t> shuffling_order;
  std::vector<std::pair<uint32_t, uint32_t>> received(this->size_);
  std::vector<std::pair<uint32_t, uint32_t>> sent(this->size_);
  for (uint32_t i = 0; i < this->total_size_; i++) {
    uint32_t true_i = i;
    if (shuffling_order.count(i) == 1) {
      true_i = shuffling_order.at(i);
    }
    // Query at global index j goes to global index i after shuffling.
    uint32_t j = i;
    if (i < this->total_size_ - 1) {
      j = primitives::util::Rand32(this->generator_, i, this->total_size_);
    }
    uint32_t true_j = j;
    if (shuffling_order.count(j) == 1) {
      true_j = shuffling_order.at(j);
    }

    // Who does i and j belong to?
    uint32_t receiver = i / this->size_ + 1;
    uint32_t sender = true_j / this->size_ + 1;
    shuffling_order[j] = true_i;
    shuffling_order.erase(i);

    // Case 1: i is in this machine's bucket.
    //         this machine will receive j from its owner.
    if (receiver == this->machine_id_) {
      this->response_machine_ids_.push_back(sender);
      uint32_t local_index = i % this->size_;
      auto &[previous, target] = received[local_index];
      previous = true_j;
      target = local_index;
    }
    // Case 2: j is in this machine's bucket.
    //         this machine will send j to machine_id_i.
    if (sender == this->machine_id_) {
      uint32_t local_index = true_j % this->size_;
      auto &[target, previous] = sent[local_index];
      target = i;
      previous = local_index;
    }
    // Case 3: neither of these things is true, this->machine_id_ does not care.
  }

  // We have all the information we need in received and sent!
  shuffling_order.clear();

  // query_machine_ids needs to be filled in original non-shuffled order.
  for (auto &[target, _] : sent) {
    uint32_t target_machine_id = target / this->size_ + 1;
    this->query_machine_ids_.push_back(target_machine_id);
  }

  // sent[i] ===> the global index corresponding to where the ith query
  //              this machine is sending out is going to end up after shuffle.
  // received[i]: the global index corresponding of the query that will end up
  //              in the ith location within this machine's bucket post shuffle.
  std::sort(received.begin(), received.end());
  std::sort(sent.begin(), sent.end());

  // Fill in our maps!
  for (auto &[previous, target] : received) {
    uint32_t owner = previous / this->size_ + 1;
    query_indices_[owner].second.push_back(target);
  }

  uint32_t last_owner = 0;
  std::vector<std::pair<uint32_t, uint32_t>> relative_order;
  for (auto &[target, previous] : sent) {
    uint32_t owner = target / this->size_ + 1;
    // Fill in the relative order for queries sent from this machine to
    // last_owner after we encountered all of them!
    if (owner != last_owner) {
      if (last_owner != 0) {
        std::sort(relative_order.begin(), relative_order.end());
        for (auto &[_, order] : relative_order) {
          this->query_order_[last_owner].push_back(order);
        }
        this->query_states_.at(last_owner).second.resize(relative_order.size());
        relative_order.clear();
      }
      last_owner = owner;
    }
    // Fill in related entries for this sent query.
    response_indices_[owner].second.push_back(previous);
    relative_order.push_back(std::make_pair(previous, relative_order.size()));
  }

  // The very last owner...
  if (last_owner != 0) {
    std::sort(relative_order.begin(), relative_order.end());
    for (auto &[_, order] : relative_order) {
      this->query_order_[last_owner].push_back(order);
    }
    this->query_states_.at(last_owner).second.resize(relative_order.size());
    relative_order.clear();
  }
}

// Returns a vector (logically a map) from a machine_id to the count
// of queries expected to be received from that machine.
std::vector<uint32_t> Shuffler::IncomingQueriesCount() {
  std::vector<uint32_t> result(this->parallelism_ + 1, 0);
  for (uint32_t m = 1; m <= this->parallelism_; m++) {
    uint32_t count = this->query_indices_.at(m).second.size();
    result[m] = count;
  }
  return result;
}

// Determines the machine the next query is meant to be sent to.
uint32_t Shuffler::MachineOfNextQuery(const types::QueryState &query_state) {
  // Find which machine this query is meant for.
  uint32_t machine_id = this->query_machine_ids_.front();
  this->query_machine_ids_.pop_front();
  // Find the relative order of this query among all queries from this machine
  // meant for the target machine.
  auto &nested_list = this->query_order_.at(machine_id);
  uint32_t relative_order = nested_list.front();
  nested_list.pop_front();
  // Store query_state according to the relative order.
  auto &[_, query_states] = this->query_states_.at(machine_id);
  query_states.at(relative_order) = query_state;
  // Return the machine id.
  return machine_id;
}

// Determines the machine the next response is meant to be sent to.
uint32_t Shuffler::MachineOfNextResponse() {
  uint32_t machine_id = this->response_machine_ids_.front();
  this->response_machine_ids_.pop_front();
  return machine_id;
}

// Shuffle Query into shuffler.
bool Shuffler::ShuffleQuery(uint32_t machine_id,
                            const types::ForwardQuery &query) {
  // Find index.
  auto &[i, nested_map] = this->query_indices_.at(machine_id);
  uint32_t index = nested_map.at(i++);
  // Copy into index.
  unsigned char *copy = new unsigned char[this->forward_query_size_];
  memcpy(copy, query, this->forward_query_size_);
  this->shuffled_queries_.at(index) = copy;
  return ++this->shuffled_query_count_ == this->size_;
}

// Deshuffle the response using stored shuffling order.
bool Shuffler::DeshuffleResponse(uint32_t machine_id,
                                 const types::Response &response) {
  // Find index.
  auto &[i, nested_map] = this->response_indices_.at(machine_id);
  uint32_t index = nested_map.at(i++);
  // Copy into index.
  this->deshuffled_responses_.at(index) = response;
  return ++this->deshuffled_response_count_ == this->size_;
}

// Get the next query in the shuffled order.
types::ForwardQuery &Shuffler::NextQuery() {
  return this->shuffled_queries_.at(this->query_index_++);
}

// Get the next response in the de-shuffled order.
types::Response &Shuffler::NextResponse() {
  return this->deshuffled_responses_.at(this->response_index_++);
}

// Get the next query state in the shuffled order.
types::QueryState &Shuffler::NextQueryState(uint32_t machine_id) {
  auto &[i, nested_map] = this->query_states_.at(machine_id);
  return nested_map.at(i++);
}

}  // namespace protocol
}  // namespace drivacy

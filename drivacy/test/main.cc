// Copyright 2020 multiparty.org

// Main entry point to our protocol.
// This file must be run from the command line once per server:
// every time this file is run, it is logically equivalent to a new
// server.
// The server's configurations, including which party it belongs to, is
// set by the input configuration file passed to it via the command line.

#include <cstdint>
#include <iostream>
#include <list>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "drivacy/io/simulated_socket.h"
#include "drivacy/parties/client.h"
#include "drivacy/parties/head_party.h"
#include "drivacy/parties/party.h"
#include "drivacy/types/config.pb.h"
#include "drivacy/types/types.h"
#include "drivacy/util/file.h"
#include "drivacy/util/status.h"

ABSL_FLAG(std::string, table, "", "The path to table JSON file (required)");
ABSL_FLAG(std::string, config, "", "The path to configuration file (required)");

absl::Status Test(const drivacy::types::Configuration &config,
                  const drivacy::types::Table &table) {
  std::cout << "Testing..." << std::endl;

  // Create a client.
  drivacy::parties::Client client(
      config, drivacy::io::socket::SimulatedClientSocket::Factory);

  // Verify correctness of query / response.
  std::list<uint64_t> queries;
  client.SetOnResponseHandler([&](uint64_t query, uint64_t response) {
    assert(query == queries.front());
    assert(response == table.at(query));
    queries.pop_front();
  });

  // Query everything in the table.
  for (const auto &[query, response] : table) {
    queries.push_back(query);
    client.MakeQuery(query);
  }

  // Make sure all queries were handled.
  assert(queries.size() == 0);
  std::cout << "All tests passed!" << std::endl;

  return absl::OkStatus();
}

absl::Status Setup(const std::string &table_path,
                   const std::string &config_path) {
  std::cout << "Setting up..." << std::endl;

  // Read configuration.
  drivacy::types::Configuration config;
  CHECK_STATUS(drivacy::util::file::ReadProtobufFromJson(config_path, &config));

  // Read input table.
  ASSIGN_OR_RETURN(std::string json,
                   drivacy::util::file::ReadFile(table_path.c_str()));
  ASSIGN_OR_RETURN(drivacy::types::Table table,
                   drivacy::util::file::ParseTable(json));

  // Setup parties.
  drivacy::parties::HeadParty head_party(
      1, config, table, drivacy::io::socket::SimulatedSocket::Factory,
      drivacy::io::socket::SimulatedClientSocket::Factory);
  std::list<drivacy::parties::Party> parties;
  for (uint32_t party_id = 2; party_id <= config.parties(); party_id++) {
    parties.emplace_back(party_id, config, table,
                         drivacy::io::socket::SimulatedSocket::Factory);
  }

  // Make a query.
  return Test(config, table);
}

int main(int argc, char *argv[]) {
  // Verify protobuf library was linked properly.
  GOOGLE_PROTOBUF_VERIFY_VERSION;

  // Command line usage message.
  absl::SetProgramUsageMessage(absl::StrFormat("usage: %s %s", argv[0],
                                               "--table=path/to/table.json "
                                               "--config=path/to/config.json"));
  absl::ParseCommandLine(argc, argv);

  // Get command line flags.
  const std::string &table_path = absl::GetFlag(FLAGS_table);
  const std::string &config_path = absl::GetFlag(FLAGS_config);
  if (table_path.empty()) {
    std::cout << "Please provide a valid table JSON file using --table"
              << std::endl;
    return 1;
  }
  if (config_path.empty()) {
    std::cout << "Please provide a valid config file using --config"
              << std::endl;
    return 1;
  }

  // Do the testing!
  absl::Status output = Setup(table_path, config_path);
  if (!output.ok()) {
    std::cout << output << std::endl;
    return 1;
  }

  // Clean up protobuf.
  google::protobuf::ShutdownProtobufLibrary();
  return 0;
}
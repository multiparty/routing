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
#include "drivacy/client.h"
#include "drivacy/io/file.h"
#include "drivacy/io/simulated_socket.h"
#include "drivacy/party.h"
#include "drivacy/types/config.pb.h"
#include "drivacy/types/types.h"
#include "drivacy/util/status.h"

ABSL_FLAG(std::string, table, "", "The path to table JSON file (required)");
ABSL_FLAG(std::string, config, "", "The path to configuration file (required)");

absl::Status Protocol(const std::string &table_path,
                      const std::string &config_path) {
  // Read configuration.
  drivacy::types::Configuration config;
  CHECK_STATUS(drivacy::io::file::ReadProtobufFromJson(config_path, &config));
  std::cout << "Parties: " << config.parties() << std::endl;
  std::cout << std::endl;

  // Read input table.
  ASSIGN_OR_RETURN(std::string json,
                   drivacy::io::file::ReadFile(table_path.c_str()));
  ASSIGN_OR_RETURN(drivacy::types::Table table,
                   drivacy::io::file::ParseTable(json));

  const auto &[expected_query, expected_response] = *table.cbegin();
  std::cout << "Table size: " << table.size() << std::endl;
  std::cout << "\t" << expected_query << " => " << expected_response
            << std::endl;
  std::cout << std::endl;

  // Execute mock protocol.
  std::cout << "Mock protocol:" << std::endl;
  std::cout << "\tclient query: " << expected_query << std::endl;

  // Setup parties.
  std::list<drivacy::Party<drivacy::io::socket::SimulatedSocket>> parties;
  for (uint32_t party_id = 1; party_id <= config.parties(); party_id++) {
    parties.emplace_back(party_id, config, table);
  }

  // Create a client.
  drivacy::Client<drivacy::io::socket::SimulatedSocket> client(config);
  client.SetOnResponseHandler([=](uint64_t query, uint64_t response) {
    assert(query == expected_query);
    assert(response == expected_response);
    std::cout << "\tclient response: " << response << std::endl;
  });

  // Make a query.
  client.MakeQuery(expected_query);
  return absl::OkStatus();
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

  // Execute mock protocol.
  absl::Status output = Protocol(table_path, config_path);
  if (!output.ok()) {
    std::cout << output << std::endl;
    return 1;
  }

  // Clean up protobuf.
  google::protobuf::ShutdownProtobufLibrary();
  return 0;
}

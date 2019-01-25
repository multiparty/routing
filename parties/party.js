// Includes shared code and functionality between frontend and backend parties!
// Modeled as a library where a frontend or backend party calls one or more of the provided functions
// at the right time in the execution / protocols.
// The main different in frontend and backend parties is not the code they execute, but rather when they execute it!
// Frontend and backend parties are both event-driven, with different events coming in either from the end-users
// or the different parties.
// Check parties/frontend.js or parties/backend.js for the code of frontend and backend parties respectively.

// Dependencies
var express = require('express');
var jiff_client = require('../jiff/lib/jiff-client');
var jiff_client_bignumber = require('../jiff/lib/ext/jiff-client-bignumber');

// Configurations!
var config = require('./config/config.js');

// Express Configuration
var app = express();

app.use(function (req, res, next) { // Cross Origin Requests Allowed
  res.header('Access-Control-Allow-Origin', '*');
  res.header('Access-Control-Allow-Credentials', 'true');
  res.header('Access-Control-Allow-Methods', 'OPTIONS, GET, POST');
  res.header('Access-Control-Allow-Headers', 'Content-Type, Depth, User-Agent, X-File-Size, X-Requested-With, If-Modified-Since, X-File-Name, Cache-Control');
  next();
});

// JIFF Configuration
var options = {
  // computation meta-data
  party_id: config.id,
  party_count: config.all_parties.length,
  autoConnect: false,
  initialization: {
    owner_party: config.owner
  },
  onConnect: function (jiff_instance) { // Connection
    var port = config.base_port + jiff_instance.id;
    app.listen(port, function () { // Start listening with express on port 9111
      console.log('Party up and listening on ' + port);
    });
  }
};

// Connect JIFF
var jiff_instance = jiff_client.make_jiff('http://localhost:3000', 'shortest-path-1', options);
jiff_instance.apply_extension(jiff_client_bignumber, options);
jiff_instance.connect();

// Create the this object
module.exports = {
  app: app,
  jiff: jiff_instance,
  config: config,
  // Shared properties
  keys: {}, // map recompute number to [ <column1_key>, <column2_key> ]
  recompute_number: 0
};
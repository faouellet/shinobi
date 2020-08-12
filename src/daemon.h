// Copyright 2020 Félix-Antoine Ouellet. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <boost/asio.hpp>
#include <memory>
#include <unordered_set>

namespace net = boost::asio;

using tcp = net::ip::tcp;
using ErrorCode = boost::system::error_code;

/// Multithreaded FTP server that must be run on any machine that
/// whishes to be part of the distributed cache system.
class Daemon {
  class Connection;
  using ConnectionPtr = std::shared_ptr<Connection>;

 public:
  /// Ctor
  Daemon(unsigned short port, std::string root);

  /// Starts the daemon execution. The thread calling this method will be
  /// blocked until someone explicitly stops the daemon. When the run is done,
  /// an error code will be returned.
  ErrorCode Run();

  /// Stops the daemon execution. Can be called from any thread except the
  /// one running the daemon
  void Stop();

 private:
  /// Accepts an incoming connection request
  void DoAccept();

  /// Context of the network messaging
  net::io_context io_context_;

  /// Handles the incoming connection requests
  tcp::acceptor acceptor_;

  /// Guard to make sure the context doesn't shutdown when no work is queued
  net::executor_work_guard<net::io_context::executor_type> work_;

  /// Root of the directory from which to select files for responses
  std::string root_;

  /// List of active connections
  std::unordered_set<ConnectionPtr> active_connections_;

  /// Delay allowed for processing a single request
  const boost::posix_time::time_duration write_timeout_ =
      boost::posix_time::seconds(30);
};

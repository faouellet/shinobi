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

#include "dcache.h"

#include <boost/asio.hpp>
#include <iostream>

namespace net = boost::asio;

static const char delim{ '\n' };

#define RETURN_ON_ERROR(error, val)       \
  if (error) {                            \
    std::cerr << error.message() << '\n'; \
    return val;                           \
  }

/// Host volunteering to be part of the distributed cache
class Host {
  using tcp = net::ip::tcp;

 public:
  /// Ctor
  Host() : io_context_{}, socket_{ io_context_ } {}

  /// Initializes a host connection. In other words, connects to a daemon
  /// running on a machine wihtin the local network.
  bool Init(const std::string& host, const std::string& service) {
    tcp::resolver resolver{ io_context_ };
    boost::system::error_code error;
    tcp::resolver::query query{ host, service,
                                net::ip::resolver_query_base::numeric_service };
    const auto endpoints = resolver.resolve(query, error);
    RETURN_ON_ERROR(error, false);

    net::connect(socket_, endpoints, error);
    RETURN_ON_ERROR(error, false);

    return true;
  }

  /// Gets the content of a file whose relative path matches a relative path on
  /// a host
  std::vector<unsigned char> GetFileContents(const std::string& path) {
    if (!socket_.is_open()) {
      // Can't do anything with a closed socket.
      return {};
    }

    boost::system::error_code error;

    // Synchronously request a file's content.
    net::write(socket_, net::buffer(path + delim, path.size() + 1), error);
    RETURN_ON_ERROR(error, {});

    net::streambuf response;
    // Synchronously wait for a response to our request.
    // Even if the file isn't there on the host, we're guaranteed to have a
    // response.
    net::read_until(socket_, response, delim, error);
    RETURN_ON_ERROR(error, {});

    // Send back the content in a non-Boost data format
    auto buf = net::buffer_cast<const unsigned char*>(response.data());
    return std::vector<unsigned char>{ buf, buf + response.size() - 1 };
  }

 private:
  /// Context of the network messaging
  net::io_context io_context_;

  /// Socket used to communicate with the host
  tcp::socket socket_;
};

DCache::DCache() = default;
DCache::~DCache() = default;

void DCache::Init(const HostInfos& infos) {
  for (const auto& [addr, service] : infos) {
    auto host = std::make_unique<Host>();
    if (!host->Init(addr, service)) {
      continue;  // Too bad
    }
    hosts_.push_back(std::move(host));
  }
}

std::vector<unsigned char> DCache::GetFileContents(
    const std::string& path) const {
  // Ask around to see if anyone has this file
  for (const auto& host : hosts_) {
    if (host == nullptr) {
      continue;
    }

    std::vector<unsigned char> fileContents{ host->GetFileContents(path) };
    if (!fileContents.empty()) {
      return fileContents;
    }
  }

  // No one has the file
  return {};
}

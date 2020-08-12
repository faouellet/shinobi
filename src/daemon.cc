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

#include "daemon.h"

#include <atomic>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

/// Message delimiter
static const char delim{ '\n' };

///
class Daemon::Connection : public std::enable_shared_from_this<Connection> {
 public:
  /// Ctor
  Connection(Daemon& daemon);

  /// Starts the connection's worker thread which will process any and all
  /// incoming request
  void Start();

  /// Completely closes this connection
  void Shutdown();

  /// Gives read/write access to the socket with which the connection operate
  tcp::socket& GetSocket();

 private:
  /// Gets the next request to process
  void FetchRequest();

  /// Prepares a response i.e. prepares a file stream buffer to send over the
  /// network
  void ProcessRequest(const std::string& path);

  /// Sends the response (a file's raw contents) to a previously made request
  void SendResponse(std::ifstream& stream);

  /// Daemon which spawned this connection
  Daemon& daemon_;

  /// Incoming request buffer
  net::streambuf buf_in_;

  /// Is the connection currently closed?
  std::atomic<bool> closed_{ false };

  /// Timer counting down the time left to send back a response
  net::deadline_timer write_timer_;

  /// Socket through which this Connection communicate
  tcp::socket socket_;
};

#define SHUTDOWN_IF(cond) \
  if (cond) {             \
    Shutdown();           \
    return;               \
  }

Daemon::Connection::Connection(Daemon& daemon)
    : daemon_{ daemon }, write_timer_{ daemon.io_context_ }, socket_{
        daemon.io_context_
      } {}

void Daemon::Connection::Start() {
  socket_.set_option(tcp::no_delay(true));
  // Immediatly start fetching request
  FetchRequest();
}

void Daemon::Connection::Shutdown() {
  // Another thread might be trying to close this connection.
  // To avoid potential problems, we ensure with a CAS that
  // only one thread will shutdown the connection.
  bool expectedClosedValue{ false };
  bool desiredClosedValue{ true };
  if (closed_.compare_exchange_strong(expectedClosedValue,
                                      desiredClosedValue)) {
    boost::system::error_code ec;
    socket_.shutdown(tcp::socket::shutdown_both, ec);
    socket_.close();
    write_timer_.cancel(ec);
    daemon_.active_connections_.erase(shared_from_this());
  }
}

tcp::socket& Daemon::Connection::GetSocket() {
  return socket_;
}

void Daemon::Connection::FetchRequest() {
  net::async_read_until(
      socket_, buf_in_, delim,
      [this, self = shared_from_this()](const ErrorCode& ec,
                                        std::size_t bytes_read) {
        SHUTDOWN_IF(ec);

        const char* data = net::buffer_cast<const char*>(buf_in_.data());
        const std::string request{
          data,
          bytes_read - (bytes_read > 1 && data[bytes_read - 2] == '\0' ? 2 : 1)
        };
        buf_in_.consume(bytes_read);
        ProcessRequest(request);
      });
}

void Daemon::Connection::ProcessRequest(const std::string& path) {
  // For better throughput, each request will be handled in its own thread.
  // Just before dying, when all processing is done, the thread will post
  // a message on the io_context to make the Connection fetch another request.
  std::thread t([this, self = shared_from_this(), path] {
    std::ifstream stream{ (daemon_.root_ + "/" + path).c_str() };

    SendResponse(stream);
    daemon_.io_context_.post([this, self]() { FetchRequest(); });
  });
  t.detach();
}

void Daemon::Connection::SendResponse(std::ifstream& stream) {
  write_timer_.expires_from_now(daemon_.write_timeout_);
  write_timer_.async_wait(
      [this](const boost::system::error_code& ec) { SHUTDOWN_IF(!ec); });

  net::streambuf streambuf;
  std::ostream os{ &streambuf };
  os << stream.rdbuf() << delim;

  net::async_write(socket_, streambuf, net::transfer_all(),
                   [this, self = shared_from_this()](const ErrorCode& ec,
                                                     size_t bytes_transferred) {
                     write_timer_.cancel();
                     SHUTDOWN_IF(ec);
                   });
}

#undef SHUTDOWN_IF

Daemon::Daemon(unsigned short port, std::string root)
    : acceptor_{ io_context_, tcp::endpoint{ tcp::v6(), port }, false },
      work_{ net::make_work_guard(io_context_) }, root_{ std::move(root) } {}

ErrorCode Daemon::Run() {
  DoAccept();
  ErrorCode err;
  io_context_.run(err);
  return err;
}

void Daemon::Stop() {
  acceptor_.close();

  // Since we have a multithreaded daemon, we have to participate in the sharing
  // of the connections' pointers here otherwise they might disappear before our
  // eyes!
  std::vector<ConnectionPtr> sessions_to_close;
  std::copy(active_connections_.begin(), active_connections_.end(),
            back_inserter(sessions_to_close));

  for (auto& session : sessions_to_close) {
    session->Shutdown();
  }

  active_connections_.clear();
  io_context_.stop();
}

void Daemon::DoAccept() {
  if (acceptor_.is_open()) {
    auto session = std::make_shared<Connection>(*this);
    acceptor_.async_accept(session->GetSocket(),
                           [this, session](const ErrorCode& ec) {
                             if (!ec) {
                               active_connections_.insert(session);
                               session->Start();
                             }
                             DoAccept();
                           });
  }
}

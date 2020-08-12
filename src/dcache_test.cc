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

#include <filesystem>
#include <fstream>
#include <iostream>
#include <system_error>
#include <thread>

#include "daemon.h"
#include "test.h"

namespace {

const std::string litany{
  "I must not fear."
  "Fear is the mind-killer."
  "Fear is the little-death that brings total obliteration."
  "I will face my fear."
  "I will permit it to pass over me and through me."
  "And when it has gone past I will turn the inner eye to see its path."
  "Where the fear has gone there will be nothing. Only I will remain."
};

/// Testing environment
struct TestFixture : public testing::Test {
  virtual ~TestFixture() = default;

  /// Sets up the testing environment
  void SetUp() override {
    // Testing should be done in a specially made directory
    // containing testing files
    std::error_code err_code;
    if (!std::filesystem::create_directory(test_dir_, err_code)) {
      return;
    }

    std::ofstream oStream{ GetTestFilePath() };
    if (!oStream.is_open()) {
      return;
    }
    oStream << litany;
    if (!oStream.good()) {
      return;
    }

    // A server will block the thread it runs on until it's stopped.
    // Thus, we need a separate thread for it.
    server_thread_ = std::thread{ [this]() {
      daemon_ = std::make_unique<Daemon>(8082, test_dir_);
      const ErrorCode run_error = daemon_->Run();

      if (run_error.failed()) {
        std::cerr << run_error.message() << "\n";
      }
    } };
  }

  /// Clean all the things
  void TearDown() override {
    daemon_->Stop();
    server_thread_.join();
    std::error_code err_code;
    std::filesystem::remove_all(test_dir_, err_code);
  }

  const std::string& GetTestFileName() const { return test_file_; }

 private:
  std::string GetTestFilePath() const { return test_dir_ + "/" + test_file_; }

  /// Daemon used for testing
  std::unique_ptr<Daemon> daemon_;

  /// Thread on which the Daemon will run
  std::thread server_thread_;

  /// Physical resources of the testing environment
  inline static const std::string test_dir_{ "TEST_DIR" };
  inline static const std::string test_file_{ "litany" };
};

}  // namespace

/// Example of the interaction between the distributed cache system and a
/// daemon.
TEST_F(TestFixture, Example) {
  const HostInfos infos{ { "localhost", "8082" } };
  DCache cache;
  ASSERT_TRUE(cache.Init(infos));

  const std::vector<unsigned char> raw_contents{ cache.GetFileContents(
      GetTestFileName()) };
  const std::string contents{ raw_contents.cbegin(), raw_contents.cend() };
  ASSERT_EQ(litany, contents);
}

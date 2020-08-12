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

#include "host_parser.h"

#include <iostream>

#include "state.h"
#include "test.h"

struct HostParserTest : public testing::Test {
  void AssertParse(const char* input) {
    HostParser parser(&state, &fs_);
    std::string err;
    EXPECT_TRUE(parser.ParseTest(input, &err));
    std::cout << err << std::endl;
    ASSERT_EQ("", err);
  }

  State state;
  VirtualFileSystem fs_;
};

TEST_F(HostParserTest, Empty) {
  ASSERT_NO_FATAL_FAILURE(AssertParse(""));
  ASSERT_TRUE(state.hosts_.empty());
}

TEST_F(HostParserTest, Hosts) {
  ASSERT_NO_FATAL_FAILURE(
      AssertParse("["
                  "{"
                  " \"host\": \"172.17.0.2\","
                  " \"port\": 8082"
                  "},"
                  "{"
                  " \"host\": \"172.17.0.1\","
                  " \"port\": 8081"
                  "}"
                  "]"));
  ASSERT_TRUE(state.hosts_.size() == 2);
  ASSERT_TRUE(state.hosts_[0].first == "172.17.0.2");
  ASSERT_TRUE(state.hosts_[0].second == "8082");
  ASSERT_TRUE(state.hosts_[1].first == "172.17.0.1");
  ASSERT_TRUE(state.hosts_[1].second == "8081");
}

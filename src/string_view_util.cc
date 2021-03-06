// Copyright 2017 Google Inc. All Rights Reserved.
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

#include "string_view_util.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

std::vector<std::string_view> SplitStringView(std::string_view input,
                                              char sep) {
  std::vector<std::string_view> elems;
  elems.reserve(std::count(input.begin(), input.end(), sep) + 1);

  size_t pos = 0;

  for (;;) {
    const size_t next_pos = input.find(sep, pos);
    if (next_pos == std::string_view::npos) {
      elems.emplace_back(input.data() + pos, input.size() - pos);
      break;
    }
    elems.emplace_back(input.data() + pos, next_pos - pos);
    pos = next_pos + 1;
  }

  return elems;
}

std::string JoinStringView(const std::vector<std::string_view>& list,
                           char sep) {
  if (list.empty()) {
    return "";
  }

  std::string ret;

  {
    size_t cap = list.size() - 1;
    for (auto i : list) {
      cap += i.size();
    }
    ret.reserve(cap);
  }

  for (size_t i = 0; i < list.size(); ++i) {
    if (i != 0) {
      ret += sep;
    }
    ret.append(list[i].data(), list[i].size());
  }

  return ret;
}

bool EqualsCaseInsensitiveASCII(std::string_view a, std::string_view b) {
  if (a.size() != b.size()) {
    return false;
  }

  for (size_t i = 0; i < a.size(); ++i) {
    if (ToLowerASCII(a.data()[i]) != ToLowerASCII(b.data()[i])) {
      return false;
    }
  }

  return true;
}

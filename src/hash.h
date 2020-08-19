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

#ifndef NINJA_HASH_H_
#define NINJA_HASH_H_

#include <cinttypes>
#include <cstring>

// 64bit MurmurHash2, by Austin Appleby
#if defined(_MSC_VER)
#define BIG_CONSTANT(x) (x)
#else  // defined(_MSC_VER)
#define BIG_CONSTANT(x) (x##LLU)
#endif  // !defined(_MSC_VER)
inline uint64_t MurmurHash64A(const void* key, size_t len) {
  static const uint64_t seed = 0xDECAFBADDECAFBADull;
  const uint64_t m = BIG_CONSTANT(0xc6a4a7935bd1e995);
  const int r = 47;
  uint64_t h = seed ^ (len * m);
  const auto* data = (const unsigned char*)key;
  while (len >= 8) {
    uint64_t k;
    memcpy(&k, data, sizeof k);
    k *= m;
    k ^= k >> r;
    k *= m;
    h ^= k;
    h *= m;
    data += 8;
    len -= 8;
  }
  switch (len & 7) {
  case 7:
    h ^= uint64_t(data[6]) << 48;
    [[fallthrough]];
  case 6:
    h ^= uint64_t(data[5]) << 40;
    [[fallthrough]];
  case 5:
    h ^= uint64_t(data[4]) << 32;
    [[fallthrough]];
  case 4:
    h ^= uint64_t(data[3]) << 24;
    [[fallthrough]];
  case 3:
    h ^= uint64_t(data[2]) << 16;
    [[fallthrough]];
  case 2:
    h ^= uint64_t(data[1]) << 8;
    [[fallthrough]];
  case 1:
    h ^= uint64_t(data[0]);
    h *= m;
  };
  h ^= h >> r;
  h *= m;
  h ^= h >> r;
  return h;
}
#undef BIG_CONSTANT

#endif  // NINJA_HASH_H_

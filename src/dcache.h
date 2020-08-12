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

#ifndef NINJA_DCACHE_H_
#define NINJA_DCACHE_H_

#include <memory>
#include <vector>

class Host;
using HostInfo = std::pair<std::string, std::string>;
using HostInfos = std::vector<HostInfo>;

/// Distributed caching system
class DCache {
 public:
  DCache();
  ~DCache();

  /// Initializes the distributed cache
  bool Init(const HostInfos& infos);

  /// Fetches the contents of a given file from the cache.
  /// The returned vector will be empty if a problem occurs or if
  /// the file is not available on any hosts.
  std::vector<unsigned char> GetFileContents(const std::string& path) const;

  /// No copies allowed
  DCache(const DCache&) = delete;
  DCache& operator=(const DCache&) = delete;

 private:
  /// Hosts making up the distributed cache
  std::vector<std::unique_ptr<Host>> hosts_;
};

#endif  // NINJA_DCACHE_H_

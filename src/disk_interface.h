// Copyright 2011 Google Inc. All Rights Reserved.
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

#ifndef NINJA_DISK_INTERFACE_H_
#define NINJA_DISK_INTERFACE_H_

#include <bits/stdint-uintn.h>

#include <map>
#include <string>

#include "timestamp.h"

/// Interface for reading files from disk.  See DiskInterface for details.
/// This base offers the minimum interface needed just to read files.
struct FileReader {
  virtual ~FileReader() = default;

  /// Result of ReadFile.
  enum Status { Okay, NotFound, OtherError };

  /// Read and store in given std::string.  On success, return Okay.
  /// On error, return another Status and fill |err|.
  virtual Status ReadFile(const std::string& path, std::string* contents,
                          std::string* err) const = 0;
};

/// Interface for accessing the disk.
///
/// Abstract so it can be mocked out for tests.  The real implementation
/// is RealDiskInterface.
struct DiskInterface : public FileReader {
  /// stat() a file, returning the mtime, or 0 if missing and -1 on
  /// other errors.
  virtual TimeStamp Stat(const std::string& path, std::string* err) const = 0;

  /// Hash the contents of a file. Returns 0 in case an error happens.
  virtual uint64_t Hash(const std::string& path, std::string* err) const = 0;

  /// Create a directory, returning false on failure.
  virtual bool MakeDir(const std::string& path) = 0;

  /// Remove a directory, returning false on failure.
  virtual bool RemoveDir(const std::string& path) = 0;

  /// Create a file, with the specified name and contents
  /// Returns true on success, false on failure
  virtual bool WriteFile(const std::string& path,
                         const std::string& contents) = 0;

  /// Remove the file named @a path. It behaves like 'rm -f path' so no errors
  /// are reported if it does not exists.
  /// @returns 0 if the file has been removed,
  ///          1 if the file does not exist, and
  ///          -1 if an error occurs.
  virtual int RemoveFile(const std::string& path) = 0;

  /// Create all the parent directories for path; like mkdir -p
  /// `basename path`.
  bool MakeDirs(const std::string& path);
};

/// Implementation of DiskInterface that actually hits the disk.
struct RealDiskInterface : public DiskInterface {
  RealDiskInterface()
#ifdef _WIN32
      : use_cache_(false)
#endif
  {
  }

  ~RealDiskInterface() override = default;
  TimeStamp Stat(const std::string& path, std::string* err) const override;
  uint64_t Hash(const std::string& path, std::string* err) const override;
  bool MakeDir(const std::string& path) override;
  bool RemoveDir(const std::string& path) override;
  bool WriteFile(const std::string& path, const std::string& contents) override;
  Status ReadFile(const std::string& path, std::string* contents,
                  std::string* err) const override;
  int RemoveFile(const std::string& path) override;

  /// Whether stat information can be cached.  Only has an effect on Windows.
  void AllowStatCache(bool allow);

 private:
#ifdef _WIN32
  /// Whether stat information can be cached.
  bool use_cache_;

  using DirCache = std::map<std::string, TimeStamp>;
  // TODO: Neither a std::map nor a hashstd::map seems ideal here.  If the
  // statcache works out, come up with a better data structure.
  using Cache = std::map<std::string, DirCache>;
  mutable Cache cache_;
#endif
};

#endif  // NINJA_DISK_INTERFACE_H_

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

// On AIX, inttypes.h gets indirectly included by build_log.h.
// It's easiest just to ask for the printf format macros right away.
#ifndef _WIN32
#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif
#endif

#include "build_log.h"

#include <cerrno>
#include <cstdlib>
#include <cstring>

#include "disk_interface.h"

#ifndef _WIN32
#include <unistd.h>

#include <cinttypes>
#include <utility>

#endif

#include "build.h"
#include "graph.h"
#include "hash.h"
#include "metrics.h"
#include "util.h"
#if defined(_MSC_VER) && (_MSC_VER < 1800)
#define strtoll _strtoi64
#endif

// Implementation details:
// Each run's log appends to the log file.
// To load, we run through all log entries in series, throwing away
// older runs.
// Once the number of redundant entries exceeds a threshold, we write
// out a new file and replace the existing one with it.

namespace {

const char kFileSignature[] = "# ninja log v%d\n";
const int kOldestSupportedVersion = 4;
const int kCurrentVersion = 5;

}  // namespace

// static
uint64_t BuildLog::LogEntry::Hash(std::string_view command) {
  return MurmurHash64A(command.data(), command.size());
}

BuildLog::LogEntry::LogEntry(std::string output) : output(std::move(output)) {}

BuildLog::LogEntry::LogEntry(std::string output, uint64_t command_hash,
                             int start_time, int end_time,
                             TimeStamp restat_mtime)
    : output(std::move(output)), command_hash(command_hash),
      start_time(start_time), end_time(end_time), mtime(restat_mtime) {}

BuildLog::BuildLog() : log_file_(nullptr) {}

BuildLog::~BuildLog() {
  Close();
}

bool BuildLog::OpenForWrite(const std::string& path, const BuildLogUser& user,
                            std::string* err) {
  if (needs_recompaction_) {
    if (!Recompact(path, user, err))
      return false;
  }

  log_file_ = fopen(path.c_str(), "ab");
  if (!log_file_) {
    *err = strerror(errno);
    return false;
  }
  std::setvbuf(log_file_, nullptr, _IOLBF, BUFSIZ);
  SetCloseOnExec(fileno(log_file_));

  // Opening a file in append mode doesn't std::set the file pointer to the
  // file's end on Windows. Do that explicitly.
  fseek(log_file_, 0, SEEK_END);

  if (ftell(log_file_) == 0) {
    if (fprintf(log_file_, kFileSignature, kCurrentVersion) < 0) {
      *err = strerror(errno);
      return false;
    }
  }

  return true;
}

bool BuildLog::RecordCommand(Edge* edge, int start_time, int end_time,
                             TimeStamp mtime) {
  std::string command = edge->EvaluateCommand(true);
  uint64_t command_hash = LogEntry::Hash(command);
  for (auto& output : edge->outputs_) {
    const std::string& path = output->path();
    auto i = entries_.find(path);
    LogEntry* log_entry;
    if (i != entries_.end()) {
      log_entry = i->second;
    } else {
      log_entry = new LogEntry(path);
      entries_.insert(Entries::value_type(log_entry->output, log_entry));
    }
    log_entry->command_hash = command_hash;
    log_entry->start_time = start_time;
    log_entry->end_time = end_time;
    log_entry->mtime = mtime;

    if (log_file_) {
      if (!WriteEntry(log_file_, *log_entry))
        return false;
      if (fflush(log_file_) != 0) {
        return false;
      }
    }
  }
  return true;
}

void BuildLog::Close() {
  if (log_file_)
    fclose(log_file_);
  log_file_ = nullptr;
}

struct LineReader {
  explicit LineReader(FILE* file)
      : file_(file), buf_end_(buf_), line_start_(buf_), line_end_(nullptr) {
    memset(buf_, 0, sizeof(buf_));
  }

  // Reads a \n-terminated line from the file passed to the constructor.
  // On return, *line_start points to the beginning of the next line, and
  // *line_end points to the \n at the end of the line. If no newline is seen
  // in a fixed buffer size, *line_end is std::set to NULL. Returns false on
  // EOF.
  bool ReadLine(char** line_start, char** line_end) {
    if (line_start_ >= buf_end_ || !line_end_) {
      // Buffer empty, refill.
      size_t size_read = fread(buf_, 1, sizeof(buf_), file_);
      if (!size_read)
        return false;
      line_start_ = buf_;
      buf_end_ = buf_ + size_read;
    } else {
      // Advance to next line in buffer.
      line_start_ = line_end_ + 1;
    }

    line_end_ = (char*)memchr(line_start_, '\n', buf_end_ - line_start_);
    if (!line_end_) {
      // No newline. Move rest of data to start of buffer, fill rest.
      size_t already_consumed = line_start_ - buf_;
      size_t size_rest = (buf_end_ - buf_) - already_consumed;
      memmove(buf_, line_start_, size_rest);

      size_t read = fread(buf_ + size_rest, 1, sizeof(buf_) - size_rest, file_);
      buf_end_ = buf_ + size_rest + read;
      line_start_ = buf_;
      line_end_ = (char*)memchr(line_start_, '\n', buf_end_ - line_start_);
    }

    *line_start = line_start_;
    *line_end = line_end_;
    return true;
  }

 private:
  FILE* file_;
  char buf_[256 << 10];
  char* buf_end_;  // Points one past the last valid byte in |buf_|.

  char* line_start_;
  // Points at the next \n in buf_ after line_start, or NULL.
  char* line_end_;
};

LoadStatus BuildLog::Load(const std::string& path, std::string* err) {
  METRIC_RECORD(".ninja_log load");
  FILE* file = fopen(path.c_str(), "r");
  if (!file) {
    if (errno == ENOENT)
      return LOAD_NOT_FOUND;
    *err = strerror(errno);
    return LOAD_ERROR;
  }

  int log_version = 0;
  int unique_entry_count = 0;
  int total_entry_count = 0;

  LineReader reader(file);
  char* line_start = nullptr;
  char* line_end = nullptr;
  while (reader.ReadLine(&line_start, &line_end)) {
    if (!log_version) {
      sscanf(line_start, kFileSignature, &log_version);

      if (log_version < kOldestSupportedVersion) {
        *err =
            ("build log version invalid, perhaps due to being too old; "
             "starting over");
        fclose(file);
        unlink(path.c_str());
        // Don't report this as a failure.  An empty build log will cause
        // us to rebuild the outputs anyway.
        return LOAD_SUCCESS;
      }
    }

    // If no newline was found in this chunk, read the next.
    if (!line_end)
      continue;

    const char kFieldSeparator = '\t';

    char* start = line_start;
    char* end = (char*)memchr(start, kFieldSeparator, line_end - start);
    if (!end)
      continue;
    *end = 0;

    int start_time = 0, end_time = 0;
    TimeStamp restat_mtime = 0;

    start_time = atoi(start);
    start = end + 1;

    end = (char*)memchr(start, kFieldSeparator, line_end - start);
    if (!end)
      continue;
    *end = 0;
    end_time = atoi(start);
    start = end + 1;

    end = (char*)memchr(start, kFieldSeparator, line_end - start);
    if (!end)
      continue;
    *end = 0;
    restat_mtime = strtoll(start, nullptr, 10);
    start = end + 1;

    end = (char*)memchr(start, kFieldSeparator, line_end - start);
    if (!end)
      continue;
    std::string output = std::string(start, end - start);

    start = end + 1;
    end = line_end;

    LogEntry* entry;
    auto i = entries_.find(output);
    if (i != entries_.end()) {
      entry = i->second;
    } else {
      entry = new LogEntry(output);
      entries_.insert(Entries::value_type(entry->output, entry));
      ++unique_entry_count;
    }
    ++total_entry_count;

    entry->start_time = start_time;
    entry->end_time = end_time;
    entry->mtime = restat_mtime;
    if (log_version >= 5) {
      char c = *end;
      *end = '\0';
      entry->command_hash = (uint64_t)strtoull(start, nullptr, 16);
      *end = c;
    } else {
      entry->command_hash =
          LogEntry::Hash(std::string_view(start, end - start));
    }
  }
  fclose(file);

  if (!line_start) {
    return LOAD_SUCCESS;  // file was empty
  }

  // Decide whether it's time to rebuild the log:
  // - if we're upgrading versions
  // - if it's getting large
  int kMinCompactionEntryCount = 100;
  int kCompactionRatio = 3;
  if (log_version < kCurrentVersion) {
    needs_recompaction_ = true;
  } else if (total_entry_count > kMinCompactionEntryCount &&
             total_entry_count > unique_entry_count * kCompactionRatio) {
    needs_recompaction_ = true;
  }

  return LOAD_SUCCESS;
}

BuildLog::LogEntry* BuildLog::LookupByOutput(const std::string& path) {
  auto i = entries_.find(path);
  if (i != entries_.end())
    return i->second;
  return nullptr;
}

bool BuildLog::WriteEntry(FILE* f, const LogEntry& entry) {
  return fprintf(f, "%d\t%d\t%" PRId64 "\t%s\t%" PRIx64 "\n", entry.start_time,
                 entry.end_time, entry.mtime, entry.output.c_str(),
                 entry.command_hash) > 0;
}

bool BuildLog::Recompact(const std::string& path, const BuildLogUser& user,
                         std::string* err) {
  METRIC_RECORD(".ninja_log recompact");

  Close();
  std::string temp_path = path + ".recompact";
  FILE* f = fopen(temp_path.c_str(), "wb");
  if (!f) {
    *err = strerror(errno);
    return false;
  }

  if (fprintf(f, kFileSignature, kCurrentVersion) < 0) {
    *err = strerror(errno);
    fclose(f);
    return false;
  }

  std::vector<std::string_view> dead_outputs;
  for (auto& entrie : entries_) {
    if (user.IsPathDead(entrie.first)) {
      dead_outputs.push_back(entrie.first);
      continue;
    }

    if (!WriteEntry(f, *entrie.second)) {
      *err = strerror(errno);
      fclose(f);
      return false;
    }
  }

  for (auto& dead_output : dead_outputs)
    entries_.erase(dead_output);

  fclose(f);
  if (unlink(path.c_str()) < 0) {
    *err = strerror(errno);
    return false;
  }

  if (rename(temp_path.c_str(), path.c_str()) < 0) {
    *err = strerror(errno);
    return false;
  }

  return true;
}

bool BuildLog::Restat(const std::string_view path,
                      const DiskInterface& disk_interface,
                      const int output_count, char** outputs,
                      std::string* const err) {
  METRIC_RECORD(".ninja_log restat");

  Close();
  std::string temp_path = std::string{ path } + ".restat";
  FILE* f = fopen(temp_path.c_str(), "wb");
  if (!f) {
    *err = strerror(errno);
    return false;
  }

  if (fprintf(f, kFileSignature, kCurrentVersion) < 0) {
    *err = strerror(errno);
    fclose(f);
    return false;
  }
  for (auto& entrie : entries_) {
    bool skip = output_count > 0;
    for (int j = 0; j < output_count; ++j) {
      if (entrie.second->output == outputs[j]) {
        skip = false;
        break;
      }
    }
    if (!skip) {
      const TimeStamp mtime = disk_interface.Stat(entrie.second->output, err);
      if (mtime == -1) {
        fclose(f);
        return false;
      }
      entrie.second->mtime = mtime;
    }

    if (!WriteEntry(f, *entrie.second)) {
      *err = strerror(errno);
      fclose(f);
      return false;
    }
  }

  fclose(f);
  if (unlink(path.data()) < 0) {
    *err = strerror(errno);
    return false;
  }

  if (rename(temp_path.c_str(), path.data()) < 0) {
    *err = strerror(errno);
    return false;
  }

  return true;
}

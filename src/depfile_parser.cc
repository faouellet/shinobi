/* Generated by re2c 1.1.1 */
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

#include "depfile_parser.h"

#include <algorithm>
#include <cstring>

#include "util.h"

DepfileParser::DepfileParser(DepfileParserOptions options)
    : options_(options) {}

// A note on backslashes in Makefiles, from reading the docs:
// Backslash-newline is the line continuation character.
// Backslash-# escapes a # (otherwise meaningful as a comment start).
// Backslash-% escapes a % (otherwise meaningful as a special).
// Finally, quoting the GNU manual, "Backslashes that are not in danger
// of quoting ‘%’ characters go unmolested."
// How do you end a line with a backslash?  The netbsd Make docs suggest
// reading the result of a shell command echoing a backslash!
//
// Rather than implement all of above, we follow what GCC/Clang produces:
// Backslashes escape a space or hash sign.
// When a space is preceded by 2N+1 backslashes, it is represents N backslashes
// followed by space.
// When a space is preceded by 2N backslashes, it represents 2N backslashes at
// the end of a filename.
// A hash sign is escaped by a single backslash. All other backslashes remain
// unchanged.
//
// If anyone actually has depfiles that rely on the more complicated
// behavior we can adjust this.
bool DepfileParser::Parse(std::string* content, std::string* err) {
  // in: current parser input point.
  // end: end of input.
  // parsing_targets: whether we are parsing targets or dependencies.
  char* in = &(*content)[0];
  char* end = in + content->size();
  bool have_target = false;
  bool parsing_targets = true;
  bool poisoned_input = false;
  while (in < end) {
    bool have_newline = false;
    // out: current output point (typically same as in, but can fall behind
    // as we de-escape backslashes).
    char* out = in;
    // filename: start of the current parsed filename.
    char* filename = out;
    for (;;) {
      // start: beginning of the current parsed span.
      const char* start = in;
      char* yymarker = NULL;

      {
        unsigned char yych;
        static const unsigned char yybm[] = {
          0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
          0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
          0,   0,   0,   0,   0,   128, 0,   0,   0,   128, 0,   0,   128, 128,
          0,   128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
          128, 128, 128, 0,   0,   128, 0,   0,   128, 128, 128, 128, 128, 128,
          128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
          128, 128, 128, 128, 128, 128, 128, 128, 0,   128, 0,   128, 0,   128,
          128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
          128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 0,   128,
          128, 0,   128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
          128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
          128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
          128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
          128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
          128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
          128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
          128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
          128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
          128, 128, 128, 128,
        };
        yych = *in;
        if (yybm[0 + yych] & 128) {
          goto yy9;
        }
        if (yych <= '\r') {
          if (yych <= '\t') {
            if (yych >= 0x01)
              goto yy4;
          } else {
            if (yych <= '\n')
              goto yy6;
            if (yych <= '\f')
              goto yy4;
            goto yy8;
          }
        } else {
          if (yych <= '$') {
            if (yych <= '#')
              goto yy4;
            goto yy12;
          } else {
            if (yych <= '?')
              goto yy4;
            if (yych <= '\\')
              goto yy13;
            goto yy4;
          }
        }
        ++in;
        { break; }
      yy4:
        ++in;
      yy5 : {
        // For any other character (e.g. whitespace), swallow it here,
        // allowing the outer logic to loop around again.
        break;
      }
      yy6:
        ++in;
        {
          // A newline ends the current file name and the current rule.
          have_newline = true;
          break;
        }
      yy8:
        yych = *++in;
        if (yych == '\n')
          goto yy6;
        goto yy5;
      yy9:
        yych = *++in;
        if (yybm[0 + yych] & 128) {
          goto yy9;
        }
      yy11 : {
        // Got a span of plain text.
        int len = (int)(in - start);
        // Need to shift it over if we're overwriting backslashes.
        if (out < start)
          memmove(out, start, len);
        out += len;
        continue;
      }
      yy12:
        yych = *++in;
        if (yych == '$')
          goto yy14;
        goto yy5;
      yy13:
        yych = *(yymarker = ++in);
        if (yych <= 0x1F) {
          if (yych <= '\n') {
            if (yych <= 0x00)
              goto yy5;
            if (yych <= '\t')
              goto yy16;
            goto yy17;
          } else {
            if (yych == '\r')
              goto yy19;
            goto yy16;
          }
        } else {
          if (yych <= '#') {
            if (yych <= ' ')
              goto yy21;
            if (yych <= '"')
              goto yy16;
            goto yy23;
          } else {
            if (yych == '\\')
              goto yy25;
            goto yy16;
          }
        }
      yy14:
        ++in;
        {
          // De-escape dollar character.
          *out++ = '$';
          continue;
        }
      yy16:
        ++in;
        goto yy11;
      yy17:
        ++in;
        {
          // A line continuation ends the current file name.
          break;
        }
      yy19:
        yych = *++in;
        if (yych == '\n')
          goto yy17;
        in = yymarker;
        goto yy5;
      yy21:
        ++in;
        {
          // 2N+1 backslashes plus space -> N backslashes plus space.
          int len = (int)(in - start);
          int n = len / 2 - 1;
          if (out < start)
            memset(out, '\\', n);
          out += n;
          *out++ = ' ';
          continue;
        }
      yy23:
        ++in;
        {
          // De-escape hash sign, but preserve other leading backslashes.
          int len = (int)(in - start);
          if (len > 2 && out < start)
            memset(out, '\\', len - 2);
          out += len - 2;
          *out++ = '#';
          continue;
        }
      yy25:
        yych = *++in;
        if (yych <= 0x1F) {
          if (yych <= '\n') {
            if (yych <= 0x00)
              goto yy11;
            if (yych <= '\t')
              goto yy16;
            goto yy11;
          } else {
            if (yych == '\r')
              goto yy11;
            goto yy16;
          }
        } else {
          if (yych <= '#') {
            if (yych <= ' ')
              goto yy26;
            if (yych <= '"')
              goto yy16;
            goto yy23;
          } else {
            if (yych == '\\')
              goto yy28;
            goto yy16;
          }
        }
      yy26:
        ++in;
        {
          // 2N backslashes plus space -> 2N backslashes, end of filename.
          int len = (int)(in - start);
          if (out < start)
            memset(out, '\\', len - 1);
          out += len - 1;
          break;
        }
      yy28:
        yych = *++in;
        if (yych <= 0x1F) {
          if (yych <= '\n') {
            if (yych <= 0x00)
              goto yy11;
            if (yych <= '\t')
              goto yy16;
            goto yy11;
          } else {
            if (yych == '\r')
              goto yy11;
            goto yy16;
          }
        } else {
          if (yych <= '#') {
            if (yych <= ' ')
              goto yy21;
            if (yych <= '"')
              goto yy16;
            goto yy23;
          } else {
            if (yych == '\\')
              goto yy25;
            goto yy16;
          }
        }
      }
    }

    int len = (int)(out - filename);
    const bool is_dependency = !parsing_targets;
    if (len > 0 && filename[len - 1] == ':') {
      len--;  // Strip off trailing colon, if any.
      parsing_targets = false;
      have_target = true;
    }

    if (len > 0) {
      std::string_view piece = std::string_view(filename, len);
      // If we've seen this as an input before, skip it.
      std::vector<std::string_view>::iterator pos =
          std::find(ins_.begin(), ins_.end(), piece);
      if (pos == ins_.end()) {
        if (is_dependency) {
          if (poisoned_input) {
            *err = "inputs may not also have inputs";
            return false;
          }
          // New input.
          ins_.push_back(piece);
        } else {
          // Check for a new output.
          if (std::find(outs_.begin(), outs_.end(), piece) == outs_.end())
            outs_.push_back(piece);
        }
      } else if (!is_dependency) {
        // We've passed an input on the left side; reject new inputs.
        poisoned_input = true;
      }
    }

    if (have_newline) {
      // A newline ends a rule so the next filename will be a new target.
      parsing_targets = true;
      poisoned_input = false;
    }
  }
  if (!have_target) {
    *err = "expected ':' in depfile";
    return false;
  }
  return true;
}

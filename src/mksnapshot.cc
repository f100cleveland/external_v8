// Copyright 2006-2008 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <signal.h>
#include <string>
#include <map>

#include "v8.h"

#include "bootstrapper.h"
#include "natives.h"
#include "platform.h"
#include "serialize.h"

// use explicit namespace to avoid clashing with types in namespace v8
namespace i = v8::internal;
using namespace v8;

static const unsigned int kMaxCounters = 256;

// A single counter in a counter collection.
class Counter {
 public:
  static const int kMaxNameSize = 64;
  int32_t* Bind(const char* name) {
    int i;
    for (i = 0; i < kMaxNameSize - 1 && name[i]; i++) {
      name_[i] = name[i];
    }
    name_[i] = '\0';
    return &counter_;
  }
 private:
  int32_t counter_;
  uint8_t name_[kMaxNameSize];
};


// A set of counters and associated information.  An instance of this
// class is stored directly in the memory-mapped counters file if
// the --save-counters options is used
class CounterCollection {
 public:
  CounterCollection() {
    magic_number_ = 0xDEADFACE;
    max_counters_ = kMaxCounters;
    max_name_size_ = Counter::kMaxNameSize;
    counters_in_use_ = 0;
  }
  Counter* GetNextCounter() {
    if (counters_in_use_ == kMaxCounters) return NULL;
    return &counters_[counters_in_use_++];
  }
 private:
  uint32_t magic_number_;
  uint32_t max_counters_;
  uint32_t max_name_size_;
  uint32_t counters_in_use_;
  Counter counters_[kMaxCounters];
};


// We statically allocate a set of local counters to be used if we
// don't want to store the stats in a memory-mapped file
static CounterCollection local_counters;


typedef std::map<std::string, int*> CounterMap;
typedef std::map<std::string, int*>::iterator CounterMapIterator;
static CounterMap counter_table_;


class CppByteSink : public i::SnapshotByteSink {
 public:
  explicit CppByteSink(const char* snapshot_file) : bytes_written_(0) {
    fp_ = i::OS::FOpen(snapshot_file, "wb");
    if (fp_ == NULL) {
      i::PrintF("Unable to write to snapshot file \"%s\"\n", snapshot_file);
      exit(1);
    }
    fprintf(fp_, "// Autogenerated snapshot file. Do not edit.\n\n");
    fprintf(fp_, "#include \"v8.h\"\n");
    fprintf(fp_, "#include \"platform.h\"\n\n");
    fprintf(fp_, "#include \"snapshot.h\"\n\n");
    fprintf(fp_, "namespace v8 {\nnamespace internal {\n\n");
    fprintf(fp_, "const byte Snapshot::data_[] = {");
  }

  virtual ~CppByteSink() {
    if (fp_ != NULL) {
      fprintf(fp_, "};\n\n");
      fprintf(fp_, "int Snapshot::size_ = %d;\n\n", bytes_written_);
      fprintf(fp_, "} }  // namespace v8::internal\n");
      fclose(fp_);
    }
  }

  virtual void Put(int byte, const char* description) {
    if (bytes_written_ != 0) {
      fprintf(fp_, ",");
    }
    fprintf(fp_, "%d", byte);
    bytes_written_++;
    if ((bytes_written_ & 0x3f) == 0) {
      fprintf(fp_, "\n");
    }
  }

  virtual int Position() {
    return bytes_written_;
  }

 private:
  FILE* fp_;
  int bytes_written_;
};


int main(int argc, char** argv) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  // By default, log code create information in the snapshot.
  i::FLAG_log_code = true;
#endif
  // Print the usage if an error occurs when parsing the command line
  // flags or if the help flag is set.
  int result = i::FlagList::SetFlagsFromCommandLine(&argc, argv, true);
  if (result > 0 || argc != 2 || i::FLAG_help) {
    ::printf("Usage: %s [flag] ... outfile\n", argv[0]);
    i::FlagList::PrintHelp();
    return !i::FLAG_help;
  }
  i::Serializer::Enable();
  Persistent<Context> context = v8::Context::New();
  ASSERT(!context.IsEmpty());
  // Make sure all builtin scripts are cached.
  { HandleScope scope;
    for (int i = 0; i < i::Natives::GetBuiltinsCount(); i++) {
      i::Bootstrapper::NativesSourceLookup(i);
    }
  }
  context.Dispose();
  CppByteSink sink(argv[1]);
  // This results in a somewhat smaller snapshot, probably because it gets rid
  // of some things that are cached between garbage collections.
  i::Heap::CollectAllGarbage(true);
  i::StartupSerializer ser(&sink);
  ser.Serialize();
  return 0;
}
// Copyright 2025 The QLever Authors, in particular:
//
// 2025 Johannes Kalmbach <kalmbach@cs.uni-freiburg.de>, UFR
//
// UFR = University of Freiburg, Chair of Algorithms and Data Structures

#include <iostream>
#include <memory>
#include <memory_resource>

#include "libqlever/Qlever.h"
#include "util/Exception.h"
#include "util/Timer.h"

static const std::string query = R"(
SELECT (COUNT(*) AS ?count) WHERE {
 ?s ?p ?o
}
)";

class unsynchronized_counting_memory_resource
    : public std::pmr::memory_resource {
 public:
  unsynchronized_counting_memory_resource(
      std::pmr::memory_resource* const upstream)
      : upstream_(upstream) {}

  std::size_t get_allocated_bytes() const { return allocated_bytes_; }

 protected:
  void* do_allocate(std::size_t bytes, std::size_t alignment) override {
    allocated_bytes_ += bytes;
    return upstream_->allocate(bytes, alignment);
  }

  void do_deallocate(void* p, std::size_t bytes,
                     std::size_t alignment) override {
    allocated_bytes_ -= bytes;
    upstream_->deallocate(p, bytes, alignment);
  }

  bool do_is_equal(
      const std::pmr::memory_resource& other) const noexcept override {
    return this == &other;
  }

 private:
  std::size_t allocated_bytes_ = 0;
  std::pmr::memory_resource* upstream_;
};

int main(int argc, char** argv) {
  // Parse command line arguments.
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " <input file> <index basename>"
              << std::endl;
    return 1;
  }
  std::string inputFile = argv[1];
  std::string indexBasename = argv[2];

  // Build index for the given input file and write index files to disk.
  std::cout << "\x1b[1mBuilding index for input file \"" << inputFile << "\""
            << " with basename \"" << indexBasename << "\x1b[0m" << std::endl;
  qlever::IndexBuilderConfig config;
  config.inputFiles_.push_back(
      {inputFile, qlever::Filetype::Turtle, std::nullopt});
  config.baseName_ = indexBasename;
  try {
    qlever::Qlever::buildIndex(config);
  } catch (const std::exception& e) {
    std::cerr << "Building the index failed: " << e.what() << std::endl;
    return 1;
  }
  std::cout << std::endl;

  // Load index.
  std::cout << "\x1b[1mLoading index with basename \"" << indexBasename
            << "\"\x1b[0m" << std::endl;

  qlever::EngineConfig engineConfig{config};

  constexpr std::size_t bufferSize = 128 * 1024 * 1024;
  auto buffer = std::make_unique<std::byte[]>(bufferSize);
  std::pmr::monotonic_buffer_resource monotonicResource{buffer.get(),
                                                        bufferSize};
  std::pmr::unsynchronized_pool_resource poolResource{&monotonicResource};

  unsynchronized_counting_memory_resource countingResource{&poolResource};
  qlever::Qlever qlever{engineConfig, &countingResource};
  std::cout << std::endl;

  // Execute query.
  std::cout << "\x1b[1mExecuting test query" << "\x1b[0m" << std::endl;
  std::string queryResult;
  ad_utility::Timer timer{ad_utility::Timer::Started};
  try {
    queryResult = qlever.query(query);
  } catch (const std::exception& e) {
    std::cerr << "Executing the query failed: " << e.what() << std::endl;
    return 1;
  }
  std::cout << "Query executed in " << timer.msecs().count() << "ms"
            << std::endl;
  std::cout << "Mem allocated: " << countingResource.get_allocated_bytes()
            << " bytes" << std::endl;
  std::cout << std::endl;

  // Show result.
  std::cout << "\x1b[1mResult string is:\x1b[0m" << std::endl;
  std::cout << queryResult << std::endl;
  std::cout << std::endl;
}

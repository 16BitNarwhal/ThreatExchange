#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pdq/cpp/index/mih.h>
#include <pdq/cpp/io/hashio.h>
#include <pdq/cpp/common/pdqutils.h>

#include <algorithm>
#include <random>
#include <set>

// ================================================================
// Static function declarations
static void usage(char* argv0, int rc);
static void query(char* argv0, int argc, char** argv);

// Function declarations for each query method
static void queryLinear(
    const int maxDistance,
    const bool verbose,
    const unsigned int seed,
    const size_t indexSize,
    const size_t querySize,
    const std::vector<std::pair<facebook::pdq::hashing::Hash256, std::string>>&
        queries,
    const std::vector<std::pair<facebook::pdq::hashing::Hash256, std::string>>&
        index);
static void queryMIH(
    const int maxDistance,
    const bool verbose,
    const unsigned int seed,
    const size_t indexSize,
    const size_t querySize,
    const std::vector<std::pair<facebook::pdq::hashing::Hash256, std::string>>&
        queries,
    const std::vector<std::pair<facebook::pdq::hashing::Hash256, std::string>>&
        index);

// ----------------------------------------------------------------
int main(int argc, char** argv) {
  if (argc > 1 && (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help"))) {
    usage(argv[0], 0);
  } else {
    query(argv[0], argc - 1, argv + 1);
  }
  return 0;
}

// ----------------------------------------------------------------
static void usage(char* argv0, int rc) {
  FILE* fp = (rc == 0) ? stdout : stderr;
  fprintf(fp, "Usage: %s [options]\n", argv0);
  fprintf(fp, "Options:\n");
  fprintf(fp, "  -v               Verbose output\n");
  fprintf(fp, "  --seed N         Random seed (default: 41)\n");
  fprintf(fp, "  -q N             Number of queries to run (default: 1000)\n");
  fprintf(
      fp,
      "  -b N             Number of PDQ hashes to query against (default: 10000)\n");
  fprintf(
      fp,
      "  -d N             Maximum Hamming distance for matches (default: 31)\n");
  fprintf(
      fp,
      "  -m               Method for querying (default: linear), Available: linear, mih\n");
  exit(rc);
}

static void query(char* argv0, int argc, char** argv) {
  int maxDistance = 31;
  bool verbose = false;
  unsigned int seed = 41;
  size_t indexSize = 10000;
  size_t querySize = 1000;
  std::string method = "linear";

  // Parse command line arguments
  for (int i = 0; i < argc; i++) {
    std::string arg = argv[i];
    if (arg == "-q") {
      if (i + 1 < argc) {
        querySize = std::stoi(argv[++i]);
      } else {
        fprintf(stderr, "Error: Missing argument for -q\n");
        usage(argv0, 1);
        return;
      }
    } else if (arg == "-b") {
      if (i + 1 < argc) {
        indexSize = std::stoi(argv[++i]);
      } else {
        fprintf(stderr, "Error: Missing argument for -b\n");
        usage(argv0, 1);
        return;
      }
    } else if (arg == "-d") {
      if (i + 1 < argc) {
        maxDistance = std::stoi(argv[++i]);
      } else {
        fprintf(stderr, "Error: Missing argument for -d\n");
        usage(argv0, 1);
        return;
      }
    } else if (arg == "--seed") {
      if (i + 1 < argc) {
        seed = std::stoi(argv[++i]);
      } else {
        fprintf(stderr, "Error: Missing argument for --seed\n");
        usage(argv0, 1);
        return;
      }
    } else if (arg == "-m") {
      if (i + 1 < argc) {
        std::string methodName = argv[++i];
        if (methodName == "linear" || methodName == "mih") {
          method = methodName;
        } else {
          fprintf(stderr, "Invalid method: %s\n", methodName.c_str());
          usage(argv0, 1);
          return;
        }
      }
    } else if (arg == "-v") {
      verbose = true;
    } else if (arg == "-h" || arg == "--help") {
      usage(argv0, 0);
      return;
    } else if (arg.length() > 0) {
      fprintf(stderr, "Unknown argument: %s\n", arg.c_str());
      usage(argv0, 1);
      return;
    }
  }

  // Initialize random number generator
  std::mt19937 gen(seed);

  // Generate random hashes for queries
  std::vector<std::pair<facebook::pdq::hashing::Hash256, std::string>> queries;
  for (size_t i = 0; i < querySize; i++) {
    auto hash = facebook::pdq::hashing::generateRandomHash(gen);
    queries.push_back({hash, "query_" + std::to_string(i)});
  }

  // Generate random hashes for index
  std::vector<std::pair<facebook::pdq::hashing::Hash256, std::string>> index;
  for (size_t i = 0; i < indexSize - querySize; i++) {
    auto hash = facebook::pdq::hashing::generateRandomHash(gen);
    index.push_back({hash, "index_" + std::to_string(i)});
  }

  // Add noise to queries then insert into index
  std::uniform_int_distribution<int> noiseDist(1, maxDistance);
  for (const auto& query : queries) {
    int bitsToFlip = noiseDist(gen);
    auto noisyHash = facebook::pdq::hashing::addNoise(query.first, bitsToFlip, gen);
    index.push_back({noisyHash, "index_noisy_" + query.second});
  }
  std::shuffle(index.begin(), index.end(), gen);

  if (verbose) {
    printf("GENERATED QUERIES:\n");
    for (const auto& it : queries) {
      printf("%s,%s\n", it.first.format().c_str(), it.second.c_str());
    }
    printf("\n");

    printf("GENERATED INDEX:\n");
    for (const auto& it : index) {
      printf("%s,%s\n", it.first.format().c_str(), it.second.c_str());
    }
    printf("\n");
  }

  if (method == "linear") {
    queryLinear(
        maxDistance, verbose, seed, indexSize, querySize, queries, index);
  } else if (method == "mih") {
    queryMIH(maxDistance, verbose, seed, indexSize, querySize, queries, index);
  } else {
    fprintf(stderr, "Unknown method: %s\n", method.c_str());
    usage(argv0, 1);
    return;
  }
}

///////////////////////
//// Query methods ////
///////////////////////

static void queryLinear(
    const int maxDistance,
    const bool verbose,
    const unsigned int seed,
    const size_t indexSize,
    const size_t querySize,
    const std::vector<std::pair<facebook::pdq::hashing::Hash256, std::string>>&
        queries,
    const std::vector<std::pair<facebook::pdq::hashing::Hash256, std::string>>&
        index) {
  // Do linear searches
  std::vector<std::pair<facebook::pdq::hashing::Hash256, std::string>> matches;

  Timer queryTimer("Linear query", verbose);
  for (const auto& it : queries) {
    for (const auto& it2 : index) {
      if (it.first.hammingDistance(it2.first) <= maxDistance) {
        matches.push_back(it2);
      }
    }
  }
  double seconds = queryTimer.elapsed();

  printf("METHOD: Linear query\n"); // TODO: dont make people rewrite this a buncha times
  printf("QUERY COUNT:             %d\n", (int)queries.size());
  printf("INDEX COUNT:             %d\n", (int)index.size());
  printf("TOTAL MATCH COUNT:       %d\n", (int)matches.size());
  printf("TOTAL QUERY SECONDS:     %.6lf\n", seconds);
  printf(
      "SECONDS PER QUERY:       %.6lf\n",
      querySize > 0 ? seconds / querySize : 0);
  printf("\n");
}

static void queryMIH(
    const int maxDistance,
    const bool verbose,
    const unsigned int seed,
    const size_t indexSize,
    const size_t querySize,
    const std::vector<std::pair<facebook::pdq::hashing::Hash256, std::string>>&
        queries,
    const std::vector<std::pair<facebook::pdq::hashing::Hash256, std::string>>&
        index) {
  // Build the MIH
  facebook::pdq::index::MIH256<std::string> mih;

  Timer indexTimer("Building MIH", verbose);
  for (const auto& it : index) {
    mih.insert(it.first, it.second);
  }
  double indexTimeSeconds = indexTimer.elapsed();

  printf("\n");
  if (verbose) {
    printf("\n");
    mih.dump();
    printf("\n");
  }

  // Do indexed searches
  std::vector<std::pair<facebook::pdq::hashing::Hash256, std::string>> matches;
  matches.clear();

  Timer queryTimer("MIH query", verbose);
  for (const auto& it : queries) {
    mih.queryAll(it.first, maxDistance, matches);
  }
  double seconds = queryTimer.elapsed();

  printf("METHOD: Mutually-indexed hashing query\n");
  printf("INDEX BUILD SECONDS:     %.6lf\n", indexTimeSeconds);
  printf("QUERY COUNT:             %d\n", (int)queries.size());
  printf("INDEX COUNT:             %d\n", (int)mih.size());
  printf("TOTAL MATCH COUNT:       %d\n", (int)matches.size());
  printf("TOTAL QUERY SECONDS:     %.6lf\n", seconds);
  printf(
      "SECONDS PER QUERY:       %.6lf\n",
      querySize > 0 ? seconds / querySize : 0);
  printf("\n");
}

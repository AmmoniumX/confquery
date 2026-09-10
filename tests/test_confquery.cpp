#include "doctest/doctest.h"

#include "confquery.hpp"

#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <stdlib.h>
#include <unistd.h>

using namespace confquery;

namespace {

// Drives ConfigDataView::parse() the same way parseFile() does, but from an
// in-memory list of lines so tests don't need to touch the filesystem.
ConfigDataView parse_lines(const std::vector<std::string> &input_lines,
                            std::ostream &err = std::cerr) {
  ConfigDataView config{err};
  int line_num = 0;
  for (const auto &line : input_lines) {
    bool ok = config.parse(line, ++line_num);
    REQUIRE(ok);
  }
  config.end();
  return config;
}

std::string serialize(const ConfigDataView &config) {
  std::ostringstream oss;
  oss << config;
  return oss.str();
}

// RAII helper for tests that need a real file on disk (parseFile()).
struct TempFile {
  int fd = -1;
  fs::path path;

  explicit TempFile(std::string_view content) {
    std::string templ =
        (fs::temp_directory_path() / "confquery_doctest_XXXXXX").string();
    fd = mkstemp(templ.data());
    if (fd == -1) {
      throw std::runtime_error("mkstemp() failed");
    }
    path = templ;
    write(fd, content.data(), content.size());
  }

  ~TempFile() {
    if (fd != -1) {
      close(fd);
    }
    std::error_code ec;
    fs::remove(path, ec);
  }

  TempFile(const TempFile &) = delete;
  TempFile &operator=(const TempFile &) = delete;
};

} // namespace

TEST_CASE("parse preserves comments, blank lines, and section structure") {
  auto config = parse_lines({
      "# a comment",
      "",
      "[options]",
      "CheckSpace",
      "Foo = Bar",
  });

  CHECK(config.has_section_header("[options]"));
  CHECK(serialize(config) == "# a comment\n\n[options]\nCheckSpace\nFoo = Bar\n");
}

TEST_CASE("parse extracts key/value and bare value entries correctly") {
  auto config = parse_lines({
      "[options]",
      "CheckSpace",
      "ParallelDownloads = 16",
  });

  const auto *value = config.get_value_entry("[options]", "CheckSpace");
  REQUIRE(value != nullptr);
  CHECK(value->value() == "CheckSpace");

  const auto *kv = config.get_key_value_pair("[options]", "ParallelDownloads");
  REQUIRE(kv != nullptr);
  CHECK(kv->key() == "ParallelDownloads");
  CHECK(kv->value() == "16");
}

TEST_CASE("get_section / has_section_header") {
  auto config = parse_lines({"[options]", "CheckSpace"});

  CHECK(config.has_section_header("[options]"));
  CHECK_FALSE(config.has_section_header("[missing]"));

  const auto *section = config.get_section("[options]");
  REQUIRE(section != nullptr);
  CHECK(section->name == "[options]");
  CHECK(config.get_section("[missing]") == nullptr);
}

TEST_CASE("has_value_entry / has_key_value_pair reflect lookups") {
  auto config =
      parse_lines({"[options]", "CheckSpace", "ParallelDownloads = 16"});

  CHECK(config.has_value_entry("[options]", "CheckSpace"));
  CHECK_FALSE(config.has_value_entry("[options]", "NoSuchValue"));
  CHECK(config.has_key_value_pair("[options]", "ParallelDownloads"));
  CHECK_FALSE(config.has_key_value_pair("[options]", "NoSuchKey"));
}

TEST_CASE("set_value on an existing section appends without duplicating") {
  auto config = parse_lines({"[options]", "CheckSpace"});

  config.set_value("[options]", "ILoveCandy");
  CHECK(config.has_value_entry("[options]", "ILoveCandy"));
  CHECK(serialize(config) == "[options]\nCheckSpace\nILoveCandy\n");

  // Setting an already-present value is a no-op.
  config.set_value("[options]", "ILoveCandy");
  CHECK(serialize(config) == "[options]\nCheckSpace\nILoveCandy\n");
}

TEST_CASE("set_value on a brand-new section creates it with the correct value") {
  auto config = parse_lines({"[options]", "CheckSpace"});

  config.set_value("[multilib]", "hello");

  const auto *value = config.get_value_entry("[multilib]", "hello");
  REQUIRE(value != nullptr);
  CHECK(value->value() == "hello");
  CHECK(serialize(config) ==
        "[options]\nCheckSpace\n[multilib]\nhello\n");
}

TEST_CASE("set_key_value replaces an existing key's value in place") {
  auto config = parse_lines({"[options]", "ParallelDownloads = 5"});

  config.set_key_value("[options]", "ParallelDownloads", "16");

  const auto *kv = config.get_key_value_pair("[options]", "ParallelDownloads");
  REQUIRE(kv != nullptr);
  CHECK(kv->value() == "16");
  CHECK(serialize(config) == "[options]\nParallelDownloads = 16\n");
}

TEST_CASE("set_key_value on an existing section without the key appends it") {
  auto config = parse_lines({"[options]", "CheckSpace"});

  config.set_key_value("[options]", "ParallelDownloads", "16");

  CHECK(serialize(config) ==
        "[options]\nCheckSpace\nParallelDownloads = 16\n");
}

TEST_CASE("set_key_value on a brand-new section writes 'key = value'") {
  auto config = parse_lines({"[options]", "CheckSpace"});

  config.set_key_value("[multilib]", "Include", "/etc/pacman.d/mirrorlist");

  const auto *kv = config.get_key_value_pair("[multilib]", "Include");
  REQUIRE(kv != nullptr);
  CHECK(kv->key() == "Include");
  CHECK(kv->value() == "/etc/pacman.d/mirrorlist");
  CHECK(serialize(config) ==
        "[options]\nCheckSpace\n[multilib]\nInclude = "
        "/etc/pacman.d/mirrorlist\n");
}

TEST_CASE("remove_section removes the section and all of its lines") {
  auto config = parse_lines(
      {"[options]", "CheckSpace", "[multilib]", "Include = mirrorlist"});

  CHECK(config.remove_section("[multilib]"));
  CHECK_FALSE(config.has_section_header("[multilib]"));
  CHECK(serialize(config) == "[options]\nCheckSpace\n");

  CHECK_FALSE(config.remove_section("[nonexistent]"));
}

TEST_CASE("remove_value removes only the matching value") {
  auto config =
      parse_lines({"[options]", "CheckSpace", "ILoveCandy", "VerbosePkgLists"});

  CHECK(config.remove_value("[options]", "ILoveCandy"));
  CHECK_FALSE(config.has_value_entry("[options]", "ILoveCandy"));
  CHECK(config.has_value_entry("[options]", "CheckSpace"));
  CHECK(config.has_value_entry("[options]", "VerbosePkgLists"));

  CHECK_FALSE(config.remove_value("[options]", "ILoveCandy"));
}

TEST_CASE("remove_key removes only the matching key") {
  auto config = parse_lines(
      {"[options]", "ParallelDownloads = 16", "CleanMethod = KeepCurrent"});

  CHECK(config.remove_key("[options]", "ParallelDownloads"));
  CHECK_FALSE(config.has_key_value_pair("[options]", "ParallelDownloads"));
  CHECK(config.has_key_value_pair("[options]", "CleanMethod"));

  CHECK_FALSE(config.remove_key("[options]", "ParallelDownloads"));
}

TEST_CASE("parse reports errors through the configured error stream") {
  SUBCASE("section header missing closing bracket") {
    std::ostringstream err;
    ConfigDataView config{err};
    CHECK_FALSE(config.parse("[unclosed", 1));
    CHECK_FALSE(err.str().empty());
  }

  SUBCASE("key-value entry outside a section") {
    std::ostringstream err;
    ConfigDataView config{err};
    CHECK_FALSE(config.parse("key = value", 1));
    CHECK_FALSE(err.str().empty());
  }

  SUBCASE("value entry outside a section") {
    std::ostringstream err;
    ConfigDataView config{err};
    CHECK_FALSE(config.parse("orphanvalue", 1));
    CHECK_FALSE(err.str().empty());
  }

  SUBCASE("unrecognized line shape") {
    std::ostringstream err;
    ConfigDataView config{err};
    CHECK_FALSE(config.parse("foo bar", 1));
    CHECK_FALSE(err.str().empty());
  }
}

TEST_CASE("parseFile reads a real file from disk") {
  TempFile file{"[options]\nCheckSpace\nParallelDownloads = 16\n"};

  auto result = parseFile(file.path);
  REQUIRE(result.has_value());
  CHECK(result->has_value_entry("[options]", "CheckSpace"));
  CHECK(result->has_key_value_pair("[options]", "ParallelDownloads"));
}

TEST_CASE("parseFile returns EOPEN for a nonexistent file") {
  auto result = parseFile("/nonexistent/path/confquery_doctest.conf");
  REQUIRE_FALSE(result.has_value());
  CHECK(result.error() == ExitCode::EOPEN);
}

TEST_CASE("parseFile returns EPARSE for malformed content") {
  TempFile file{"[unclosed\n"};

  auto result = parseFile(file.path);
  REQUIRE_FALSE(result.has_value());
  CHECK(result.error() == ExitCode::EPARSE);
}

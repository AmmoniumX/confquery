#pragma once

#include <cstddef>
#include <expected>
#include <filesystem>
#include <iostream>
#include <optional>
#include <ostream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

// Linux INI config file parser and query tool
// Using https://linuxcnc.org/docs/html/config/ini-config.html as format
// reference

namespace confquery {

enum class ExitCode : int {
  SUCCESS = 0,
  FAILURE = 1,
  EARGS = 2,
  EOPEN = 3,
  EPARSE = 4,
  EINTERNAL = 5,
};

class ConfigDataView {
public:
  // This is for the actual file format, we preserve comments
  struct SectionHeader {
    std::string line;
  };

  struct Comment {
    std::string line;
  };

  struct KeyValueEntry {
    std::string line;
    size_t keyLen, valueStart, valueLen;

    std::string_view key() const {
      return std::string_view(line).substr(0, keyLen);
    }
    std::string_view value() const {
      return std::string_view(line).substr(valueStart, valueLen);
    }

    static KeyValueEntry create(std::string key, std::string value);
  };

  struct ValueEntry {
    std::string line;
    size_t valueLen;

    std::string_view value() const {
      return std::string_view(line).substr(0, valueLen);
    }

    static ValueEntry create(std::string value);
  };

  using Line = std::variant<SectionHeader, KeyValueEntry, ValueEntry, Comment>;

  // This is for simple lookup for sections
  struct Section {
    std::string name;
    using Entry = std::variant<KeyValueEntry, ValueEntry>;
    std::vector<Entry> entries;
  };

  std::vector<Line> lines;
  std::vector<Section> sections;

  std::span<Line> as_span() { return {lines.begin(), lines.end()}; }
  std::span<const Line> as_span() const { return {lines.begin(), lines.end()}; }

  static std::string to_string(const Line &line);
  static std::string to_string(const Section::Entry &entry);

  explicit ConfigDataView(std::ostream &err_stream = std::cerr);

  friend std::ostream &operator<<(std::ostream &out,
                                  const ConfigDataView &data);

  // Returns true if parse success, false if error
  bool parse(std::string line, int line_num);

  void end();

  const Section *get_section(std::string_view section_name) const;
  bool has_section_header(std::string_view section_name) const;

  const ValueEntry *get_value_entry(std::string_view section_name,
                                    std::string_view value_name) const;
  bool has_value_entry(std::string_view section, std::string_view value) const;

  const KeyValueEntry *get_key_value_pair(std::string_view section_name,
                                          std::string_view key_name) const;
  bool has_key_value_pair(std::string_view section, std::string_view key) const;

  void set_value(std::string_view section_name, std::string_view value);

  void set_key_value(std::string_view section_name, std::string_view key,
                     std::string_view value);

  bool remove_section(std::string_view section_name);
  bool remove_value(std::string_view section_name, std::string_view value_name);
  bool remove_key(std::string_view section_name, std::string_view key_name);

private:
  std::optional<Section> current_section;
  std::ostream &err = std::cerr;

  static std::string_view trim_right(std::string_view sv);
  static bool has_whitespace(std::string_view sv);

  static std::vector<Section>::iterator
  find_section(std::vector<Section> &sections, std::string_view search_name);

  static std::vector<Section>::const_iterator
  find_section(const std::vector<Section> &sections,
               std::string_view search_name);

  static std::vector<Line>::iterator
  find_section_header(std::vector<Line> &lines, std::string_view section_name);

  static std::span<Line>::iterator
  find_section_header(std::span<Line> lines, std::string_view section_name);

  static std::span<const Line>::const_iterator
  find_section_header(std::span<const Line> lines,
                      std::string_view section_name);

  static std::span<Line> find_section_subspan(std::span<Line> lines,
                                              std::string_view section_name);

  template <typename T>
  static void append_to_section(std::vector<Line> &lines,
                                std::vector<Section>::iterator section_it,
                                std::string_view section_name, T entry) {
    // First add to section vector
    section_it->entries.emplace_back(entry);

    // Then find in lines
    // Search for entire subspan belonging to section
    auto lines_span = std::span(lines.begin(), lines.end());
    auto subspan = find_section_subspan(lines_span, section_name);
    if (subspan.begin() == lines_span.end()) {
      throw std::runtime_error(
          "find_section_subspan cannot fail within append_to_section");
    }

    // Calculate the index as the last element of subspan
    auto index = std::distance(lines_span.begin(), subspan.end());

    // Insert into the vector using a vector iterator
    lines.emplace(lines.begin() + index, entry);
  }
};

namespace fs = std::filesystem;

std::expected<ConfigDataView, ExitCode> parseFile(const fs::path &path);

} // namespace confquery

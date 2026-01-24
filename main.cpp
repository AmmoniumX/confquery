#include <algorithm>
#include <cctype>
#include <expected>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <ostream>
#include <print>
#include <ranges>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

// Linux INI config file parser and query tool
// Using https://linuxcnc.org/docs/html/config/ini-config.html as format
// reference

using std::string, std::string_view;

enum class ExitCode : int {
  SUCCESS = 0,
  FAILURE = 1,
  EARGS = 2,
  EOPEN = 3,
  EPARSE = 4,
};

class ConfigDataView {
public:
  static constexpr int parse_error = std::to_underlying(ExitCode::EPARSE);

  // This is for the actual file format, we preserve comments
  struct SectionHeader {
    string line;
  };

  struct Comment {
    string line;
  };

  struct KeyValueEntry {
    string line;
    size_t keyEnd, valueStart, valueEnd;

    string_view key() const { return string_view(line).substr(0, keyEnd); }
    string_view value() const {
      return string_view(line).substr(valueStart, valueEnd);
    }

    static KeyValueEntry create(string key, string value) {
      size_t kEnd = key.length();
      size_t vStart = kEnd + 3;
      return {
          .line = std::format("{} = {}", key, value),
          .keyEnd = kEnd,
          .valueStart = vStart,
          .valueEnd = value.length() // Note: substr length vs end index
      };
    }
  };

  struct ValueEntry {
    string line;
    size_t valueEnd;

    string_view value() const { return string_view(line).substr(0, valueEnd); }
  };

  using Line = std::variant<SectionHeader, KeyValueEntry, ValueEntry, Comment>;

  // This is for simple lookup for sections
  struct Section {
    string name;
    using Entry = std::variant<KeyValueEntry, ValueEntry>;
    std::vector<Entry> entries;
  };

  std::vector<Line> lines;
  std::vector<Section> sections;

  std::span<Line> as_span() { return {lines.begin(), lines.end()}; }

  static string to_string(const Line &line) {
    return std::visit([](const auto &val) { return val.line; }, line);
  }

  static string to_string(const Section::Entry &entry) {
    return std::visit([](const auto &val) { return val.line; }, entry);
  }

  explicit ConfigDataView() : lines{}, sections{}, current_section{} {}

  friend std::ostream &operator<<(std::ostream &out,
                                  const ConfigDataView &data) {
    for (const auto line : data.lines) {
      out << to_string(line) << '\n';
    }
    return out;
  }

  // Returns 0 if parse is a success, else parse_error
  int parse(std::string &&line, int line_num) {

    // Parse comment lines
    if (line.empty() || line.starts_with("#") || line.starts_with(";")) {
      lines.emplace_back(Comment{std::move(line)});
      return 0;
    }

    // We make a view substring with right whitespace trimmed for parsing, but
    // use original line for preserving original structure
    auto tline = trim_right(line);

    // Parse Section Header
    if (tline[0] == '[') {
      if (tline.back() != ']') {
        std::println(std::cerr,
                     "[{}] Expecting ']' as last character to close section, "
                     "found '{}':\n{}",
                     line_num, tline.back(), line);
        return parse_error;
      }

      // Create a new section, adding the current one to the list only if it's
      // not empty
      if (current_section.has_value()) {
        sections.emplace_back(std::move(*current_section));
      }

      // Add line to new Section
      current_section = Section{std::string{tline}, {}};

      // Create SectionHeader, we can move line now
      lines.emplace_back(SectionHeader{std::move(line)});

      return 0;
    }

    // Parse KeyValueEntry
    if (const auto pos = tline.find(" = "); pos != tline.npos) {
      if (!current_section.has_value()) {
        std::println(
            std::cerr,
            "[{}] Found Key-Value entry outside of a section definition:\n{}",
            line_num, line);
        return parse_error;
      }

      // Construct KeyValueEntry with the original (moved) line, pos are correct
      // since we only trim the end
      auto keyEnd = trim_right(string_view{line}.substr(0, pos)).size();
      auto entry = KeyValueEntry{.line = std::move(line),
                                 .keyEnd = keyEnd,
                                 .valueStart = pos + 3,
                                 .valueEnd = tline.size()};

      // Store entry to lines and current_section
      lines.emplace_back(entry);
      current_section->entries.emplace_back(std::move(entry));

      return 0;
    }

    // Parse ValueEntry
    if (!has_whitespace(tline)) {
      auto entry = ValueEntry{std::move(line), tline.size()};
      lines.emplace_back(entry);
      current_section->entries.emplace_back(std::move(entry));
      return 0;
    }

    // Unknown line
    std::println(std::cerr, "[{}] Unknown line type:\n{}", line_num, line);
    return parse_error;
  }

  void end() {
    // Add current section to the list only if it's not empty
    if (current_section.has_value()) {
      sections.emplace_back(std::move(*current_section));
    }
  }

  std::optional<ValueEntry> get_value_entry(std::string_view section_name,
                                            std::string_view value_name) const {
    const auto section_it = find_section(this->sections, section_name);

    if (section_it != sections.end()) {
      for (const auto &entry : section_it->entries) {
        if (const auto *ve = std::get_if<ValueEntry>(&entry)) {
          if (ve->value() == value_name) {
            return *ve;
          }
        }
      }
    }

    return std::nullopt;
  }

  bool has_value_entry(string_view section, string_view value) const {
    return get_value_entry(section, value).has_value();
  }

  std::optional<KeyValueEntry> get_key_value_pair(string_view section_name,
                                                  string_view key_name) const {
    const auto section_it = find_section(this->sections, section_name);

    if (section_it != sections.end()) {
      for (const auto &entry : section_it->entries) {
        if (const auto *kv = std::get_if<KeyValueEntry>(&entry)) {
          if (kv->key() == key_name) {
            return *kv;
          }
        }
      }
    }

    return std::nullopt;
  }

  bool has_key_value_pair(string_view section, string_view key) const {
    return get_key_value_pair(section, key).has_value();
  }

  void set_value(string_view section_name, string_view value) {
    const auto section_it = find_section(this->sections, section_name);

    if (section_it == this->sections.end()) {
      // Add new section
      this->sections.emplace_back(
          Section{std::string{section_name}, {ValueEntry{std::string{value}}}});
      // Add new lines for it
      this->lines.emplace_back(SectionHeader{std::string{section_name}});
      this->lines.emplace_back(ValueEntry{std::string{value}});
    }
    for (const auto &entry : section_it->entries) {
      if (const auto *v = std::get_if<ValueEntry>(&entry)) {
        // do nothing if we already have it
        if (v->value() == value) {
          return;
        }
      }
    }
    // If we don't have it, add it
    append_to_section(this->lines, section_it, section_name,
                      ValueEntry{std::string{value}});
  }

  void set_key_value(string_view section_name, string_view key,
                     string_view value) {
    const auto section_it = find_section(this->sections, section_name);

    if (section_it == this->sections.end()) {
      // Add new section
      this->sections.emplace_back(Section{
          std::string{section_name},
          {KeyValueEntry::create(std::string{key}, std::string{value})}});
      // Add new lines for it
      this->lines.emplace_back(SectionHeader{std::string{section_name}});
      this->lines.emplace_back(ValueEntry{std::string{value}});
    }
    for (auto &entry : section_it->entries) {
      if (auto *kv = std::get_if<KeyValueEntry>(&entry)) {
        // replace the value if we have the key
        if (kv->key() == key) {
          *kv = KeyValueEntry::create(std::string{key}, std::string{value});
          // Locate the lines entry as well and modify it
          auto lines_sp = as_span();
          auto section_subsp = find_section_subspan(lines_sp, section_name);
          if (section_subsp.begin() == lines_sp.end()) {
            throw std::runtime_error(
                "find_section_subspan cannot fail within set_key_value");
          }
          auto line = std::find_if(
              section_subsp.begin(), section_subsp.end(), [key](auto &l) {
                if (auto l_kv = std::get_if<KeyValueEntry>(&l)) {
                  return l_kv->key() == key;
                }
                return false;
              });
          if (line == section_subsp.end()) {
            throw std::runtime_error(
                "line search cannot fail within set_key_value");
          }

          // Finally replace the line at the same index
          auto index = std::distance(lines_sp.begin(), line);
          this->lines[index] =
              KeyValueEntry::create(std::string{key}, std::string{value});
          return;
        }
      }
    }
    // If we don't have it, add it
    append_to_section(
        this->lines, section_it, section_name,
        KeyValueEntry::create(std::string{key}, std::string{value}));
  }

  bool remove_value(string_view section_name, string_view value_name) {

    // Locate within sections first
    auto section_it = find_section(this->sections, section_name);
    if (section_it == this->sections.end()) {
      return false;
    }
    auto value_it =
        std::find_if(section_it->entries.begin(), section_it->entries.end(),
                     [value_name](auto &entry) {
                       if (auto *v = std::get_if<ValueEntry>(&entry)) {
                         return v->value() == value_name;
                       }
                       return false;
                     });
    if (value_it == section_it->entries.end()) {
      return false;
    }
    // Eliminate from section
    section_it->entries.erase(value_it);

    // Then locate on lines
    auto lines_sp = as_span();
    auto section_subspan = find_section_subspan(lines_sp, section_name);
    if (section_subspan.begin() == lines_sp.end()) {
      throw std::runtime_error(
          "find_section_subspan cannot fail within remove_value at this point");
    }
    auto line_value_it =
        std::find_if(section_subspan.begin(), section_subspan.end(),
                     [value_name](auto &entry) {
                       if (auto *v = std::get_if<ValueEntry>(&entry)) {
                         return v->value() == value_name;
                       }
                       return false;
                     });
    if (line_value_it == section_subspan.end()) {
      throw std::runtime_error(
          "remove_value cannot fail to find line_value_it at this point");
    }

    // Eliminate from lines
    auto offset = std::distance(lines_sp.begin(), line_value_it);
    this->lines.erase(this->lines.begin() + offset);

    return true;
  }

  bool remove_key(string_view section_name, string_view key_name) {

    // Locate within sections first
    auto section_it = find_section(this->sections, section_name);
    if (section_it == this->sections.end()) {
      return false;
    }
    auto value_it =
        std::find_if(section_it->entries.begin(), section_it->entries.end(),
                     [key_name](auto &entry) {
                       if (auto *v = std::get_if<KeyValueEntry>(&entry)) {
                         return v->key() == key_name;
                       }
                       return false;
                     });
    if (value_it == section_it->entries.end()) {
      return false;
    }
    // Eliminate from section
    section_it->entries.erase(value_it);

    // Then locate on lines
    auto lines_sp = as_span();
    auto section_subspan = find_section_subspan(lines_sp, section_name);
    if (section_subspan.begin() == lines_sp.end()) {
      throw std::runtime_error(
          "find_section_subspan cannot fail within remove_key at this point");
    }
    auto line_value_it =
        std::find_if(section_subspan.begin(), section_subspan.end(),
                     [key_name](auto &entry) {
                       if (auto *v = std::get_if<KeyValueEntry>(&entry)) {
                         return v->key() == key_name;
                       }
                       return false;
                     });
    if (line_value_it == section_subspan.end()) {
      throw std::runtime_error(
          "remove_key cannot fail to find line_value_it at this point");
    }

    // Eliminate from lines
    auto offset = std::distance(lines_sp.begin(), line_value_it);
    this->lines.erase(this->lines.begin() + offset);

    return true;
  }

private:
  std::optional<Section> current_section;

  static std::string_view trim_right(std::string_view sv) {
    // Find the first non-whitespace character starting from the back
    auto it =
        std::ranges::find_if(sv | std::views::reverse, [](unsigned char ch) {
          return !std::isspace(ch);
        });

    // it.base() converts the reverse_iterator back to a normal iterator
    // We take the prefix of the string from the start to that point
    return sv.substr(0, std::distance(sv.begin(), it.base()));
  }

  static bool has_whitespace(std::string_view sv) {
    return std::ranges::any_of(
        sv, [](unsigned char ch) { return std::isspace(ch); });
  }

  static std::vector<Section>::iterator
  find_section(std::vector<Section> &sections, string_view search_name) {
    auto check = [search_name](const Section section) {
      return section.name == search_name;
    };

    return std::find_if(sections.begin(), sections.end(), check);
  }

  static std::vector<Section>::const_iterator
  find_section(const std::vector<Section> &sections, string_view search_name) {
    auto check = [search_name](const Section section) {
      return section.name == search_name;
    };

    return std::find_if(sections.begin(), sections.end(), check);
  }

  static std::vector<Line>::iterator
  find_section_header(std::vector<Line> &lines, string_view section_name) {
    return std::find_if(lines.begin(), lines.end(), [section_name](auto l) {
      if (auto *sh = std::get_if<SectionHeader>(&l)) {
        return sh->line == section_name;
      }
      return false;
    });
  }

  static std::span<Line>::iterator
  find_section_header(std::span<Line> lines, string_view section_name) {
    return std::find_if(lines.begin(), lines.end(), [section_name](auto l) {
      if (auto *sh = std::get_if<SectionHeader>(&l)) {
        return sh->line == section_name;
      }
      return false;
    });
  }

  static std::span<Line> find_section_subspan(std::span<Line> lines,
                                              string_view section_name) {
    auto header_start = find_section_header(lines, section_name);
    if (header_start == lines.end()) {
      return {lines.end(), lines.end()};
    }

    auto offset = std::distance(lines.begin(), header_start) + 1;
    auto tail = lines.subspan(offset);

    auto next_header = std::find_if(tail.begin(), tail.end(), [](auto &l) {
      return std::holds_alternative<SectionHeader>(l);
    });

    return std::span(header_start, next_header);
  }

  template <typename T>
  static void append_to_section(std::vector<Line> &lines,
                                std::vector<Section>::iterator section_it,
                                string_view section_name, T entry) {
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
    auto str = std::visit(
        [](const Line &val) { return ConfigDataView::to_string(val); },
        lines[index]);
  }
};

namespace fs = std::filesystem;

std::expected<ConfigDataView, int> parseFile(const fs::path &path) {
  std::ifstream file{path, std::ios_base::in};
  if (!file.is_open()) {
    std::println(std::cerr, "Failed to open file");
    return std::unexpected(std::to_underlying(ExitCode::EOPEN));
  }

  std::string current_line{};
  ConfigDataView config{};
  int line_num{0};

  while (std::getline(file, current_line)) {
    int res = config.parse(std::move(current_line), ++line_num);
    if (res != 0) {
      return std::unexpected(res);
    }
  }
  config.end();

  return config;
}

int main(int argc, char *argv[]) {
  const string USAGE = std::format(
      "{} [FILE] <operation> [options...] \n"
      "Operations:\n"
      "-Qv [section] [value]: Query if value exists\n"
      "-Qk [section] [key]: Query if key exists\n"
      "-Rv [section] [value]: Remove value if exists\n"
      "-Rv [section] [key]: Remove value with specified key if it exists\n"
      "-Sv [section] [value]: Set value, does nothing if exists\n"
      "-Sv [section] [key] [value]: Set key to value, overriding if exists\n"
      "\n"
      "Example Usage:\n"
      "{} /etc/pacman.conf -Qv \"[options]\" \"CheckSpace\"\n"
      "{} /etc/pacman.conf -Qk \"[options]\" \"HoldPkg\"\n"
      "{} /etc/pacman.conf -Rv \"[options]\" \"CheckSpace\"\n"
      "{} /etc/pacman.conf -Rk \"[options]\" \"HoldPkg\"\n"
      "{} /etc/pacman.conf -Sv \"[options]\" \"NoProgressBar\"\n"
      "{} /etc/pacman.conf -Sk \"[options]\" \"ParallelDownloads\" \"16\"",
      argv[0], argv[0], argv[0], argv[0], argv[0], argv[0], argv[0]);

  if (argc <= 1) {
    std::println(std::cerr, "{}", USAGE);
    return std::to_underlying(ExitCode::EARGS);
  }

  if (argc < 5) {
    std::println(std::cerr, "Not enough arguments, minimum 4 needed");
    return std::to_underlying(ExitCode::EARGS);
  }

  fs::path file_path{argv[1]};
  auto parsed_res = parseFile(file_path);
  if (!parsed_res)
    return parsed_res.error();
  auto parsed = parsed_res.value();

  string_view op{argv[2]};
  string_view section{argv[3]};

  if (op == "-Qv") {
    string_view value{argv[4]};
    auto query = parsed.get_value_entry(section, value);
    if (!query) {
      return std::to_underlying(ExitCode::FAILURE);
    }
    std::println("{}", query->value());
    return std::to_underlying(ExitCode::SUCCESS);
  } else if (op == "-Qk") {
    string_view key{argv[4]};
    auto query = parsed.get_key_value_pair(section, key);
    if (!query) {
      return std::to_underlying(ExitCode::FAILURE);
    }
    std::println("{}", query->value());
    return std::to_underlying(ExitCode::SUCCESS);
  } else if (op == "-Rv") {
    string_view value{argv[4]};
    auto res = parsed.remove_value(section, value);
    std::cout << parsed << std::endl;
    return std::to_underlying(res ? ExitCode::SUCCESS : ExitCode::FAILURE);
  } else if (op == "-Rk") {
    string_view key{argv[4]};
    auto res = parsed.remove_key(section, key);
    std::cout << parsed << std::endl;
    return std::to_underlying(res ? ExitCode::SUCCESS : ExitCode::FAILURE);
  } else if (op == "-Sv") {
    string_view value{argv[4]};
    parsed.set_value(section, value);
    std::cout << parsed << std::endl;
    return std::to_underlying(ExitCode::SUCCESS);
  } else if (op == "-Sk") {
    if (argc < 6) {
      std::println(std::cerr, "Not enough arguments, 5 needed");
      return std::to_underlying(ExitCode::EARGS);
    }
    string_view key{argv[4]};
    string_view value{argv[5]};
    parsed.set_key_value(section, key, value);
    std::cout << parsed << std::endl;
    return std::to_underlying(ExitCode::SUCCESS);
  }

  std::print(std::cerr, "Unknown operation: {}", op);
  return std::to_underlying(ExitCode::EARGS);
}

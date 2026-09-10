#include "confquery.hpp"

#include <algorithm>
#include <cctype>
#include <format>
#include <fstream>
#include <iterator>
#include <print>
#include <ranges>
#include <utility>

namespace confquery {

ConfigDataView::KeyValueEntry ConfigDataView::KeyValueEntry::create(
    std::string key, std::string value) {
  size_t vStart = key.length() + 3;
  return {
      .line = std::format("{} = {}", key, value),
      .keyLen = key.length(),
      .valueStart = vStart,
      .valueLen = value.length() // Note: substr length vs end index
  };
}

ConfigDataView::ValueEntry
ConfigDataView::ValueEntry::create(std::string value) {
  size_t vLen = value.length();
  return {.line = std::move(value), .valueLen = vLen};
}

std::string ConfigDataView::to_string(const Line &line) {
  return std::visit([](const auto &val) { return val.line; }, line);
}

std::string ConfigDataView::to_string(const Section::Entry &entry) {
  return std::visit([](const auto &val) { return val.line; }, entry);
}

ConfigDataView::ConfigDataView(std::ostream &err_stream)
    : lines{}, sections{}, current_section{}, err(err_stream) {}

std::ostream &operator<<(std::ostream &out, const ConfigDataView &data) {
  for (const auto &line : data.lines) {
    out << ConfigDataView::to_string(line) << '\n';
  }
  return out;
}

bool ConfigDataView::parse(std::string line, int line_num) {

  // Parse comment lines
  if (line.empty() || line.starts_with("#") || line.starts_with(";")) {
    lines.emplace_back(Comment{std::move(line)});
    return true;
  }

  // We make a view substring with right whitespace trimmed for parsing, but
  // use original line for preserving original structure
  auto tline = trim_right(line);

  // Parse empty lines
  if (tline.empty()) {
    lines.emplace_back(Comment{std::move(line)});
    return true;
  }

  // Parse Section Header
  if (tline[0] == '[') {
    if (tline.back() != ']') {
      std::println(err,
                   "[{}] Expecting ']' as last character to close section, "
                   "found '{}':\n{}",
                   line_num, tline.back(), line);
      return false;
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

    return true;
  }

  // Parse KeyValueEntry
  if (const auto pos = tline.find(" = "); pos != tline.npos) {
    if (!current_section.has_value()) {
      std::println(
          err,
          "[{}] Found Key-Value entry outside of a section definition:\n{}",
          line_num, line);
      return false;
    }

    // Construct KeyValueEntry with the original (moved) line, pos are correct
    // since we only trim the end
    auto keyEnd = trim_right(std::string_view{line}.substr(0, pos)).size();
    auto entry = KeyValueEntry{.line = std::move(line),
                               .keyLen = keyEnd,
                               .valueStart = pos + 3,
                               .valueLen = tline.size()};

    // Store entry to lines and current_section
    lines.emplace_back(entry);
    current_section->entries.emplace_back(std::move(entry));

    return true;
  }

  // Parse ValueEntry
  if (!has_whitespace(tline)) {
    if (!current_section.has_value()) {
      std::println(
          err, "[{}] Found Value entry outside of a section definition:\n{}",
          line_num, line);
      return false;
    }
    auto entry = ValueEntry{std::move(line), tline.size()};
    lines.emplace_back(entry);
    current_section->entries.emplace_back(std::move(entry));
    return true;
  }

  // Unknown line
  std::println(err, "[{}] Unknown line type:\n{}", line_num, line);
  return false;
}

void ConfigDataView::end() {
  // Add current section to the list only if it's not empty
  if (current_section.has_value()) {
    sections.emplace_back(std::move(*current_section));
  }
}

const ConfigDataView::Section *
ConfigDataView::get_section(std::string_view section_name) const {
  const auto section_it = find_section(this->sections, section_name);

  if (section_it == this->sections.end())
    return nullptr;

  return &*section_it;
}

bool ConfigDataView::has_section_header(std::string_view section_name) const {
  return get_section(section_name) != nullptr;
}

const ConfigDataView::ValueEntry *
ConfigDataView::get_value_entry(std::string_view section_name,
                                std::string_view value_name) const {
  const auto section_it = find_section(this->sections, section_name);

  if (section_it != sections.end()) {
    for (const auto &entry : section_it->entries) {
      if (const auto *ve = std::get_if<ValueEntry>(&entry)) {
        if (ve->value() == value_name) {
          return ve;
        }
      }
    }
  }

  return nullptr;
}

bool ConfigDataView::has_value_entry(std::string_view section,
                                     std::string_view value) const {
  return get_value_entry(section, value) != nullptr;
}

const ConfigDataView::KeyValueEntry *
ConfigDataView::get_key_value_pair(std::string_view section_name,
                                   std::string_view key_name) const {
  const auto section_it = find_section(this->sections, section_name);

  if (section_it != sections.end()) {
    for (const auto &entry : section_it->entries) {
      if (const auto *kv = std::get_if<KeyValueEntry>(&entry)) {
        if (kv->key() == key_name) {
          return kv;
        }
      }
    }
  }

  return nullptr;
}

bool ConfigDataView::has_key_value_pair(std::string_view section,
                                        std::string_view key) const {
  return get_key_value_pair(section, key) != nullptr;
}

void ConfigDataView::set_value(std::string_view section_name,
                               std::string_view value) {
  const auto section_it = find_section(this->sections, section_name);

  if (section_it == this->sections.end()) {
    // Add new section
    this->sections.emplace_back(Section{
        std::string{section_name}, {ValueEntry::create(std::string{value})}});
    // Add new lines for it
    this->lines.emplace_back(SectionHeader{std::string{section_name}});
    this->lines.emplace_back(ValueEntry::create(std::string{value}));
    return;
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
                    ValueEntry::create(std::string{value}));
}

void ConfigDataView::set_key_value(std::string_view section_name,
                                   std::string_view key,
                                   std::string_view value) {
  const auto section_it = find_section(this->sections, section_name);

  if (section_it == this->sections.end()) {
    // Add new section
    this->sections.emplace_back(Section{
        std::string{section_name},
        {KeyValueEntry::create(std::string{key}, std::string{value})}});
    // Add new lines for it
    this->lines.emplace_back(SectionHeader{std::string{section_name}});
    this->lines.emplace_back(
        KeyValueEntry::create(std::string{key}, std::string{value}));
    return;
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

bool ConfigDataView::remove_section(std::string_view section_name) {
  // Locate within sections first
  auto section_it = find_section(this->sections, section_name);
  if (section_it == this->sections.end()) {
    return false;
  }

  // Eliminate from sections
  this->sections.erase(section_it);

  // Then locate on lines
  auto lines_sp = this->as_span();
  auto section_subspan = find_section_subspan(lines_sp, section_name);
  if (section_subspan.begin() == lines_sp.end()) {
    throw std::runtime_error(
        "find_section_subspan cannot fail within remove_value at this point");
  }

  // Eliminate from lines
  auto offset = std::distance(lines_sp.begin(), section_subspan.begin());
  this->lines.erase(this->lines.begin() + offset,
                    this->lines.begin() + offset + section_subspan.size());
  return true;
}

bool ConfigDataView::remove_value(std::string_view section_name,
                                  std::string_view value_name) {

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

bool ConfigDataView::remove_key(std::string_view section_name,
                                std::string_view key_name) {

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

std::string_view ConfigDataView::trim_right(std::string_view sv) {
  // Find the first non-whitespace character starting from the back
  auto it =
      std::ranges::find_if(sv | std::views::reverse, [](unsigned char ch) {
        return !std::isspace(ch);
      });

  // it.base() converts the reverse_iterator back to a normal iterator
  // We take the prefix of the string from the start to that point
  return sv.substr(0, std::distance(sv.begin(), it.base()));
}

bool ConfigDataView::has_whitespace(std::string_view sv) {
  return std::ranges::any_of(
      sv, [](unsigned char ch) { return std::isspace(ch); });
}

std::vector<ConfigDataView::Section>::iterator
ConfigDataView::find_section(std::vector<Section> &sections,
                             std::string_view search_name) {
  auto check = [search_name](const Section &section) {
    return section.name == search_name;
  };

  return std::find_if(sections.begin(), sections.end(), check);
}

std::vector<ConfigDataView::Section>::const_iterator
ConfigDataView::find_section(const std::vector<Section> &sections,
                             std::string_view search_name) {
  auto check = [search_name](const Section &section) {
    return section.name == search_name;
  };

  return std::find_if(sections.begin(), sections.end(), check);
}

std::vector<ConfigDataView::Line>::iterator
ConfigDataView::find_section_header(std::vector<Line> &lines,
                                    std::string_view section_name) {
  return std::find_if(lines.begin(), lines.end(), [section_name](auto &l) {
    if (auto *sh = std::get_if<SectionHeader>(&l)) {
      return sh->line == section_name;
    }
    return false;
  });
}

std::span<ConfigDataView::Line>::iterator
ConfigDataView::find_section_header(std::span<Line> lines,
                                    std::string_view section_name) {
  return std::find_if(lines.begin(), lines.end(), [section_name](auto &l) {
    if (auto *sh = std::get_if<SectionHeader>(&l)) {
      return sh->line == section_name;
    }
    return false;
  });
}

std::span<const ConfigDataView::Line>::const_iterator
ConfigDataView::find_section_header(std::span<const Line> lines,
                                    std::string_view section_name) {
  return std::find_if(lines.begin(), lines.end(), [section_name](auto &l) {
    if (const auto *sh = std::get_if<SectionHeader>(&l)) {
      return sh->line == section_name;
    }
    return false;
  });
}

std::span<ConfigDataView::Line>
ConfigDataView::find_section_subspan(std::span<Line> lines,
                                     std::string_view section_name) {
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

std::expected<ConfigDataView, ExitCode> parseFile(const fs::path &path) {
  std::ifstream file{path, std::ios_base::in};
  if (!file.is_open()) {
    std::println(std::cerr, "Failed to open file");
    return std::unexpected(ExitCode::EOPEN);
  }

  std::string current_line{};
  ConfigDataView config{};
  int line_num{0};

  while (std::getline(file, current_line)) {
    if (!config.parse(std::move(current_line), ++line_num)) {
      return std::unexpected(ExitCode::EPARSE);
    }
  }
  config.end();

  return config;
}

} // namespace confquery

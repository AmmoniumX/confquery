#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <print>
#include <stdexcept>
#include <string_view>
#include <utility>

#include <argdispatch/argdispatch.hpp>

#include "confquery.hpp"

using namespace confquery;
using std::string_view;

namespace {

ConfigDataView load_or_exit(string_view file) {
  fs::path file_path{file};
  auto parsed_res = parseFile(file_path);
  if (!parsed_res) {
    std::exit(std::to_underlying(parsed_res.error()));
  }
  return std::move(parsed_res).value();
}

void query_section(string_view file, string_view section) {
  auto parsed = load_or_exit(file);
  auto query = parsed.get_section(section);
  if (!query) {
    std::exit(std::to_underlying(ExitCode::FAILURE));
  }
  std::println("{}", query->name);
}

void query_value(string_view file, string_view section, string_view value) {
  auto parsed = load_or_exit(file);
  auto query = parsed.get_value_entry(section, value);
  if (!query) {
    std::exit(std::to_underlying(ExitCode::FAILURE));
  }
  std::println("{}", query->value());
}

void query_key(string_view file, string_view section, string_view key) {
  auto parsed = load_or_exit(file);
  auto query = parsed.get_key_value_pair(section, key);
  if (!query) {
    std::exit(std::to_underlying(ExitCode::FAILURE));
  }
  std::println("{}", query->value());
}

void remove_section_op(string_view file, string_view section) {
  auto parsed = load_or_exit(file);
  auto res = parsed.remove_section(section);
  std::cout << parsed << std::endl;
  if (!res) {
    std::exit(std::to_underlying(ExitCode::FAILURE));
  }
}

void remove_value_op(string_view file, string_view section, string_view value) {
  auto parsed = load_or_exit(file);
  auto res = parsed.remove_value(section, value);
  std::cout << parsed << std::endl;
  if (!res) {
    std::exit(std::to_underlying(ExitCode::FAILURE));
  }
}

void remove_key_op(string_view file, string_view section, string_view key) {
  auto parsed = load_or_exit(file);
  auto res = parsed.remove_key(section, key);
  std::cout << parsed << std::endl;
  if (!res) {
    std::exit(std::to_underlying(ExitCode::FAILURE));
  }
}

void set_value_op(string_view file, string_view section, string_view value) {
  auto parsed = load_or_exit(file);
  parsed.set_value(section, value);
  std::cout << parsed << std::endl;
}

void set_key_value_op(string_view file, string_view section, string_view key,
                      string_view value) {
  auto parsed = load_or_exit(file);
  parsed.set_key_value(section, key, value);
  std::cout << parsed << std::endl;
}

} // namespace

int main(int argc, char *argv[]) {
  argdispatch::ArgDispatcher dispatcher;

  auto file = dispatcher.and_then<string_view>("file");

  file.literal("-Qs").and_then<string_view>("section").executes(query_section);
  file.literal("-Qv")
      .and_then<string_view>("section")
      .and_then<string_view>("value")
      .executes(query_value);
  file.literal("-Qk")
      .and_then<string_view>("section")
      .and_then<string_view>("key")
      .executes(query_key);
  file.literal("-Rs").and_then<string_view>("section").executes(remove_section_op);
  file.literal("-Rv")
      .and_then<string_view>("section")
      .and_then<string_view>("value")
      .executes(remove_value_op);
  file.literal("-Rk")
      .and_then<string_view>("section")
      .and_then<string_view>("key")
      .executes(remove_key_op);
  file.literal("-Sv")
      .and_then<string_view>("section")
      .and_then<string_view>("value")
      .executes(set_value_op);
  file.literal("-Sk")
      .and_then<string_view>("section")
      .and_then<string_view>("key")
      .and_then<string_view>("value")
      .executes(set_key_value_op);

  try {
    return dispatcher.dispatch(argc, argv);
  } catch (const std::runtime_error &e) {
    std::println(std::cerr, "Internal error: {}", e.what());
    return std::to_underlying(ExitCode::EINTERNAL);
  }
}

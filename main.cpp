#include <cstddef>
#include <filesystem>
#include <format>
#include <iostream>
#include <optional>
#include <print>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include "confquery.hpp"

using namespace confquery;
using std::string, std::string_view;

namespace {

// Bounds-checked argument access for program args
struct Args {
  std::span<const char *const> args;

  std::optional<std::string_view> operator[](std::size_t idx) const {
    if (idx >= args.size()) {
      return std::nullopt;
    }

    if (args[idx] == nullptr) {
      return std::nullopt;
    }

    return std::string_view(args[idx]);
  }
};

} // namespace

int main(int argc, char *argv[]) {
  Args args{std::span(argv, static_cast<size_t>(argc))};

  const string USAGE = std::format(
      "{0} [FILE] <operation> [options...] \n"
      "Operations:\n"
      "-Qs [section]: Query if section exists\n"
      "-Qv [section] [value]: Query if value exists\n"
      "-Qk [section] [key]: Query if key exists\n"
      "-Rs [section]: Remove entire section\n"
      "-Rv [section] [value]: Remove value if exists\n"
      "-Rk [section] [key]: Remove value with specified key if it exists\n"
      "-Sv [section] [value]: Set value, does nothing if exists\n"
      "-Sk [section] [key] [value]: Set key to value, overriding if exists\n"
      "\n"
      "Example Usage:\n"
      "{0} /etc/pacman.conf -Qv \"[options]\" \"CheckSpace\"\n"
      "{0} /etc/pacman.conf -Qk \"[options]\" \"HoldPkg\"\n"
      "{0} /etc/pacman.conf -Rv \"[options]\" \"CheckSpace\"\n"
      "{0} /etc/pacman.conf -Rk \"[options]\" \"HoldPkg\"\n"
      "{0} /etc/pacman.conf -Sv \"[options]\" \"NoProgressBar\"\n"
      "{0} /etc/pacman.conf -Sk \"[options]\" \"ParallelDownloads\" \"16\"",
      args[0].value_or("confq"));

  // Early parse: min 4 args (file, op, section)
  auto op_arg = args[2];
  auto section_arg = args[3];
  if (!section_arg) {
    std::println(std::cerr, "{}", USAGE);
    return std::to_underlying(ExitCode::EARGS);
  }
  string_view op = *op_arg;
  string_view section = *section_arg;

  // Parse arguments and execute
  if (op == "-Qs" || op == "-Rs") {
    if (!section_arg) {
      std::println(std::cerr, "{}", USAGE);
      return std::to_underlying(ExitCode::EARGS);
    }
  } else if (op == "-Sk") {
    if (!args[5]) {
      std::println(std::cerr, "{}", USAGE);
      return std::to_underlying(ExitCode::EARGS);
    }
  } else if (!args[4]) {
    std::println(std::cerr, "{}", USAGE);
    return std::to_underlying(ExitCode::EARGS);
  }

  fs::path file_path{*args[1]};
  auto parsed_res = parseFile(file_path);
  if (!parsed_res)
    return std::to_underlying(parsed_res.error());
  auto parsed = std::move(parsed_res).value();

  try {
    if (op == "-Qs") {
      auto query = parsed.get_section(section);
      if (!query) {
        return std::to_underlying(ExitCode::FAILURE);
      }
      std::println("{}", query->name);

      return std::to_underlying(ExitCode::SUCCESS);

    } else if (op == "-Qv") {
      string_view value = *args[4];
      auto query = parsed.get_value_entry(section, value);
      if (!query) {
        return std::to_underlying(ExitCode::FAILURE);
      }
      std::println("{}", query->value());

      return std::to_underlying(ExitCode::SUCCESS);

    } else if (op == "-Qk") {
      string_view key = *args[4];
      auto query = parsed.get_key_value_pair(section, key);
      if (!query) {
        return std::to_underlying(ExitCode::FAILURE);
      }
      std::println("{}", query->value());

      return std::to_underlying(ExitCode::SUCCESS);

    } else if (op == "-Rs") {
      auto res = parsed.remove_section(section);
      std::cout << parsed << std::endl;

      return std::to_underlying(res ? ExitCode::SUCCESS : ExitCode::FAILURE);

    } else if (op == "-Rv") {
      string_view value = *args[4];
      auto res = parsed.remove_value(section, value);
      std::cout << parsed << std::endl;

      return std::to_underlying(res ? ExitCode::SUCCESS : ExitCode::FAILURE);

    } else if (op == "-Rk") {
      string_view key = *args[4];
      auto res = parsed.remove_key(section, key);
      std::cout << parsed << std::endl;

      return std::to_underlying(res ? ExitCode::SUCCESS : ExitCode::FAILURE);

    } else if (op == "-Sv") {
      string_view value = *args[4];
      parsed.set_value(section, value);
      std::cout << parsed << std::endl;

      return std::to_underlying(ExitCode::SUCCESS);

    } else if (op == "-Sk") {
      string_view key = *args[4];
      string_view value = *args[5];
      parsed.set_key_value(section, key, value);
      std::cout << parsed << std::endl;

      return std::to_underlying(ExitCode::SUCCESS);
    }
  } catch (const std::runtime_error &e) {
    std::println(std::cerr, "Internal error: {}", e.what());
    return std::to_underlying(ExitCode::EINTERNAL);
  }

  std::print(std::cerr, "Unknown operation: {}", op);
  return std::to_underlying(ExitCode::EARGS);
}

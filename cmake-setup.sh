#!/bin/bash
set -e
cmake -S . -B build/debug -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=On
cmake -S . -B build/release -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=On
[[ -L compile_commands.json || -f compile_commands.json ]] && rm ./compile_commands.json
ln -s build/debug/compile_commands.json ./compile_commands.json

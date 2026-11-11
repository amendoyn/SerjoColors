#!/bin/sh
set -eu
cd "$(dirname "$0")"
major=$(awk '/^#define SERJO_VERSION_MAJOR / {print $3}' src/version.h)
minor=$(awk '/^#define SERJO_VERSION_MINOR / {print $3}' src/version.h)
test -n "$major" && test -n "$minor"
mkdir -p dist
x86_64-w64-mingw32-g++ -std=c++17 -O2 -s -Wall -Wextra -municode -mwindows -static src/main.cpp -o dist/SerjoColors.exe -lcomctl32 -lgdi32 -lshell32 -ladvapi32 -lwinhttp -lbcrypt
hash=$(shasum -a 256 dist/SerjoColors.exe | awk '{print $1}')
printf '%s.%s\n%s\n' "$major" "$minor" "$hash" > version.txt

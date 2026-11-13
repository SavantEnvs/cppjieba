#!/usr/bin/env bash
#
# mayhem/build.sh — build cppjieba's fuzz harnesses (sanitized, DWARF 3) and its gtest suite (normal flags).
# Runs inside the commit image as `mayhem` in /mayhem; re-runnable and air-gapped (googletest comes from
# /opt/toolchains/deps/googletest-1.12.1, cached by the Dockerfile).
set -euo pipefail

[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
: "${SRC:=/mayhem}"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS

cd "$SRC"

# 1) The test suite, with the project's NORMAL flags, into `build/` (gitignored upstream, so the
#    `git clean -ffdX` the patch grader runs removes it and this step recreates it). A stale configure
#    from the image build (compiler resolved through ccache's masquerade dir) would make CMake drop its
#    cache and lose the offline googletest override, so always configure from scratch.
rm -rf build
cmake -S . -B build -G Ninja \
  -DCMAKE_C_COMPILER="$CC" -DCMAKE_CXX_COMPILER="$CXX" \
  -DCMAKE_C_FLAGS="$COVERAGE_FLAGS" -DCMAKE_CXX_FLAGS="$COVERAGE_FLAGS" \
  -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=/opt/toolchains/deps/googletest-1.12.1/src \
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON
cmake --build build -j"$MAYHEM_JOBS" --target test.run

# 2) The fuzz harnesses. cppjieba is header-only, so the library is compiled into each harness with the
#    sanitizer + SanCov flags. Dictionaries are read from the image's dict/ (the library's own default).
# 3) Each harness twice: libFuzzer binary + standalone run-once reproducer.
#    LOGGING_LEVEL=LL_FATAL silences the library's per-input "invalid UTF-8" ERROR lines on stderr.
CXXFLAGS_COMMON=(-std=c++14 -O1 -I"$SRC/include"
  -DLOGGING_LEVEL=LL_FATAL -DCPPJIEBA_DICT_DIR="\"$SRC/dict\""
  -DTINY_DICT="\"$SRC/mayhem/fuzz_userdict/tiny.dict.utf8\"")

OBJ="$(mktemp -d)"
trap 'rm -rf "$OBJ"' EXIT
# shellcheck disable=SC2086
$CC $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o "$OBJ/standalone_main.o"
# shellcheck disable=SC2086
$CXX $SANITIZER_FLAGS $DEBUG_FLAGS -c "$SRC/mayhem/lsan_off.cc" -o "$OBJ/lsan_off.o"

pids=()
for t in fuzz_cut fuzz_extract fuzz_userdict; do
  # shellcheck disable=SC2086
  $CXX $SANITIZER_FLAGS $DEBUG_FLAGS "${CXXFLAGS_COMMON[@]}" $LIB_FUZZING_ENGINE \
      "$SRC/mayhem/$t.cpp" "$OBJ/lsan_off.o" -o "$SRC/$t" &
  pids+=($!)
  # shellcheck disable=SC2086
  $CXX $SANITIZER_FLAGS $DEBUG_FLAGS "${CXXFLAGS_COMMON[@]}" \
      "$SRC/mayhem/$t.cpp" "$OBJ/standalone_main.o" "$OBJ/lsan_off.o" -o "$SRC/$t-standalone" &
  pids+=($!)
done
for p in "${pids[@]}"; do wait "$p"; done
for t in fuzz_cut fuzz_extract fuzz_userdict; do
  [ -x "$SRC/$t" ] && [ -x "$SRC/$t-standalone" ] || { echo "missing $t" >&2; exit 1; }
done

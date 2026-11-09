#!/bin/sh
# Builds and runs every source/*_tests.cpp (or the ones named), printing PASS/FAIL per test. Exit 1 if any failed.
# Usage: sh source/run-cpp-tests.sh [name_tests.cpp ...]   Output exe goes to $TEST_OUT (default: a temp folder).
# Optional real-file cases: BLUEASH_TEST_NR (a signed nvngx_dlssnr.dll), BLUEASH_TEST_OPTI_ZIP (the OptiScaler release).
cd "$(dirname "$0")" || exit 1
if ! command -v g++ >/dev/null 2>&1; then
  PATH="$(ls -d "$LOCALAPPDATA"/Microsoft/WinGet/Packages/BrechtSanders.WinLibs*/mingw64/bin 2>/dev/null | head -1):$PATH"; export PATH
fi
out="${TEST_OUT:-${TEMP:-/tmp}/blueash-cpp-tests}"; mkdir -p "$out"
tests="$*"; [ -n "$tests" ] || tests=$(ls *_tests.cpp)
failed=0
for t in $tests; do
  exe="$out/${t%.cpp}.exe"; rm -f "$exe"
  if ! g++ -std=c++17 -Wall -Wextra -Werror "$t" -o "$exe" -lole32 -lwindowsapp -lbcrypt -lwintrust -lcrypt32 -lversion -lwinhttp -lgdi32 -luser32 -loleaut32 -lwbemuuid -lpowrprof -lpdh -ld3d11 -ldxgi -lwinmm -luuid -lshell32 -lpsapi -lwevtapi >"$out/build.log" 2>&1; then
    echo "FAIL (build) $t"; grep -m5 error "$out/build.log"; failed=1; continue; fi
  if "$exe" >"$out/run.log" 2>&1; then echo "PASS $t"; else echo "FAIL $t"; tail -5 "$out/run.log"; failed=1; fi
done
exit $failed

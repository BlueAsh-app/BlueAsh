#!/bin/sh
set -eu
cd "$(dirname "$0")"
# Linux cross toolchain uses prefixed names; Windows WinLibs/MSYS2 ship plain g++/windres.
if command -v x86_64-w64-mingw32-windres >/dev/null 2>&1; then WINDRES=x86_64-w64-mingw32-windres; else WINDRES=windres; fi
if command -v x86_64-w64-mingw32-g++ >/dev/null 2>&1; then CXX=x86_64-w64-mingw32-g++; else CXX=g++; fi
# Toolchains that link their own default-manifest.o clash with app.manifest
# (".rsrc merge failure: multiple non-default manifests"). Shadow it with an empty object.
tmp=$(mktemp -d);trap 'rm -rf "$tmp" .bundle' EXIT
echo "" | "$CXX" -x c -c - -o "$tmp/default-manifest.o"
"$WINDRES" resources.rc -O coff -o "$tmp/resources.o"
# Embed the helper folders (bundle.h unpacks them per build id) so BlueAsh.exe runs on its own.
# Paths stay relative/ASCII for windres; C# sources are build inputs, not runtime files: the helpers run as one program,
# companion/BlueAshHelper.exe (build-helper.ps1). Developer files the app never reads stay out: the test/tool library
# CompanionUI.dll, the feature status list (companion/FEATURES.txt) and the deployment notes (updater/DEPLOY.txt).
files=$(cd .. && find companion maps updater -type f ! -name '*.cs' ! -name 'CompanionUI.dll' ! -name 'FEATURES.txt' ! -name 'DEPLOY.txt' | LC_ALL=C sort)
id=$(cd .. && cat $files | sha256sum | cut -c1-16)
rm -rf .bundle;mkdir .bundle
{ echo "$id";for f in $files;do echo "$f";done; } > .bundle/list.txt
{ echo '999 RCDATA ".bundle/list.txt"';i=1000;for f in $files;do echo "$i RCDATA \"../$f\"";i=$((i+1));done; } > .bundle/bundle.rc
"$WINDRES" .bundle/bundle.rc -O coff -o "$tmp/bundle.o"
"$CXX" -B"$tmp/" -std=c++17 -O2 -Wall -Wextra -Werror -municode -mwindows \
  -static -static-libgcc -static-libstdc++ ModeSwitch.cpp "$tmp/resources.o" "$tmp/bundle.o" \
  -lole32 -luuid -luser32 -lgdi32 -ladvapi32 -lshell32 -ldwmapi -lwinhttp -lcomdlg32 -lwinmm -ld3d11 -ldxgi -lruntimeobject -lgdiplus -lcomctl32 -lbcrypt -lcrypt32 -lversion -lwintrust -lwbemuuid -lpowrprof -lpdh -loleaut32 -lwevtapi -lurlmon -o "${BLUEASH_OUT:-../BlueAsh.exe}"
echo "bundle $id: $(echo "$files" | wc -l) files"

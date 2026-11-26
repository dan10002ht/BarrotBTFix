#!/bin/bash
#
# Build BarrotBTFix.kext (x86_64) without Xcode.
#
#   ./build.sh            build into build/BarrotBTFix.kext and dist/BarrotBTFix-<ver>.zip
#   ./build.sh clean      remove build/ and dist/ (deps/ is kept)
#
# Requirements: Xcode Command Line Tools (clang, macOS SDK), curl, unzip, git.
# Dependencies are downloaded into deps/ on first run and pinned below.
#
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"

NAME=BarrotBTFix
BUNDLE_ID=io.github.dan10002ht.BarrotBTFix
VERSION="$(tr -d '[:space:]' < VERSION)"

LILU_VERSION=1.7.2
MACKERNELSDK_COMMIT=3f750085caa17ec3a7880f11c11bf4f48cd6a164

DEPS="$ROOT/deps"
BUILD="$ROOT/build"
DIST="$ROOT/dist"

if [[ "${1:-}" == "clean" ]]; then
	rm -rf "$BUILD" "$DIST"
	echo "cleaned"
	exit 0
fi

# --- dependencies -----------------------------------------------------------

mkdir -p "$DEPS"

LILU_SDK="$DEPS/Lilu.kext/Contents/Resources"
if [[ ! -f "$DEPS/.lilu-$LILU_VERSION" ]]; then
	echo "==> fetching Lilu $LILU_VERSION (DEBUG, includes SDK)"
	rm -rf "$DEPS/Lilu.kext" "$DEPS/Lilu.zip"
	curl -fsSL -o "$DEPS/Lilu.zip" \
		"https://github.com/acidanthera/Lilu/releases/download/$LILU_VERSION/Lilu-$LILU_VERSION-DEBUG.zip"
	unzip -q -o "$DEPS/Lilu.zip" 'Lilu.kext/*' -d "$DEPS"
	rm -f "$DEPS/Lilu.zip" "$DEPS"/.lilu-*
	touch "$DEPS/.lilu-$LILU_VERSION"
fi

KSDK="$DEPS/MacKernelSDK"
if [[ "$(git -C "$KSDK" rev-parse HEAD 2>/dev/null || true)" != "$MACKERNELSDK_COMMIT" ]]; then
	echo "==> fetching MacKernelSDK @ ${MACKERNELSDK_COMMIT:0:7}"
	rm -rf "$KSDK"
	git init -q "$KSDK"
	git -C "$KSDK" fetch -q --depth 1 https://github.com/acidanthera/MacKernelSDK.git "$MACKERNELSDK_COMMIT"
	git -C "$KSDK" checkout -q FETCH_HEAD
fi

# --- compile ----------------------------------------------------------------

SDK="$(xcrun --sdk macosx --show-sdk-path)"
CLANG_RT="$(clang -print-resource-dir)/lib/darwin/libclang_rt.cc_kext.a"
[[ -f "$CLANG_RT" ]] || { echo "error: $CLANG_RT not found" >&2; exit 1; }

TARGET=(-target x86_64-apple-macos12.0 -isysroot "$SDK")
COMMON=("${TARGET[@]}" -nostdinc -I"$KSDK/Headers" -I"$LILU_SDK"
	-mkernel -fapple-kext -fno-builtin -fno-common -fno-stack-protector -O2
	-Wall -Wno-unused-variable -Wno-unused-parameter -Wno-deprecated-declarations
	-DKERNEL -DKERNEL_PRIVATE -DDRIVER_PRIVATE -DAPPLE -DNeXT
	-DPRODUCT_NAME="$NAME" -DMODULE_VERSION="$VERSION")
CXX=(-std=c++17 -fno-rtti -fno-exceptions)

OBJ="$BUILD/obj"
rm -rf "$BUILD"
mkdir -p "$OBJ"

echo "==> compiling $NAME $VERSION"
clang++ "${COMMON[@]}" "${CXX[@]}" -c src/BarrotBTFix.cpp          -o "$OBJ/BarrotBTFix.o"
clang++ "${COMMON[@]}" "${CXX[@]}" -c "$LILU_SDK/Library/plugin_start.cpp" -o "$OBJ/plugin_start.o"
clang   "${COMMON[@]}"             -c src/kmod_info.c              -o "$OBJ/kmod_info.o"

KEXT="$BUILD/$NAME.kext"
mkdir -p "$KEXT/Contents/MacOS"
clang++ "${TARGET[@]}" -nostdlib -Xlinker -kext -Xlinker -export_dynamic \
	"$OBJ"/*.o -L"$KSDK/Library/x86_64" -lkmod "$CLANG_RT" \
	-o "$KEXT/Contents/MacOS/$NAME"

sed -e "s/@BUNDLE_ID@/$BUNDLE_ID/g" -e "s/@VERSION@/$VERSION/g" \
	Resources/Info.plist > "$KEXT/Contents/Info.plist"
plutil -lint -s "$KEXT/Contents/Info.plist"

# --- sanity checks ----------------------------------------------------------

file "$KEXT/Contents/MacOS/$NAME" | grep -q 'kext bundle x86_64' \
	|| { echo "error: output is not an x86_64 kext bundle" >&2; exit 1; }
strings -a "$KEXT/Contents/MacOS/$NAME" | grep -qx "$BUNDLE_ID" \
	|| { echo "error: kmod_info bundle id does not match Info.plist" >&2; exit 1; }

# --- package ----------------------------------------------------------------

mkdir -p "$DIST"
ZIP="$DIST/$NAME-$VERSION.zip"
rm -f "$ZIP"
( cd "$BUILD" && ditto -c -k --keepParent --norsrc "$NAME.kext" "$ZIP" )
( cd "$DIST" && shasum -a 256 "$(basename "$ZIP")" > "$(basename "$ZIP").sha256" )

echo "==> done"
echo "    $KEXT"
echo "    $ZIP"

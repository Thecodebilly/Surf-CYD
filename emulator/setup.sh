#!/usr/bin/env bash
# setup.sh – Install the Emscripten SDK (emsdk) and activate the latest
# release.  Run this once before `make`.
set -e

EMSDK_DIR="$HOME/emsdk"

if [ -d "$EMSDK_DIR" ]; then
  echo "emsdk already present at $EMSDK_DIR"
else
  echo "Cloning emsdk…"
  git clone https://github.com/emscripten-core/emsdk.git "$EMSDK_DIR"
fi

cd "$EMSDK_DIR"
echo "Installing latest Emscripten…"
./emsdk install  latest
./emsdk activate latest

echo ""
echo "========================================================"
echo "  Emscripten installed.  Now activate in your shell:"
echo ""
echo "    source $EMSDK_DIR/emsdk_env.sh"
echo ""
echo "  Then build the emulator:"
echo ""
echo "    cd $(dirname "$0")"
echo "    make"
echo "    make serve"
echo "========================================================"

# ── Surf CYD Web Emulator ─────────────────────────────────────────────────────
# Compiles the exact ESP32 C++ source to WebAssembly via Emscripten.
# Run `make` to build; open index.html (via a local server) to run.

EMCC ?= emcc

# ── Emscripten toolchain check ────────────────────────────────────────────────
ifeq ($(shell which $(EMCC) 2>/dev/null),)
$(error emcc not found. Run ./setup.sh to install the Emscripten SDK, then re-run make.)
endif

# ── Source files ──────────────────────────────────────────────────────────────
# main_emulator.cpp provides main() (Arduino hidden-main equivalent).
# shim_impl.cpp provides all singleton/FS/SPIFFS implementations.
# All original firmware sources are compiled unchanged.
SRCS = \
  main_emulator.cpp \
  shims/shim_impl.cpp \
  ../src/main.cpp \
  ../src/Config.cpp \
  ../src/Theme.cpp \
  ../src/Storage.cpp \
  ../src/Display.cpp \
  ../src/Network.cpp \
  ../src/TouchUI.cpp \
  ../src/Game.cpp \
  ../src/Database.cpp

# ── Include paths ─────────────────────────────────────────────────────────────
# shims/ shadows the Arduino/ESP32 headers that the firmware normally uses.
# ArduinoJson is included directly from the PlatformIO library cache.
ARDUINOJSON_DIR := $(wildcard ../.pio/libdeps/*/ArduinoJson)
ifeq ($(ARDUINOJSON_DIR),)
$(error ArduinoJson not found. Run: pio run -t build  from the project root first.)
endif

INCLUDES = \
  -Ishims \
  -I../include \
  -I$(ARDUINOJSON_DIR) \
  -I$(ARDUINOJSON_DIR)/src

# ── Compile / link flags ──────────────────────────────────────────────────────
CXXFLAGS = \
  -std=gnu++17 \
  -O2 \
  $(INCLUDES)

# ASYNCIFY lets emscripten_sleep() (delay()) suspend C++ without blocking JS.
# FETCH  enables emscripten_fetch() for synchronous HTTP calls.
EMFLAGS = \
  -sASYNCIFY \
  -sASYNCIFY_STACK_SIZE=131072 \
  -sALLOW_MEMORY_GROWTH \
  -sINITIAL_MEMORY=33554432 \
  -sMODULARIZE=0 \
  -sEXPORTED_RUNTIME_METHODS="['ccall','cwrap']" \
  --js-library canvas_bridge.js \
  -sSTACK_SIZE=1048576

OUTPUT = surfcyd.js

# ── Targets ───────────────────────────────────────────────────────────────────
.PHONY: all clean serve

all: $(OUTPUT)

$(OUTPUT): $(SRCS) shims/*.h canvas_bridge.js
	@echo "==> Compiling Surf CYD to WebAssembly…"
	$(EMCC) $(CXXFLAGS) $(EMFLAGS) $(SRCS) -o $(OUTPUT)
	@echo "==> Done: $(OUTPUT) + surfcyd.wasm"
	@echo "==> Serve with:  make serve"

clean:
	rm -f surfcyd.js surfcyd.wasm

# Start a minimal local server so the browser can load the WASM file.
# (file:// is blocked for WASM by default in modern browsers.)
serve: $(OUTPUT)
	@echo "==> Open http://localhost:8000/index.html"
	python3 -m http.server 8000

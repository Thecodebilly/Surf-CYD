// main_emulator.cpp
// Provides the C++ main() that the Emscripten runtime calls.
// It mirrors the hidden Arduino main(): call setup() once, then loop() forever.
// ASYNCIFY transforms the stack so that every emscripten_sleep() (delay())
// properly yields to the browser event-loop.

#include <emscripten.h>

extern void setup();
extern void loop();

int main() {
    setup();
    while (true) {
        loop();
    }
    return 0;
}

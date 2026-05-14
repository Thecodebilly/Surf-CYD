// main_emulator.cpp
// Provides the C++ main() that the Emscripten runtime calls.
// It mirrors the hidden Arduino main(): call setup() once, then loop() forever.
// ASYNCIFY transforms the stack so that every emscripten_sleep() (delay())
// properly yields to the browser event-loop.

#include <emscripten.h>

extern void setup();
extern void loop();

int main() {
    // Pre-seed player name so API submissions identify this as the emulator.
    // Only sets it if no name has been saved yet (preserves any prior override).
    EM_ASM({
        var key = 'spiffs:/player_name.json';
        if (!localStorage.getItem(key)) {
            localStorage.setItem(key, '{"name":"Emulator"}');
            var kl = JSON.parse(localStorage.getItem('spiffs:__keys__') || '[]');
            if (kl.indexOf('/player_name.json') === -1) {
                kl.push('/player_name.json');
                localStorage.setItem('spiffs:__keys__', JSON.stringify(kl));
            }
        }
    });
    setup();
    while (true) {
        loop();
    }
    return 0;
}

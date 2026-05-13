// canvas_bridge.js  –  Emscripten JS library loaded via --js-library.
// Provides Module.ctx, Module.rgb565(), Module.gfxText(), and the touch-event
// bookkeeping that the XPT2046 shim reads via EM_ASM.

mergeInto(LibraryManager.library, {

    // ── Placeholder (real setup happens in Module.onRuntimeInitialized) ──────
    // The actual canvas & context are wired up in index.html.
    // We just ensure the names exist so C++ EM_ASM can reference them.
    $surfEmulatorInit: function() {
        // Called once from index.html after the WASM runtime is ready.
    }
});

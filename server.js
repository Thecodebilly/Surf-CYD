const express = require('express');
const path = require('path');

const app = express();
const PORT = process.env.PORT || 8080;

// Serve .wasm files with the correct MIME type
app.use(function (req, res, next) {
  if (req.path.endsWith('.wasm')) {
    res.setHeader('Content-Type', 'application/wasm');
  }
  next();
});

const emulatorDir = path.join(__dirname, 'emulator');

// Redirect /SurfBoard → /SurfBoard/ so relative asset paths resolve correctly
app.get('/SurfBoard', (req, res) => res.redirect(301, '/SurfBoard/'));

// Serve under /SurfBoard (proxied from main site) and at root (direct access)
app.use('/SurfBoard', express.static(emulatorDir));
app.use('/', express.static(emulatorDir));

app.listen(PORT, function () {
  console.log('Surf CYD server listening on port ' + PORT);
});

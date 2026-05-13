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

// Serve the emulator directory as the site root
app.use(express.static(path.join(__dirname, 'emulator')));

app.listen(PORT, function () {
  console.log('Surf CYD server listening on port ' + PORT);
});

#pragma once
#include <Arduino.h>

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>Visor Control</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: sans-serif; background:#111; color:#eee; text-align:center; padding:20px; }
    h2 { letter-spacing:2px; }
    h3 { font-size:13px; color:#888; text-transform:uppercase; letter-spacing:2px; margin:22px 0 8px; }
    input[type=color] { width:100px; height:60px; border:none; background:none; }
    input[type=range] { width:80%; max-width:320px; accent-color:#a3f; }
    label { font-size:14px; color:#aaa; }
    button { padding:12px 20px; margin:6px; font-size:16px; border-radius:8px; border:none; background:#333; color:#eee; }
    button.active { background:#a3f; color:#fff; }
    .toggles { margin-top:14px; font-size:14px; color:#aaa; }
    .toggles label { margin:0 10px; }
  </style>
</head>
<body>
  <h2>Murder Drones Visor</h2>

  <h3>Expression</h3>
  <div id="exprRow">
    <button data-expr="neutral">Neutral</button>
    <button data-expr="angry">Angry</button>
    <button data-expr="x">X</button>
    <button data-expr="happy">Happy</button>
  </div>

  <h3>Mode</h3>
  <div id="modeRow">
    <button data-mode="eyes">Eyes</button>
    <button data-mode="rainbow">Rainbow</button>
    <button data-mode="off">Off</button>
    <button data-mode="test">Test</button>
  </div>

  <h3>Colour</h3>
  <input type="color" id="colorPicker" value="#ff00ff">

  <h3>Brightness</h3>
  <label for="brightness"><span id="brightVal">80</span></label><br>
  <input type="range" id="brightness" min="1" max="255" value="80">

  <div class="toggles">
    <label><input type="checkbox" id="pulse"> Pulse</label>
    <label><input type="checkbox" id="idle" checked> Idle motion</label>
  </div>

  <script>
    const $ = id => document.getElementById(id);

    function mark(rowId, attr, value) {
      for (const b of $(rowId).children) b.classList.toggle('active', b.dataset[attr] === value);
    }

    for (const b of $('exprRow').children) {
      b.onclick = () => { fetch('/setexpression?e=' + b.dataset.expr); mark('exprRow', 'expr', b.dataset.expr); mark('modeRow', 'mode', 'eyes'); };
    }
    for (const b of $('modeRow').children) {
      b.onclick = () => { fetch('/setmode?mode=' + b.dataset.mode); mark('modeRow', 'mode', b.dataset.mode); };
    }

    $('colorPicker').addEventListener('input', () => {
      const hex = $('colorPicker').value.substring(1);
      const r = parseInt(hex.substring(0, 2), 16);
      const g = parseInt(hex.substring(2, 4), 16);
      const b = parseInt(hex.substring(4, 6), 16);
      fetch(`/setcolor?r=${r}&g=${g}&b=${b}`);
    });

    $('pulse').onchange = () => fetch('/settoggle?pulse=' + ($('pulse').checked ? 1 : 0));
    $('idle').onchange  = () => fetch('/settoggle?idle='  + ($('idle').checked  ? 1 : 0));

    // Dragging the slider fires 'input' far faster than the ESP32 can service
    // requests, so coalesce: one in flight at a time, always send the last value.
    const bright = $('brightness');
    let pending = null, inflight = false;

    function sendBrightness(v) {
      if (inflight) { pending = v; return; }
      inflight = true;
      fetch(`/setbrightness?v=${v}`).finally(() => {
        inflight = false;
        if (pending !== null) { const next = pending; pending = null; sendBrightness(next); }
      });
    }

    bright.addEventListener('input', () => {
      $('brightVal').textContent = bright.value;
      sendBrightness(bright.value);
    });

    // Sync the UI to whatever the visor is actually doing.
    fetch('/state').then(r => r.json()).then(s => {
      mark('exprRow', 'expr', s.expression);
      mark('modeRow', 'mode', s.mode);
      $('colorPicker').value = '#' + s.color;
      bright.value = s.brightness;
      $('brightVal').textContent = s.brightness;
      $('pulse').checked = s.pulse;
      $('idle').checked = s.idle;
    });
  </script>
</body>
</html>
)rawliteral";

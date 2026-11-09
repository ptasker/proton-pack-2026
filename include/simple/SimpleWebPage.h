#pragma once

// Simple pack control page served at http://192.168.5.1/ (see SimpleWeb.h).
// Self-contained: no external files, fonts, or internet access needed.
static const char SIMPLE_WEB_PAGE[] = R"PAGE(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Proton Pack</title>
<style>
:root{--bg:#111;--card:#1d1d1d;--text:#eee;--dim:#888;--on:#3c3;--off:#555;--warn:#e93;--hot:#e33;--accent:#e33}
*{box-sizing:border-box}
body{margin:0;font-family:-apple-system,system-ui,sans-serif;background:var(--bg);color:var(--text)}
header{padding:14px 16px;background:#000;border-bottom:2px solid var(--accent);display:flex;justify-content:space-between;align-items:center}
h1{font-size:1.2em;margin:0;letter-spacing:.05em}
#conn{font-size:.85em;color:var(--dim)}
main{padding:12px;max-width:520px;margin:auto}
section{background:var(--card);border-radius:10px;padding:12px 14px;margin-bottom:12px}
h2{font-size:.85em;text-transform:uppercase;color:var(--dim);margin:0 0 10px;letter-spacing:.08em}
.state{font-size:1.6em;font-weight:600}
.grid{display:grid;grid-template-columns:repeat(3,1fr);gap:8px;margin-top:10px}
.pill{text-align:center;padding:8px 4px;border-radius:8px;background:var(--off);font-size:.85em}
.pill.on{background:var(--on);color:#000}
.pill.warn{background:var(--warn);color:#000}
.pill.hot{background:var(--hot)}
.row{display:flex;justify-content:space-between;align-items:center;margin:8px 0;gap:10px}
button{font-size:1em;padding:10px 16px;border-radius:8px;border:0;background:#333;color:var(--text)}
button.primary{background:var(--accent);color:#fff}
button:active{opacity:.7}
input[type=range]{flex:1}
select{flex:1;font-size:1em;padding:8px;border-radius:8px;background:#333;color:var(--text);border:0}
.small{font-size:.8em;color:var(--dim)}
</style>
</head>
<body>
<header><h1 id="title">PROTON PACK</h1><span id="conn">connecting...</span></header>
<main>
<section>
<h2>Status</h2>
<div class="state" id="pack">-</div>
<div class="grid">
<div class="pill" id="w-activate">Activate</div>
<div class="pill" id="w-firing">Firing</div>
<div class="pill" id="w-cooling">Overheat</div>
</div>
</section>
<section>
<h2>Theme</h2>
<div class="row"><select id="theme">
<option value="0">1984 (Ghostbusters)</option>
<option value="1">1989 (Ghostbusters II)</option>
<option value="2">Afterlife</option>
<option value="3">Frozen Empire</option>
</select></div>
<div class="small">Sounds, cyclotron style and timings. Saved on the pack.</div>
</section>
<section>
<h2>Sound</h2>
<div class="row"><span id="muteLabel">Sound on</span><button id="mute" class="primary">Mute</button></div>
<div class="row"><span>Volume</span><input type="range" id="vol" min="-40" max="0" step="1"><span id="volLabel">0 dB</span></div>
</section>
<section>
<h2>Music</h2>
<div class="row"><select id="track"><option>Loading...</option></select></div>
<div class="row"><span id="musicLabel">Stopped</span><span><button id="play" class="primary">Play</button> <button id="stop">Stop</button></span></div>
</section>
<section>
<h2>System</h2>
<div class="small" id="sys">-</div>
</section>
</main>
<script>
const $=id=>document.getElementById(id);
let status=null,dragging=false,indexVersion=-1,failures=0;
async function post(path){
  try{const r=await fetch(path,{method:'POST'});if(!r.ok)throw r.status;setTimeout(poll,150);}
  catch(e){$('conn').textContent='command failed';}
}
function pill(id,on,cls){const el=$(id);el.className='pill'+(on?' '+(cls||'on'):'');}
function render(s){
  status=s;
  $('title').textContent=s.name;
  $('pack').textContent=s.pack+' \u2022 '+s.themeName;
  if(document.activeElement!==$('theme'))$('theme').value=s.theme;
  pill('w-activate',s.wand.activate);pill('w-firing',s.wand.firing,'warn');pill('w-cooling',s.wand.cooling,'hot');
  $('muteLabel').textContent=s.muted?'Sound MUTED':'Sound on';
  $('mute').textContent=s.muted?'Unmute':'Mute';
  if(!dragging){$('vol').value=s.volume;$('volLabel').textContent=s.volume+' dB';}
  $('musicLabel').textContent=s.music?'Playing '+String(s.music).padStart(3,'0'):'Stopped';
  $('sys').textContent=(s.sd.mounted?'SD card: '+s.sd.tracks+' tracks':'SD card: NOT MOUNTED')+
    ' | Wi-Fi clients: '+s.clients+' | Up '+Math.floor(s.uptime/60000)+' min'+
    ' | Last restart: '+s.reset+' | Restarts since power-on: '+s.restarts;
  if(s.sd.index!==indexVersion){indexVersion=s.sd.index;loadMusic();}
}
async function poll(){
  try{const r=await fetch('/api/status');render(await r.json());failures=0;$('conn').textContent='connected';}
  catch(e){if(++failures>2)$('conn').textContent='offline';}
}
async function loadMusic(){
  try{
    const list=await (await fetch('/api/music')).json();const sel=$('track');sel.innerHTML='';
    if(!list.length){sel.innerHTML='<option value="">No music tracks (500+)</option>';return;}
    for(const t of list){const o=document.createElement('option');o.value=t.n;o.textContent=t.name;sel.appendChild(o);}
  }catch(e){}
}
$('mute').onclick=()=>status&&post('/api/mute?on='+(status.muted?0:1));
$('vol').oninput=()=>{dragging=true;$('volLabel').textContent=$('vol').value+' dB';};
$('vol').onchange=()=>{dragging=false;post('/api/volume?db='+$('vol').value);};
$('play').onclick=()=>{const t=$('track').value;if(t)post('/api/music/play?track='+t);};
$('stop').onclick=()=>post('/api/music/stop');
$('theme').onchange=()=>post('/api/theme?t='+$('theme').value);
poll();setInterval(poll,500);
</script>
</body>
</html>
)PAGE";

#ifndef WEBUI_H
#define WEBUI_H

static const char WEBUI_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>AHRS-GDL90 Settings</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{background:#1a1a2e;color:#e0e0e0;font-family:-apple-system,system-ui,sans-serif;font-size:14px;padding:12px;max-width:520px;margin:0 auto}
h1{font-size:18px;color:#4fc3f7;margin-bottom:12px;text-align:center}
h2{font-size:15px;color:#81d4fa;margin:16px 0 8px;padding-bottom:4px;border-bottom:1px solid #333}
.card{background:#16213e;border:1px solid #333;border-radius:8px;padding:12px;margin-bottom:12px}
.row{display:flex;justify-content:space-between;padding:3px 0}
.row .label{color:#999}
.row .val{color:#e0e0e0;font-family:monospace}
.ok{color:#66bb6a}
.err{color:#ef5350}
.mode-badge{display:inline-block;padding:2px 8px;border-radius:4px;font-weight:bold;font-size:13px}
.mode-standalone{background:#1b5e20;color:#a5d6a7}
.mode-companion{background:#0d47a1;color:#90caf9}
label{display:flex;justify-content:space-between;align-items:center;padding:6px 0}
label span{flex:1}
select,input[type=text],input[type=password]{background:#0f3460;color:#e0e0e0;border:1px solid #555;border-radius:4px;padding:6px 8px;width:100%}
select{width:auto;min-width:180px}
input[type=checkbox]{width:18px;height:18px;accent-color:#4fc3f7}
.field{margin-bottom:8px}
.field .lbl{font-size:12px;color:#999;margin-bottom:2px}
.btn-row{display:flex;gap:8px;margin-top:12px}
button{flex:1;padding:10px;border:none;border-radius:6px;font-size:14px;font-weight:bold;cursor:pointer}
#btnSave{background:#1565c0;color:#fff}
#btnSave:hover{background:#1976d2}
#btnReboot{background:#b71c1c;color:#fff}
#btnReboot:hover{background:#c62828}
#msg{text-align:center;margin-top:8px;font-size:13px;min-height:18px}
.sensors{display:flex;gap:12px;margin-top:4px}
.sensor{padding:2px 6px;border-radius:3px;font-size:12px;font-weight:bold}
</style>
</head>
<body>
<h1>AHRS-GDL90</h1>

<!-- Status Section -->
<div class="card" id="status">
<h2>Live Status</h2>
<div class="row"><span class="label">Mode</span><span class="val" id="sMode">---</span></div>
<div class="row"><span class="label">Roll</span><span class="val" id="sRoll">---</span></div>
<div class="row"><span class="label">Pitch</span><span class="val" id="sPitch">---</span></div>
<div class="row"><span class="label">Heading</span><span class="val" id="sHeading">---</span></div>
<div class="row"><span class="label">Heading Source</span><span class="val" id="sHdgSrc">---</span></div>
<div class="row"><span class="label">Baro Alt</span><span class="val" id="sAlt">---</span></div>
<div class="row"><span class="label">GPS</span><span class="val" id="sGPS">---</span></div>
<div class="row"><span class="label">Satellites</span><span class="val" id="sSats">---</span></div>
<div class="row"><span class="label">WiFi RSSI</span><span class="val" id="sRSSI">---</span></div>
<div class="row"><span class="label">Clients</span><span class="val" id="sClients">---</span></div>
<div class="row"><span class="label">Uptime</span><span class="val" id="sUptime">---</span></div>
<div class="sensors" id="sSensors"></div>
</div>

<!-- Settings Section -->
<div class="card">
<h2>Settings</h2>

<div class="field">
<div class="lbl">Operating Mode</div>
<select id="cfgMode">
<option value="0">Auto (scan for Stratux)</option>
<option value="1">Force Standalone</option>
<option value="2">Force Companion</option>
</select>
</div>

<div class="field">
<div class="lbl">AP SSID (Standalone)</div>
<input type="text" id="cfgApSSID" maxlength="31">
</div>

<div class="field">
<div class="lbl">AP Password (Standalone)</div>
<input type="password" id="cfgApPass" maxlength="31">
</div>

<div class="field">
<div class="lbl">Stratux SSID (Companion)</div>
<input type="text" id="cfgStratuxSSID" maxlength="31">
</div>

<h2>Messages</h2>
<label><span>AHRS (0x65/0x01)</span><input type="checkbox" id="cfgAHRS"></label>
<label><span>Ownship Report (0x0A + 0x0B)</span><input type="checkbox" id="cfgOwnship"></label>
<label><span>Heartbeat (0x00)</span><input type="checkbox" id="cfgHeartbeat"></label>
<label><span>ForeFlight ID (0x65/0x00)</span><input type="checkbox" id="cfgFFID"></label>
<label><span>XGPS (port 49002)</span><input type="checkbox" id="cfgXGPS"></label>
<label><span>XATT (port 49002)</span><input type="checkbox" id="cfgXATT"></label>

<h2>Sensor Options</h2>
<label><span>Invert Roll</span><input type="checkbox" id="cfgInvertRoll"></label>

<div class="btn-row">
<button id="btnSave" onclick="saveSettings()">Save</button>
<button id="btnReboot" onclick="reboot()">Reboot</button>
</div>
<div id="msg"></div>
</div>

<script>
function $(id){return document.getElementById(id)}
function fmtTime(ms){
  var s=Math.floor(ms/1000),m=Math.floor(s/60),h=Math.floor(m/60);
  return h+'h '+m%60+'m '+s%60+'s';
}

function updateStatus(){
  fetch('/api/status').then(function(r){return r.json()}).then(function(d){
    $('sMode').innerHTML='<span class="mode-badge mode-'+(d.mode==='COMPANION'?'companion':'standalone')+'">'+d.mode+'</span>';
    $('sRoll').textContent=d.roll.toFixed(1)+'\u00B0';
    $('sPitch').textContent=d.pitch.toFixed(1)+'\u00B0';
    $('sHeading').textContent=d.heading.toFixed(1)+'\u00B0';
    $('sHdgSrc').textContent=d.headingSource;
    $('sAlt').textContent=d.pressAltFt+' ft';
    $('sGPS').textContent=d.gpsFix?'FIX':'NO FIX';
    $('sGPS').className='val '+(d.gpsFix?'ok':'err');
    $('sSats').textContent=d.satellites;
    $('sRSSI').textContent=d.wifiRSSI+' dBm';
    $('sClients').textContent=d.clients;
    $('sUptime').textContent=fmtTime(d.uptimeMs);
    var sh='';
    sh+='<span class="sensor" style="background:'+(d.bno086ok?'#1b5e20':'#b71c1c')+'">BNO086</span>';
    sh+='<span class="sensor" style="background:'+(d.bmp390ok?'#1b5e20':'#b71c1c')+'">BMP390</span>';
    sh+='<span class="sensor" style="background:'+(d.gpsFix?'#1b5e20':'#e65100')+'">GPS</span>';
    $('sSensors').innerHTML=sh;
  }).catch(function(){});
}

function loadSettings(){
  fetch('/api/settings').then(function(r){return r.json()}).then(function(d){
    $('cfgMode').value=d.mode;
    $('cfgApSSID').value=d.apSSID;
    $('cfgApPass').value=d.apPassword;
    $('cfgStratuxSSID').value=d.stratuxSSID;
    $('cfgAHRS').checked=d.sendAHRS;
    $('cfgOwnship').checked=d.sendOwnship;
    $('cfgHeartbeat').checked=d.sendHeartbeat;
    $('cfgFFID').checked=d.sendFFID;
    $('cfgXGPS').checked=d.sendXGPS;
    $('cfgXATT').checked=d.sendXATT;
    $('cfgInvertRoll').checked=d.invertRoll;
  }).catch(function(){$('msg').textContent='Failed to load settings';});
}

function saveSettings(){
  var body=JSON.stringify({
    mode:parseInt($('cfgMode').value),
    apSSID:$('cfgApSSID').value,
    apPassword:$('cfgApPass').value,
    stratuxSSID:$('cfgStratuxSSID').value,
    sendAHRS:$('cfgAHRS').checked,
    sendOwnship:$('cfgOwnship').checked,
    sendHeartbeat:$('cfgHeartbeat').checked,
    sendFFID:$('cfgFFID').checked,
    sendXGPS:$('cfgXGPS').checked,
    sendXATT:$('cfgXATT').checked,
    invertRoll:$('cfgInvertRoll').checked
  });
  fetch('/api/settings',{method:'POST',headers:{'Content-Type':'application/json'},body:body})
    .then(function(r){return r.json()})
    .then(function(d){
      $('msg').textContent=d.ok?'Saved \u2014 reboot to apply':'Save failed';
      $('msg').style.color=d.ok?'#66bb6a':'#ef5350';
    })
    .catch(function(){$('msg').textContent='Save failed';$('msg').style.color='#ef5350';});
}

function reboot(){
  if(!confirm('Reboot the device?'))return;
  fetch('/api/reboot',{method:'POST'}).catch(function(){});
  $('msg').textContent='Rebooting...';
  $('msg').style.color='#ffb74d';
}

loadSettings();
updateStatus();
setInterval(updateStatus,1000);
</script>
</body>
</html>
)rawliteral";

#endif

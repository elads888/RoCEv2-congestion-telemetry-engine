#include "ui_html.h"

// =============================================================================
// ui_html.cpp — Embedded UI HTML definition
// Served on GET / so the demo requires only a binary and a browser.
// JavaScript fetches /state every 200ms and POSTs to /rate on slider input.
// All numbers displayed in the UI come from real C++ data through the
// zero-copy bridge — not from a separate JavaScript simulation.
// =============================================================================

const std::string UI_HTML = R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>RoCEv2 Telemetry</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:'IBM Plex Mono',monospace,'Courier New';background:#080c18;color:#b8cce0;min-height:100vh;padding:1rem}
.app{max-width:1080px;margin:0 auto;display:flex;flex-direction:column;gap:.75rem}
.header{display:flex;align-items:center;justify-content:space-between;padding:.6rem 1rem;background:#0d1220;border:1px solid #1a2840;border-radius:7px}
.htitle{font-size:11px;font-weight:600;letter-spacing:.14em;color:#5a94c0;text-transform:uppercase}
.hst{display:flex;align-items:center;gap:7px;font-size:10px;color:#304860}
.dot{width:7px;height:7px;border-radius:50%;background:#00c060}
@keyframes pulse{0%,100%{opacity:1}50%{opacity:.35}}
.blink{animation:pulse 1.3s ease-in-out infinite}
.panels{display:grid;grid-template-columns:1fr 28px 1fr 28px 1fr}
.arrow{display:flex;align-items:center;justify-content:center;color:#1a3050;font-size:16px;padding-top:1.4rem}
.panel{background:#0d1220;border:1px solid #1a2840;border-radius:7px;padding:.875rem;display:flex;flex-direction:column;gap:.55rem;transition:border-color .3s,box-shadow .3s}
.panel.warn{border-color:#906000;box-shadow:0 0 0 1px rgba(140,90,0,.18)}
.panel.crit{border-color:#780a0a;box-shadow:0 0 0 1px rgba(180,15,15,.18)}
.plbl{font-size:9px;font-weight:600;letter-spacing:.14em;text-transform:uppercase;color:#243848}
.pname{font-size:14px;font-weight:600;color:#5a94c0;margin-top:1px}
.bnum{font-size:26px;font-weight:600;color:#d8ecff;line-height:1;letter-spacing:-.02em}
.bunit{font-size:10px;color:#304860;margin-left:3px;font-weight:400}
.sr{display:flex;justify-content:space-between;align-items:center;padding:3px 0;border-bottom:1px solid #0c1828;font-size:10px}
.sr:last-child{border-bottom:none}
.sl{color:#304860}.sv{font-weight:600;color:#6898b8}
.sv.g{color:#00b050}.sv.r{color:#c82020}.sv.a{color:#c06800}
.btk{width:38px;height:130px;background:#050b14;border:1px solid #162030;border-radius:4px;position:relative;overflow:hidden;flex-shrink:0}
.bfill{position:absolute;bottom:0;width:100%;border-radius:3px 3px 0 0;transition:height .2s ease,background .2s ease}
.tl{position:absolute;width:calc(100% + 36px);height:1px;background:#b06000;opacity:.45;left:0}
.dl{position:absolute;top:0;width:calc(100% + 28px);height:1px;background:#a02020;opacity:.45;left:0}
.tlbl{position:absolute;left:calc(100% + 3px);font-size:7px;color:#b06000;white-space:nowrap;transform:translateY(4px)}
.dlbl{position:absolute;top:-1px;left:calc(100% + 3px);font-size:7px;color:#a02020;white-space:nowrap}
.bpct{font-size:16px;font-weight:600;text-align:center;margin-top:4px;transition:color .25s;font-family:monospace}
.ecnb{display:inline-flex;align-items:center;gap:4px;font-size:9px;font-weight:600;padding:2px 7px;border-radius:3px;letter-spacing:.08em;text-transform:uppercase;border:1px solid}
.ecnb.off{background:#08120a;color:#185a18;border-color:#183818}
.ecnb.on{background:#1e0606;color:#d03030;border-color:#580808}
.flow-box{position:relative;height:50px;background:#050b14;border:1px solid #162030;border-radius:7px;overflow:hidden}
.flow-lbl{position:absolute;top:50%;transform:translateY(-50%);font-size:8px;color:#182838;letter-spacing:.1em;text-transform:uppercase;pointer-events:none}
.flow-line{position:absolute;top:50%;left:0;right:0;height:1px;background:#0f1e2e;transform:translateY(-50%)}
.pdot{position:absolute;width:9px;height:9px;border-radius:2px;top:50%;transform:translateY(-50%) translateX(-50%);pointer-events:none}
.rate-box{background:#0d1220;border:1px solid #1a2840;border-radius:7px;padding:.875rem 1rem}
.rh{display:flex;justify-content:space-between;align-items:center;margin-bottom:.55rem}
.rtt{font-size:9px;font-weight:600;letter-spacing:.14em;text-transform:uppercase;color:#243848}
.rv{font-size:19px;font-weight:600;color:#d8ecff;font-family:monospace}
.rv span{font-size:10px;color:#304860;margin-left:3px}
input[type=range]{width:100%;height:4px;-webkit-appearance:none;appearance:none;background:#162030;border-radius:2px;outline:none}
input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;width:15px;height:15px;border-radius:50%;cursor:pointer;border:2px solid #080c18;transition:transform .1s,background .2s}
input[type=range].ok::-webkit-slider-thumb{background:#009840}
input[type=range].wa::-webkit-slider-thumb{background:#c06800}
input[type=range].cr::-webkit-slider-thumb{background:#c02020}
.rticks{display:flex;justify-content:space-between;font-size:8px;color:#1a3050;margin-top:3px}
.legend{display:flex;gap:.875rem;margin-top:.5rem;flex-wrap:wrap}
.leg{font-size:9px;color:#3a5a78;display:flex;align-items:center;gap:4px}
.leg-dot{width:8px;height:8px;border-radius:1px;display:inline-block;flex-shrink:0}
.cbar{height:5px;background:#050b14;border-radius:3px;overflow:hidden;border:1px solid #162030;margin-top:4px}
.cbf{height:100%;border-radius:3px;transition:width .35s ease,background .3s}
.pkt-grid{display:flex;flex-wrap:wrap;gap:2px;min-height:36px;align-content:flex-start}
.pk{width:11px;height:11px;border-radius:2px;flex-shrink:0}
.pk.n{background:#083518}.pk.e{background:#420a0a}
.alert{background:#140606;border:1px solid #560a0a;border-radius:7px;padding:.7rem .875rem;display:flex;align-items:flex-start;gap:.55rem;transition:opacity .3s,transform .3s}
.alert.hide{opacity:0;pointer-events:none;transform:translateY(-3px)}
.adot{width:7px;height:7px;border-radius:50%;background:#d02020;margin-top:3px;flex-shrink:0}
.atxt{font-size:11px;color:#a04040;line-height:1.5}
.atxt strong{color:#d06060;display:block;font-size:12px;margin-bottom:2px}
.conn-err{background:#140a04;border:1px solid #563010;border-radius:7px;padding:.7rem .875rem;font-size:11px;color:#a06030;transition:opacity .3s}
.conn-err.hide{opacity:0;pointer-events:none}
</style>
</head>
<body>
<div class="app">

<div class="header">
  <div class="htitle">RoCEv2 Telemetry — Live Demo</div>
  <div class="hst"><div class="dot blink" id="liveDot"></div><span id="hst">connecting...</span></div>
</div>

<div class="conn-err hide" id="connErr">
  Cannot reach the telemetry server. Is the integrated_demo binary running?
  Run: sudo bash scripts/run_demo.sh
</div>

<div class="panels">
  <div class="panel" id="pA">
    <div><div class="plbl">Node A — GPU Server</div><div class="pname">Sender</div></div>
    <div>
      <div class="bnum" id="rateNum">—<span class="bunit">pps</span></div>
      <div style="font-size:9px;color:#243848;margin-top:2px">transmission rate</div>
    </div>
    <div style="border-top:1px solid #0c1828;padding-top:.45rem">
      <div class="sr"><span class="sl">Total sent</span><span class="sv" id="tSent">—</span></div>
      <div class="sr"><span class="sl">Switch drain</span><span class="sv g" id="drainRate">—</span></div>
      <div class="sr"><span class="sl">Protocol</span><span class="sv">RoCEv2 / RDMA</span></div>
    </div>
  </div>

  <div class="arrow">&#10132;</div>

  <div class="panel" id="pB">
    <div style="display:flex;justify-content:space-between;align-items:flex-start">
      <div><div class="plbl">Spine Switch</div><div class="pname">Switch</div></div>
      <div class="ecnb off" id="ecnB">ECN off</div>
    </div>
    <div style="display:flex;gap:.75rem;align-items:flex-end">
      <div>
        <div class="btk">
          <div class="bfill" id="bFill" style="height:0%;background:#083518"></div>
          <div class="tl" style="bottom:70%"><span class="tlbl">70% RED</span></div>
          <div class="dl"><span class="dlbl">drop</span></div>
        </div>
        <div class="bpct" id="bPct" style="color:#00b050">0%</div>
      </div>
      <div style="flex:1;display:flex;flex-direction:column;gap:0;justify-content:flex-end">
        <div class="sr"><span class="sl">Buffer</span><span class="sv" id="bSlots">—</span></div>
        <div class="sr"><span class="sl">ECN marked</span><span class="sv r" id="ecnMk">—</span></div>
        <div class="sr"><span class="sl">Cong. rate</span><span class="sv" id="cRt">—</span></div>
      </div>
    </div>
  </div>

  <div class="arrow">&#10132;</div>

  <div class="panel" id="pC">
    <div><div class="plbl">Node B — Telemetry Engine</div><div class="pname">Receiver</div></div>
    <div>
      <div class="bnum" id="tRx">—</div>
      <div style="font-size:9px;color:#243848;margin-top:2px">packets received</div>
    </div>
    <div>
      <div style="display:flex;justify-content:space-between;font-size:9px;color:#304860;margin-bottom:3px">
        <span>Congestion rate (100ms window)</span><span id="cRxPct">—</span>
      </div>
      <div class="cbar"><div class="cbf" id="cBar" style="width:0%;background:#009840"></div></div>
    </div>
    <div>
      <div style="font-size:8px;color:#243848;letter-spacing:.1em;text-transform:uppercase;margin-bottom:4px">Packet stream — last 40</div>
      <div class="pkt-grid" id="pktGrid"></div>
    </div>
    <div style="margin-top:auto;border-top:1px solid #0c1828;padding-top:.45rem">
      <div class="sr"><span class="sl">Normal pkts</span><span class="sv g" id="normPk">—</span></div>
      <div class="sr"><span class="sl">ECN-marked</span><span class="sv r" id="congPk">—</span></div>
      <div class="sr"><span class="sl">Alert threshold</span><span class="sv">20% / 100ms</span></div>
    </div>
  </div>
</div>

<div class="flow-box" id="flowBox">
  <span class="flow-lbl" style="left:1%">sender</span>
  <span class="flow-lbl" style="right:1%;left:auto">receiver</span>
  <div class="flow-line"></div>
</div>

<div class="rate-box">
  <div class="rh">
    <div class="rtt">Transmission rate — drag to simulate congestion control</div>
    <div class="rv" id="rvDisp">—<span>pps</span></div>
  </div>
  <input type="range" id="slider" min="200" max="6000" step="100" value="3000" class="ok">
  <div class="rticks"><span>200</span><span>2k</span><span>3k</span><span>4k</span><span>5k</span><span>6k</span></div>
  <div class="legend">
    <span class="leg"><span class="leg-dot" style="background:#083518"></span>Below 5,000 pps — buffer drains, no ECN</span>
    <span class="leg"><span class="leg-dot" style="background:#4a2800"></span>5,000–6,000 pps — buffer fills, ECN at 70%</span>
    <span class="leg"><span class="leg-dot" style="background:#420a0a"></span>Above 6,000 pps — heavy congestion + drops</span>
  </div>
</div>

<div class="alert hide" id="alertBox">
  <div class="adot blink"></div>
  <div class="atxt">
    <strong>DCQCN intervention required — congestion rate above 20%</strong>
    Switch buffer has been above the ECN threshold for more than 100ms.
    DCQCN has failed to self-correct. Lower the transmission rate to drain
    the switch buffer. Watch the buffer level fall and packets return to green.
  </div>
</div>

</div>

<script>
const TRAVEL=1600, POLL_MS=200;
let pkts=[], nid=0, lf=0;

function fmt(n) {
  if(n===undefined||n===null) return '—';
  if(n>=1e6) return (n/1e6).toFixed(1)+'M';
  if(n>=1e3) return (n/1e3).toFixed(1)+'k';
  return Math.round(n).toLocaleString();
}
function fmtFull(n) { return n===undefined ? '—' : Math.round(n).toLocaleString(); }

function applyState(d) {
  const f = d.buffer_fill_pct / 100;
  const fp = Math.round(d.buffer_fill_pct);
  const bg = f<0.7 ? '#083518' : f<0.9 ? '#4a2800' : '#420808';
  const col = f<0.7 ? '#00b050' : f<0.9 ? '#c06800' : '#c82020';

  document.getElementById('bFill').style.height = fp + '%';
  document.getElementById('bFill').style.background = bg;
  document.getElementById('bPct').textContent = fp + '%';
  document.getElementById('bPct').style.color = col;
  document.getElementById('bSlots').textContent = d.buffer_slots + '/' + d.buffer_capacity;
  document.getElementById('ecnMk').textContent = fmt(d.total_congested);

  const cr = d.congestion_rate;
  const crC = cr<5 ? '#00a048' : cr<20 ? '#c06800' : '#c82020';
  document.getElementById('cRt').textContent = cr.toFixed(1) + '%';
  document.getElementById('cRt').style.color = crC;
  document.getElementById('cRxPct').textContent = cr.toFixed(1) + '%';
  document.getElementById('cBar').style.width = Math.min(100, cr * 4) + '%';
  document.getElementById('cBar').style.background = crC;

  const eb = document.getElementById('ecnB');
  eb.className = 'ecnb ' + (d.ecn_active ? 'on' : 'off');
  eb.textContent = d.ecn_active ? 'ECN active' : 'ECN off';

  document.getElementById('pB').className = 'panel' +
    (d.dropping ? ' crit' : d.ecn_active ? ' warn' : '');

  document.getElementById('rateNum').innerHTML =
    fmtFull(d.sender_rate) + '<span class="bunit">pps</span>';
  document.getElementById('tSent').textContent = fmt(d.total_sent);
  document.getElementById('tRx').textContent = fmtFull(d.total_received);
  document.getElementById('normPk').textContent = fmt(d.total_received - d.total_congested);
  document.getElementById('congPk').textContent = fmt(d.total_congested);
  document.getElementById('drainRate').textContent = fmt(d.drain_rate) + ' pps';
  document.getElementById('rvDisp').innerHTML =
    fmtFull(d.sender_rate) + '<span>pps</span>';
  document.getElementById('slider').value = d.sender_rate;

  const sv = d.sender_rate;
  document.getElementById('slider').className = sv>6000 ? 'cr' : sv>5000 ? 'wa' : 'ok';

  if (d.recent_packets) {
    document.getElementById('pktGrid').innerHTML = d.recent_packets.map(p =>
      `<div class="pk ${p.ecn?'e':'n'}" title="seq ${p.seq}${p.ecn?' ECN':''}"></div>`
    ).join('');

    // Spawn animated packet dots across the flow lane
    const cnt = Math.max(1, Math.round(d.sender_rate / 2500));
    for (let i = 0; i < cnt; i++) {
      pkts.push({
        id: nid++,
        ecn: d.ecn_active && Math.random() < 0.5,
        born: performance.now() + i * (POLL_MS / cnt),
        el: null
      });
    }
  }

  document.getElementById('alertBox').className =
    'alert' + (d.alert_active ? '' : ' hide');
  document.getElementById('hst').textContent =
    d.dropping ? 'critical — drops active' :
    d.ecn_active ? 'warning — ECN active' : 'nominal';
}

function poll() {
  fetch('/state')
    .then(r => r.json())
    .then(d => {
      document.getElementById('connErr').className = 'conn-err hide';
      applyState(d);
    })
    .catch(() => {
      document.getElementById('connErr').className = 'conn-err';
      document.getElementById('hst').textContent = 'not connected';
    });
}

setInterval(poll, POLL_MS);
poll();

const sl = document.getElementById('slider');
let debounce = null;
sl.addEventListener('input', () => {
  const v = parseInt(sl.value);
  document.getElementById('rvDisp').innerHTML = v.toLocaleString() + '<span>pps</span>';
  sl.className = v>6000 ? 'cr' : v>5000 ? 'wa' : 'ok';
  clearTimeout(debounce);
  debounce = setTimeout(() => {
    fetch('/rate', {
      method: 'POST',
      headers: {'Content-Type': 'application/json'},
      body: JSON.stringify({rate: v})
    }).catch(() => {});
  }, 80);
});

function anim(ts) {
  requestAnimationFrame(anim);
  if (!lf) { lf = ts; return; }
  lf = ts;
  const box = document.getElementById('flowBox');
  const W = box.clientWidth;
  pkts = pkts.filter(p => {
    const age = ts - p.born;
    if (age < 0) return true;
    const prog = Math.min(1.1, age / TRAVEL);
    if (prog > 1.05) { if (p.el) p.el.remove(); return false; }
    if (!p.el) {
      p.el = document.createElement('div');
      p.el.className = 'pdot';
      box.appendChild(p.el);
    }
    const inSw = prog > .3 && prog < .7;
    const showE = p.ecn && prog > .38;
    p.el.style.background = showE ? '#b02020' : inSw ? '#285a28' : '#00b050';
    p.el.style.left = (prog * (W - 10)) + 'px';
    return true;
  });
}
requestAnimationFrame(anim);
</script>
</body>
</html>
)HTML";

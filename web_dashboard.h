/**
 * Dashboard web live per OBD2 Monitor.
 *
 * Fornisce gli endpoint HTTP:
 *   /dashboard  — pagina HTML dashboard con auto-refresh 500ms
 *   /data       — JSON con tutti i 47 parametri (diretti + calcolati)
 *   /debug      — toggle diagnostica seriale (?on/?off)
 *   /brightness — stato/override luminosità OLED (?set=N | ?auto)
 *   /clear-dtc  — cancella DTC (OBD2 Mode 04)
 *
 * L'handler /data legge i PID internamente, come printAdvancedDiagnostics().
 * Richiede che lo .ino definisca OBD_CONN_WIFI o OBD_CONN_CAN prima dell'include,
 * e che le funzioni queryOBDPID/queryPID siano gia' dichiarate.
 *
 * Usato da: WIFI_conn.ino, CANbus_conn.ino
 *
 * @since 01/04/26 Mattia Alesi
 */

#ifndef WEB_DASHBOARD_H
#define WEB_DASHBOARD_H

#include <ESP8266WebServer.h>
#include "dtc_descriptions.h"
#include "light_sensor.h"
#include "pid_descriptions.h"
#include "serial_logger.h"  // per TeeSerial::logReadFrom() nell'handler /serial-data

// ============================================================
// HTML della dashboard (PROGMEM)
// ============================================================

static const char DASHBOARD_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="it">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>OBD2 Dashboard</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{background:#111;color:#e0e0e0;font-family:'Segoe UI',-apple-system,Arial,sans-serif;padding:12px;max-width:480px;margin:0 auto}
h1{text-align:center;color:#00d4ff;font-size:1.2em;margin-bottom:4px}
.sub{text-align:center;color:#666;font-size:.75em;margin-bottom:12px}
.s{background:#1a1a2e;border-radius:8px;padding:10px 12px;margin-bottom:10px;border:1px solid #222}
.st{color:#00d4ff;font-size:.8em;font-weight:600;text-transform:uppercase;letter-spacing:1px;margin-bottom:6px;border-bottom:1px solid #222;padding-bottom:4px}
.g{display:grid;grid-template-columns:1fr 1fr;gap:4px 12px}
.i{display:flex;justify-content:space-between;align-items:baseline;padding:2px 0}
.l{color:#888;font-size:.75em}
.v{color:#fff;font-size:.9em;font-weight:600;font-variant-numeric:tabular-nums}
.mg{display:grid;grid-template-columns:1fr 1fr;gap:8px}
.mc{background:#0f1a30;border-radius:8px;padding:8px 10px;text-align:center;border:1px solid #1a3060}
.mc .l{font-size:.7em;color:#5588bb;margin-bottom:2px}
.mc .v{font-size:1.4em;color:#00d4ff}
.mc .u{font-size:.65em;color:#5588bb;margin-left:2px}
.db{background:#2a1010;border:2px solid #cc3333;border-radius:8px;padding:10px 12px;margin-bottom:10px}
.db .st{color:#ff5555;border-bottom-color:#551111}
.di{padding:4px 0;border-bottom:1px solid #331111}
.di:last-child{border-bottom:none}
.dc{color:#ff8888;font-weight:700;font-size:.95em}
.dd{color:#cc8888;font-size:.75em}
.mil{display:inline-block;background:#cc3333;color:#fff;font-size:.7em;font-weight:700;padding:2px 8px;border-radius:4px;margin-left:8px}
.db .st{display:flex;align-items:center;gap:8px}
.db .mil{flex:0 0 auto;margin-left:0;font-size:.85em;padding:5px 10px}
.hid{display:none}
.sb{text-align:center;font-size:.65em;color:#444;margin-top:8px}
.ok{color:#4a4}.w{color:#ca4}.al{color:#f44}.mc .v.al{color:#f44}.mc .v.w{color:#ca4}
.tg{display:flex;align-items:center;justify-content:center;gap:8px;margin-bottom:12px}
.tg label{color:#888;font-size:.8em}
.sw{position:relative;width:40px;height:22px}
.sw input{opacity:0;width:0;height:0}
.sl{position:absolute;cursor:pointer;top:0;left:0;right:0;bottom:0;background:#333;border-radius:22px;transition:.3s}
.sl:before{position:absolute;content:"";height:16px;width:16px;left:3px;bottom:3px;background:#888;border-radius:50%;transition:.3s}
.sw input:checked+.sl{background:#0a5}
.sw input:checked+.sl:before{transform:translateX(18px);background:#fff}
#btnClr{flex:1;background:#1a0808;color:#ff8888;border:1px solid #cc3333;border-radius:4px;padding:7px 10px;font-size:.9em;font-weight:700;cursor:pointer;letter-spacing:.5px}
#btnClr:hover{background:#cc3333;color:#fff}
#btnClr:active{background:#881111}
#btnClr:disabled{opacity:.5;cursor:wait}
.tabs{display:flex;gap:4px;margin-bottom:14px;border-bottom:1px solid #222}
.tabBtn{flex:1;background:#1a1a2e;color:#888;border:1px solid #222;border-bottom:none;border-radius:6px 6px 0 0;padding:8px 12px;font-size:.85em;font-weight:600;cursor:pointer;font-family:inherit}
.tabBtn.act{background:#0f1a30;color:#00d4ff;border-color:#1a3060}
.tabC{display:none}
.tabC.act{display:block}
.ecu{background:#1a1a2e;border-radius:8px;padding:10px 12px;margin-bottom:12px;border:1px solid #222}
.eh{display:flex;justify-content:space-between;align-items:baseline;border-bottom:1px solid #222;padding-bottom:6px;margin-bottom:8px}
.eid{color:#00d4ff;font-size:.95em;font-weight:600;letter-spacing:1px}
.ecnt{color:#5588bb;font-size:.78em}
.pl{display:flex;flex-wrap:wrap;gap:6px}
.pl span{background:#0f1a30;border:1px solid #1a3060;border-radius:4px;padding:3px 8px;font-size:.78em;color:#cfe6f5}
.empty{color:#888;font-style:italic;font-size:.85em}
.fnote{color:#666;font-size:.72em;text-align:center;margin-top:14px}
.smHdr{display:flex;justify-content:space-between;align-items:center;margin-bottom:6px}
.smBtn{background:#1a2a30;color:#8eddf0;border:1px solid #1a3060;border-radius:4px;padding:3px 10px;font-size:.72em;font-weight:600;cursor:pointer;font-family:inherit}
.smBtn:hover{background:#1a3060;color:#fff}
.smPre{background:#0a0a14;border:1px solid #222;border-radius:6px;padding:8px;height:60vh;overflow-y:auto;color:#cfe6f5;font-family:Consolas,Menlo,monospace;font-size:.78em;line-height:1.35;white-space:pre-wrap;word-break:break-all;margin:0;tab-size:4}
.smStat{color:#666;font-size:.7em;margin-top:6px;text-align:right}
.smStat.drp{color:#ca4}
</style>
</head>
<body>
<h1>OBD2 Dashboard</h1>
<p class="sub">Audi A5 B8 2.7 TDI CGKA</p>
<div class="tg"><label>Diagnostica Serial</label><label class="sw"><input type="checkbox" id="dbgTgl" onchange="fetch('/debug?'+(this.checked?'on':'off'))"><span class="sl"></span></label></div>
<p class="sub" id="dbgWarn" style="display:none;color:#ca4">Rallenta il refresh dei dati sul display</p>
<div class="tabs">
<button class="tabBtn act" id="tbD" onclick="swT('d')">Dashboard</button>
<button class="tabBtn" id="tbS" onclick="swT('s')">PID supportati</button>
<button class="tabBtn" id="tbL" onclick="swT('l')">Serial monitor</button>
</div>
<div class="tabC act" id="tcD">
<div class="s"><div class="st">Dati principali</div><div class="mg">
<div class="mc"><div class="l">BOOST</div><div class="v" id="d_boost">--</div><div class="u">bar</div></div>
<div class="mc"><div class="l">TEMP</div><div class="v" id="d_cool">--</div><div class="u">&deg;C</div></div>
<div class="mc"><div class="l">COPPIA</div><div class="v" id="d_torq">--</div><div class="u">Nm</div></div>
<div class="mc"><div class="l">EGR</div><div class="v" id="d_egr">--</div><div class="u"></div></div>
</div></div>
<div class="s"><div class="st">Luce ambiente</div>
<div class="g">
<div class="i"><span class="l">LDR</span><span class="v" id="d_ldr">--</span></div>
<div class="i"><span class="l">Contrasto</span><span class="v" id="d_ctr">--</span></div>
</div>
<div class="i" style="margin-top:6px"><span class="l">Modo</span><span class="v" id="d_brm">--</span></div>
<input type="range" min="0" max="255" id="brSld" oninput="brSet(this.value)" style="width:100%;margin-top:8px">
<button onclick="fetch('/brightness?auto')" style="width:100%;margin-top:6px;background:#1a2a30;color:#8eddf0;border:1px solid #1a3060;border-radius:4px;padding:6px;font-weight:600;cursor:pointer">Reset auto</button>
</div>
<div class="db hid" id="dtcBox"><div class="st">Errori DTC <span class="mil" id="milB">MIL</span><button id="btnClr" onclick="clrDtc()">CANCELLA</button></div><div id="dtcL"></div></div>
<div class="s debug" style="display:none"><div class="st">Motore</div><div class="g">
<div class="i"><span class="l">Carico</span><span class="v" id="d_load">--</span></div>
<div class="i"><span class="l">RPM</span><span class="v" id="d_rpm">--</span></div>
<div class="i"><span class="l">MAF</span><span class="v" id="d_maf">--</span></div>
<div class="i"><span class="l">Press. rail</span><span class="v" id="d_rail">--</span></div>
<div class="i"><span class="l">Lambda</span><span class="v" id="d_lam">--</span></div>
<div class="i"><span class="l">Air/Fuel Ratio</span><span class="v" id="d_afr">--</span></div>
<div class="i"><span class="l">Temp. liq.</span><span class="v" id="d_cool2">--</span></div>
<div class="i"><span class="l">IAT</span><span class="v" id="d_iat">--</span></div>
</div></div>
<div class="s debug" style="display:none"><div class="st">Veicolo</div><div class="g">
<div class="i"><span class="l">Velocit&agrave;</span><span class="v" id="d_spd">--</span></div>
<div class="i"><span class="l">Farfalla</span><span class="v" id="d_thr">--</span></div>
<div class="i"><span class="l">Rapporto CVT</span><span class="v" id="d_gear">--</span></div>
<div class="i"><span class="l">Acceleraz.</span><span class="v" id="d_acc">--</span></div>
</div>
<div class="i"><span class="l">Pedale D</span><span class="v" id="d_pD">--</span></div>
<div class="i"><span class="l">Pedale E</span><span class="v" id="d_pE">--</span></div>
</div>
<div class="s debug" style="display:none"><div class="st">Consumi</div><div class="g">
<div class="i"><span class="l">Consumo</span><span class="v" id="d_fl100">--</span></div>
<div class="i"><span class="l">Cons. specifico</span><span class="v" id="d_bsfc">--</span></div>
<div class="i"><span class="l">Potenza</span><span class="v" id="d_pow">--</span></div>
</div></div>
<div class="s debug" style="display:none"><div class="st">Turbo</div><div class="g">
<div class="i"><span class="l">Boost</span><span class="v" id="d_boost2">--</span></div>
<div class="i"><span class="l">Rapporto compr.</span><span class="v" id="d_pr">--</span></div>
<div class="i"><span class="l">Eff. intercooler</span><span class="v" id="d_ic">--</span></div>
<div class="i"><span class="l">Variaz. boost</span><span class="v" id="d_br">--</span></div>
</div></div>
<div class="s debug" style="display:none"><div class="st">Diagnostica</div><div class="g">
<div class="i"><span class="l">Batteria</span><span class="v" id="d_volt">--</span></div>
<div class="i"><span class="l">Drift pedale</span><span class="v" id="d_drift">--</span></div>
<div class="i"><span class="l">Taglio iniezione</span><span class="v" id="d_dfco">--</span></div>
<div class="i"><span class="l">Eff. volum.</span><span class="v" id="d_ve">--</span></div>
<div class="i"><span class="l">&Delta; farfalla</span><span class="v" id="d_dt">--</span></div>
</div></div>
<div class="s debug" style="display:none"><div class="st">Ambiente</div><div class="g">
<div class="i"><span class="l">Temp. esterna</span><span class="v" id="d_amb">--</span></div>
<div class="i"><span class="l">Press. baro</span><span class="v" id="d_bar">--</span></div>
<div class="i"><span class="l">Altitudine</span><span class="v" id="d_alt">--</span></div>
<div class="i"><span class="l">Densit&agrave; aria</span><span class="v" id="d_ad">--</span></div>
</div></div>
<div class="s debug" style="display:none"><div class="st">Contatori</div><div class="g">
<div class="i"><span class="l">Tempo motore</span><span class="v" id="d_rt">--</span></div>
<div class="i"><span class="l">Km con MIL</span><span class="v" id="d_km">--</span></div>
<div class="i"><span class="l">Avviamenti</span><span class="v" id="d_st">--</span></div>
</div></div>
<div class="sb">Auto-refresh 500ms</div>
</div>
<div class="tabC" id="tcS">
<div id="scR" class="empty">Carico elenco PID...</div>
<p class="fnote">I PID non documentati nel database SAE sono mostrati come "PID 0xXX".</p>
</div>
<div class="tabC" id="tcL">
<div class="s">
<div class="smHdr"><div class="st" style="border:none;padding:0;margin:0">Serial monitor</div><div><button class="smBtn" onclick="logScrollEnd()">A fine</button> <button class="smBtn" onclick="logClr()">Pulisci</button></div></div>
<pre id="smPre" class="smPre"></pre>
<p class="smStat" id="smStat">In attesa...</p>
</div>
</div>
<script>
var scLoaded=false;
var TABS={d:['tcD','tbD'],s:['tcS','tbS'],l:['tcL','tbL']};
function swT(w){for(var k in TABS){var c=document.getElementById(TABS[k][0]),b=document.getElementById(TABS[k][1]);if(k===w){c.classList.add('act');b.classList.add('act');}else{c.classList.remove('act');b.classList.remove('act');}}if(w==='s'&&!scLoaded)loadScan();if(w==='l')logStart();else logStop();}
function loadScan(){fetch('/scan-data').then(r=>r.json()).then(d=>{var r=document.getElementById('scR');if(!d.ecus||d.ecus.length===0){r.innerHTML='<p class="empty">Nessun ECU rilevato.</p>';return;}var h='';d.ecus.forEach(function(e){h+='<div class="ecu"><div class="eh"><span class="eid">ECU '+e.id+'</span><span class="ecnt">'+e.totalRaw+' PID totali ('+e.pids.length+' documentati)</span></div>';if(e.pids.length===0){h+='<p class="empty">Nessun PID documentato per questo ECU.</p>';}else{h+='<div class="pl">';e.pids.forEach(function(p){h+='<span>'+p+'</span>';});h+='</div>';}h+='</div>';});r.innerHTML=h;scLoaded=true;}).catch(()=>{document.getElementById('scR').innerHTML='<p class="empty">Errore di rete.</p>';});}
var brT;function brSet(v){clearTimeout(brT);brT=setTimeout(function(){fetch('/brightness?set='+v);},200);}
function u(){fetch('/data').then(r=>r.json()).then(d=>{
function c(e,r,y){e.className='v'+(r?' al':y?' w':'');}
document.getElementById('dbgTgl').checked=d.debug;
document.getElementById('dbgWarn').style.display=d.debug?'':'none';
document.querySelectorAll('.debug').forEach(function(e){e.style.display=d.debug?'':'none';});
var s=d.boost>=0?'+':'';
var el=document.getElementById('d_boost');el.textContent=s+d.boost.toFixed(2);el.className='v'+(d.boost>1.5?' al':'');
el=document.getElementById('d_cool');el.textContent=d.coolant;el.className='v'+(d.coolant>110?' al':d.coolant>100?' w':'');
document.getElementById('d_torq').textContent=d.torque;
el=document.getElementById('d_egr');el.textContent=d.egr+'%('+d.egrErr+')';el.className='v'+(Math.abs(d.egrErr)>10?' al':'');
document.getElementById('d_ldr').textContent=d.lightLevel;
document.getElementById('d_ctr').textContent=d.contrast;
document.getElementById('d_brm').textContent=d.brightnessMode;
var sl=document.getElementById('brSld');if(document.activeElement!==sl)sl.value=d.contrast;
if(d.debug){
document.getElementById('d_load').textContent=d.load.toFixed(1)+' %';
document.getElementById('d_rpm').textContent=d.rpm;
document.getElementById('d_maf').textContent=d.maf.toFixed(1)+' g/s';
el=document.getElementById('d_rail');el.textContent=d.railBar+' bar';c(el,d.railBar<200||d.railBar>1850,false);
el=document.getElementById('d_lam');el.textContent=d.lambda.toFixed(3);c(el,d.lambda<0.8,false);
document.getElementById('d_afr').textContent=d.afr.toFixed(1);
el=document.getElementById('d_cool2');el.textContent=d.coolant+' \u00B0C';c(el,d.coolant>110,d.coolant>100);
document.getElementById('d_iat').textContent=d.iat+' \u00B0C';
document.getElementById('d_spd').textContent=d.speed+' km/h';
document.getElementById('d_pD').textContent=d.pedalD+' %';
document.getElementById('d_pE').textContent=d.pedalE+' %';
document.getElementById('d_thr').textContent=d.throttle+' %';
document.getElementById('d_gear').textContent=d.gearRatio>0?d.gearRatio.toFixed(2):'N/D';
document.getElementById('d_acc').textContent=d.accel.toFixed(2)+' m/s\u00B2';
document.getElementById('d_fl100').textContent=d.fuelL100>0?d.fuelL100.toFixed(1)+' L/100':'N/D';
el=document.getElementById('d_bsfc');el.textContent=d.bsfc>0?d.bsfc+' g/kWh':'N/D';c(el,d.bsfc>400,false);
document.getElementById('d_pow').textContent=d.powerKw.toFixed(1)+' kW / '+d.powerCv.toFixed(1)+' CV';
document.getElementById('d_boost2').textContent=s+d.boost.toFixed(2)+' bar';
document.getElementById('d_pr').textContent=d.pressureRatio.toFixed(2);
el=document.getElementById('d_ic');el.textContent=d.intercoolerEff>=0?d.intercoolerEff+' %':'N/D';c(el,d.intercoolerEff>=0&&d.intercoolerEff<40,false);
document.getElementById('d_br').textContent=d.boostRate.toFixed(2)+' bar/s';
el=document.getElementById('d_volt');el.textContent=d.volts.toFixed(1)+' V';c(el,d.volts<12.5||d.volts>15,d.volts<13||d.volts>14.5);
el=document.getElementById('d_drift');el.textContent=d.driftPedal+' %';c(el,d.driftPedal>5,d.driftPedal>=3);
document.getElementById('d_dfco').textContent=d.dfco?'ATTIVO':'OFF';
el=document.getElementById('d_ve');el.textContent=d.volEff>=0?d.volEff+' %':'N/D';c(el,d.volEff>=0&&d.volEff<60,false);
document.getElementById('d_dt').textContent=d.deltaThrottlePedal+' %';
document.getElementById('d_amb').textContent=d.ambient+' \u00B0C';
document.getElementById('d_bar').textContent=d.baro+' kPa';
document.getElementById('d_alt').textContent=d.altitude+' m';
document.getElementById('d_ad').textContent=d.airDensity.toFixed(3)+' kg/m\u00B3';
var rt=d.runtime;var h=Math.floor(rt/3600);var m=Math.floor((rt%3600)/60);var sc=rt%60;
document.getElementById('d_rt').textContent=h+':'+String(m).padStart(2,'0')+':'+String(sc).padStart(2,'0');
document.getElementById('d_km').textContent=d.kmMil+' km';
document.getElementById('d_st').textContent=d.starts;
}
var db=document.getElementById('dtcBox');
if(d.dtcCount>0){db.className='db';var h='';for(var i=0;i<d.dtc.length;i++){h+='<div class="di"><span class="dc">'+d.dtc[i].code+'</span> <span class="dd">'+(d.dtc[i].desc||'')+'</span></div>';}document.getElementById('dtcL').innerHTML=h;}else{db.className='db hid';}
}).catch(()=>{});}
function clrDtc(){if(!confirm('Cancellare i codici DTC?\nSi perde lo storico errori e si resettano i monitor di readiness.'))return;var b=document.getElementById('btnClr');b.disabled=true;b.textContent='...';fetch('/clear-dtc').then(r=>r.json()).then(d=>{alert(d.ok?'DTC cancellati. Re-check in corso.':'Cancellazione fallita.');}).catch(()=>alert('Errore di rete')).finally(()=>{b.disabled=false;b.textContent='CANCELLA';});}
var logSeq=-1,logTimer=null,logStick=true,logErr=0;
function logStart(){if(logTimer)return;logFetch();logTimer=setInterval(logFetch,250);}
function logStop(){if(logTimer){clearInterval(logTimer);logTimer=null;}}
function logClr(){document.getElementById('smPre').textContent='';logStick=true;}
function logScrollEnd(){var p=document.getElementById('smPre');p.scrollTop=p.scrollHeight;logStick=true;}
function logFetch(){var url=logSeq<0?'/serial-data':'/serial-data?since='+logSeq;fetch(url).then(function(r){var nx=r.headers.get('X-Seq'),dr=r.headers.get('X-Dropped');return r.text().then(function(t){return{t:t,seq:nx,dr:dr};});}).then(function(o){logErr=0;var p=document.getElementById('smPre'),st=document.getElementById('smStat');var atEnd=(p.scrollHeight-p.scrollTop-p.clientHeight)<30;if(o.seq!=null)logSeq=parseInt(o.seq);if(o.dr==='1')p.textContent+='\n[--- byte persi: buffer pieno ---]\n';if(o.t)p.textContent+=o.t;if(p.textContent.length>200000)p.textContent=p.textContent.slice(-200000);if(atEnd||logStick){p.scrollTop=p.scrollHeight;logStick=true;}else{logStick=false;}st.className='smStat'+(o.dr==='1'?' drp':'');st.textContent='cursor '+logSeq+(o.dr==='1'?' (overflow rilevato)':'');}).catch(function(){logErr++;var st=document.getElementById('smStat');st.className='smStat drp';st.textContent='errore di rete ('+logErr+')';});}
setInterval(u,500);u();
</script>
</body>
</html>
)rawliteral";

// ============================================================
// HTML della pagina /scan (PROGMEM) — risultati dello scan multi-ECU
// ============================================================
static const char SCAN_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="it">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Scan PID OBD2</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{background:#111;color:#e0e0e0;font-family:'Segoe UI',-apple-system,Arial,sans-serif;padding:12px;max-width:560px;margin:0 auto}
h1{text-align:center;color:#00d4ff;font-size:1.2em;margin-bottom:4px}
.sub{text-align:center;color:#888;font-size:.78em;margin-bottom:14px}
.sub a{color:#00d4ff;text-decoration:none}
.ecu{background:#1a1a2e;border-radius:8px;padding:10px 12px;margin-bottom:12px;border:1px solid #222}
.eh{display:flex;justify-content:space-between;align-items:baseline;border-bottom:1px solid #222;padding-bottom:6px;margin-bottom:8px}
.eid{color:#00d4ff;font-size:.95em;font-weight:600;letter-spacing:1px}
.ecnt{color:#5588bb;font-size:.78em}
.pl{display:flex;flex-wrap:wrap;gap:6px}
.pl span{background:#0f1a30;border:1px solid #1a3060;border-radius:4px;padding:3px 8px;font-size:.78em;color:#cfe6f5}
.empty{color:#888;font-style:italic;font-size:.85em}
.note{color:#666;font-size:.72em;text-align:center;margin-top:14px}
</style>
</head>
<body>
<h1>Scan PID OBD2</h1>
<p class="sub">PID standard SAE J1979 supportati dagli ECU del veicolo &middot; <a href="/dashboard">Dashboard</a></p>
<div id="root">Caricamento...</div>
<p class="note">I PID non documentati nel database SAE sono mostrati come "PID 0xXX".</p>
<script>
fetch('/scan-data').then(r=>r.json()).then(d=>{
  var root=document.getElementById('root');
  if(!d.ecus||d.ecus.length===0){root.innerHTML='<p class="empty">Nessun ECU rilevato.</p>';return;}
  var html='';
  d.ecus.forEach(function(e){
    html+='<div class="ecu"><div class="eh"><span class="eid">ECU '+e.id+'</span><span class="ecnt">'+e.totalRaw+' PID totali ('+e.pids.length+' documentati)</span></div>';
    if(e.pids.length===0){html+='<p class="empty">Nessun PID documentato per questo ECU.</p>';}
    else{html+='<div class="pl">';e.pids.forEach(function(p){html+='<span>'+p+'</span>';});html+='</div>';}
    html+='</div>';
  });
  root.innerHTML=html;
}).catch(function(){document.getElementById('root').innerHTML='<p class="empty">Errore di rete.</p>';});
</script>
</body>
</html>
)rawliteral";

// ============================================================
// Handler /scan-data — JSON con i risultati dello scan multi-ECU
// Mostra solo descrizioni testuali dei PID (senza indirizzi hex).
// ============================================================

#ifdef OBD_CONN_CAN
struct ECUScanResult;
extern ECUScanResult ecuResults[];
extern uint8_t ecuCount;

/**
 * Scrive il JSON dei risultati dello scan multi-ECU su HTTP.
 * Per ogni ECU: id (hex 3 char), totale PID grezzi, elenco descrizioni
 * SAE dei soli PID documentati (i non documentati sono omessi).
 *
 * Layout struct ECUScanResult: { uint16_t ecuId; uint8_t mode01[32];
 *   uint16_t pidCount; }. Per evitare include circolari ridichiariamo
 *   solo l'estensione tramite extern e accediamo via offset memcpy:
 *   in pratica usiamo direttamente i campi tramite forward decl.
 *
 * @since 07/05/26 Mattia Alesi
 */
static void handleScanData(ESP8266WebServer& server) {
  // Accesso ai campi della struct definita in CANbus_conn.ino.
  // Per evitare conflitti di include includiamo qui la stessa
  // struttura come dichiarazione esterna (devono coincidere).
  struct LocalECUScan {
    uint16_t ecuId;
    uint8_t  mode01[32];
    uint16_t pidCount;
  };
  LocalECUScan* results = (LocalECUScan*)ecuResults;

  String json = F("{\"ecus\":[");
  for (uint8_t e = 0; e < ecuCount; e++) {
    if (e > 0) json += ',';
    char hdr[48];
    snprintf(hdr, sizeof(hdr), "{\"id\":\"%03X\",\"totalRaw\":%u,\"pids\":[",
             results[e].ecuId & 0xFFF, (unsigned)results[e].pidCount);
    json += hdr;

    bool firstPid = true;
    for (int p = 1; p <= 0xFF; p++) {
      uint8_t byteIdx = (p - 1) / 8;
      uint8_t bitIdx  = 7 - ((p - 1) % 8);
      if ((results[e].mode01[byteIdx] & (1 << bitIdx)) == 0) continue;

      // Tutti i PID supportati vengono esposti. Se il database SAE
      // non contiene una sigla, fallback a "PID 0xXX".
      char buf[40];
      const char* name = getPIDShortName((uint8_t)p);
      if (name) {
        strncpy_P(buf, name, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = 0;
      } else {
        snprintf(buf, sizeof(buf), "PID 0x%02X", p);
      }

      if (!firstPid) json += ',';
      json += '"';
      json += buf;
      json += '"';
      firstPid = false;
    }
    json += F("]}");
  }
  json += F("]}");
  server.send(200, "application/json", json);
}
#endif

// ============================================================
// Variabili statiche per derivate (accelerazione, variaz. boost)
// ============================================================
static float _prevBoostBar = 0;
static int _prevSpeedKmh = 0;
static unsigned long _prevDataMs = 0;

// ============================================================
// Helper: legge un PID e restituisce true se ok
// Adattato per WiFi o CAN in base alla define
// ============================================================

#ifdef OBD_CONN_WIFI
// WiFi: usa queryOBDPID(pid, dataBytes, maxBytes, &actualBytes)
static bool _readPid(uint8_t pid, uint8_t* buf, uint8_t maxBytes, uint8_t* actual) {
  return queryOBDPID(pid, buf, maxBytes, actual);
}
#endif

#ifdef OBD_CONN_CAN
// CAN: usa queryPID(pid, data[8], &len) — dati in buf[3..7]
static uint8_t _canBuf[8];
static uint8_t _canLen;
static bool _readPidCan(uint8_t pid) {
  return queryPID(pid, _canBuf, &_canLen);
}
#endif

// ============================================================
// Handler /data — genera JSON con tutti i 47 parametri
// ============================================================

static void handleData(ESP8266WebServer& server) {
  extern bool debugMode;

  // Variabili globali dal round-robin (sempre disponibili, zero query OBD)
  extern float boostBar;
  extern int coolantC, torqueNm, egrPct, egrErrPct;
  extern bool mapAvailable, coolantAvailable, loadAvailable, egrAvailable;
  extern bool milOn;
  extern uint8_t dtcCount;
  extern uint16_t dtcCodes[];

  if (!debugMode) {
    // --- JSON ridotto: solo 4 valori principali + DTC ---
    char json[384];
    int p = 0;
    int bc = (int)(boostBar * 100.0f);
    const char* brm = br.mode == BR_AUTO ? "auto" : br.mode == BR_MANUAL ? "manual" : "fading";
    p += snprintf(json + p, sizeof(json) - p,
      "{\"boost\":%.2f,\"coolant\":%d,\"torque\":%d,"
      "\"egr\":%d,\"egrErr\":%d,\"mil\":%s,\"dtcCount\":%d,"
      "\"lightLevel\":%d,\"contrast\":%u,\"brightnessMode\":\"%s\","
      "\"debug\":false,\"dtc\":[",
      boostBar, coolantC, torqueNm,
      egrPct, egrErrPct, milOn ? "true" : "false", (int)dtcCount,
      (int)br.ldrEma, br.currentContrast, brm);
    for (int i = 0; i < dtcCount && i < 6; i++) {
      char code[6];
      decodeDTC(dtcCodes[i], code);
      const char* desc = getDTCDescription(dtcCodes[i]);
      char descBuf[22] = "";
      if (desc) { strncpy_P(descBuf, desc, 21); descBuf[21] = 0; }
      p += snprintf(json + p, sizeof(json) - p, "%s{\"code\":\"%s\",\"desc\":\"%s\"}",
        i > 0 ? "," : "", code, descBuf);
    }
    p += snprintf(json + p, sizeof(json) - p, "]}");
    server.send(200, "application/json", json);
    return;
  }

  // --- JSON completo: legge TUTTI i PID internamente ---
  uint8_t d[4];
  uint8_t n;

  float loadPct = 0, mapKpa = 0, baroKpa = 0, mafGs = 0, lambdaVal = 1.0f;
  float iatC = 0, ambC = 0, coolC = 0, voltsV = 0, o2v = 0;
  int rpm = 0, speedKmh = 0, pedalD = 0, pedalE = 0, throttle = 0;
  int egrCmd = 0, egrErr = 0, railBar = 0;
  unsigned long runtimeS = 0;
  int kmMil = 0, starts = 0;
  bool mil = false;
  int dtcN = 0;

  bool hLoad=0,hMap=0,hBaro=0,hMaf=0,hLam=0,hIat=0,hAmb=0,hCool=0;
  bool hRpm=0,hSpd=0,hPD=0,hPE=0,hThr=0,hEgr=0,hEgrE=0,hRail=0,hV=0,hRt=0;

  // ---- Lettura PID ----
#ifdef OBD_CONN_WIFI
  if (_readPid(0x04,d,1,&n)&&n>=1) { loadPct=((float)d[0]*100.0f)/255.0f; hLoad=1; }
  if (_readPid(0x05,d,1,&n)&&n>=1) { coolC=(float)d[0]-40.0f; hCool=1; }
  if (_readPid(0x0B,d,1,&n)&&n>=1) { mapKpa=(float)d[0]; hMap=1; }
  if (_readPid(0x0C,d,2,&n)&&n>=2) { rpm=((d[0]<<8)|d[1])/4; hRpm=1; }
  if (_readPid(0x0D,d,1,&n)&&n>=1) { speedKmh=d[0]; hSpd=1; }
  if (_readPid(0x0F,d,1,&n)&&n>=1) { iatC=(float)d[0]-40.0f; hIat=1; }
  if (_readPid(0x10,d,2,&n)&&n>=2) { mafGs=((float)((d[0]<<8)|d[1]))/100.0f; hMaf=1; }
  if (_readPid(0x23,d,2,&n)&&n>=2) { railBar=((d[0]<<8)|d[1])*10/100; hRail=1; }
  if (_readPid(0x24,d,4,&n)&&n>=2) { lambdaVal=((float)((d[0]<<8)|d[1]))/32768.0f; hLam=1; if(n>=4) o2v=((float)((d[2]<<8)|d[3]))/8192.0f; }
  if (_readPid(0x2C,d,1,&n)&&n>=1) { egrCmd=((int)d[0]*100)/255; hEgr=1; }
  if (_readPid(0x2D,d,1,&n)&&n>=1) { egrErr=((int)d[0]-128)*100/128; hEgrE=1; }
  if (_readPid(0x33,d,1,&n)&&n>=1) { baroKpa=(float)d[0]; hBaro=1; }
  if (_readPid(0x42,d,2,&n)&&n>=2) { voltsV=((float)((d[0]<<8)|d[1]))/1000.0f; hV=1; }
  if (_readPid(0x46,d,1,&n)&&n>=1) { ambC=(float)d[0]-40.0f; hAmb=1; }
  if (_readPid(0x49,d,1,&n)&&n>=1) { pedalD=((int)d[0]*100)/255; hPD=1; }
  if (_readPid(0x4A,d,1,&n)&&n>=1) { pedalE=((int)d[0]*100)/255; hPE=1; }
  if (_readPid(0x4C,d,1,&n)&&n>=1) { throttle=((int)d[0]*100)/255; hThr=1; }
  if (_readPid(0x01,d,4,&n)&&n>=1) { mil=(d[0]&0x80)!=0; dtcN=d[0]&0x7F; }
  if (_readPid(0x1F,d,2,&n)&&n>=2) { runtimeS=(d[0]<<8)|d[1]; hRt=1; }
  if (_readPid(0x21,d,2,&n)&&n>=2) { kmMil=(d[0]<<8)|d[1]; }
  if (_readPid(0x30,d,1,&n)&&n>=1) { starts=d[0]; }
#endif

#ifdef OBD_CONN_CAN
  if (_readPidCan(0x04)) { loadPct=((float)_canBuf[3]*100.0f)/255.0f; hLoad=1; }
  if (_readPidCan(0x05)) { coolC=(float)_canBuf[3]-40.0f; hCool=1; }
  if (_readPidCan(0x0B)) { mapKpa=(float)_canBuf[3]; hMap=1; }
  if (_readPidCan(0x0C)) { rpm=((_canBuf[3]<<8)|_canBuf[4])/4; hRpm=1; }
  if (_readPidCan(0x0D)) { speedKmh=_canBuf[3]; hSpd=1; }
  if (_readPidCan(0x0F)) { iatC=(float)_canBuf[3]-40.0f; hIat=1; }
  if (_readPidCan(0x10)) { mafGs=((float)((_canBuf[3]<<8)|_canBuf[4]))/100.0f; hMaf=1; }
  if (_readPidCan(0x23)) { railBar=((_canBuf[3]<<8)|_canBuf[4])*10/100; hRail=1; }
  if (_readPidCan(0x24)) { lambdaVal=((float)((_canBuf[3]<<8)|_canBuf[4]))/32768.0f; hLam=1; o2v=((float)((_canBuf[5]<<8)|_canBuf[6]))/8192.0f; }
  if (_readPidCan(0x2C)) { egrCmd=((int)_canBuf[3]*100)/255; hEgr=1; }
  if (_readPidCan(0x2D)) { egrErr=((int)_canBuf[3]-128)*100/128; hEgrE=1; }
  if (_readPidCan(0x33)) { baroKpa=(float)_canBuf[3]; hBaro=1; }
  if (_readPidCan(0x42)) { voltsV=((float)((_canBuf[3]<<8)|_canBuf[4]))/1000.0f; hV=1; }
  if (_readPidCan(0x46)) { ambC=(float)_canBuf[3]-40.0f; hAmb=1; }
  if (_readPidCan(0x49)) { pedalD=((int)_canBuf[3]*100)/255; hPD=1; }
  if (_readPidCan(0x4A)) { pedalE=((int)_canBuf[3]*100)/255; hPE=1; }
  if (_readPidCan(0x4C)) { throttle=((int)_canBuf[3]*100)/255; hThr=1; }
  if (_readPidCan(0x01)) { mil=(_canBuf[3]&0x80)!=0; dtcN=_canBuf[3]&0x7F; }
  if (_readPidCan(0x1F)) { runtimeS=(_canBuf[3]<<8)|_canBuf[4]; hRt=1; }
  if (_readPidCan(0x21)) { kmMil=(_canBuf[3]<<8)|_canBuf[4]; }
  if (_readPidCan(0x30)) { starts=_canBuf[3]; }
#endif

  // ---- Calcoli derivati ----
  float boost = (hMap && hBaro) ? (mapKpa - baroKpa) / 100.0f : 0;
  int torque = hLoad ? (int)Audi27TDI140kW::Torque::estimateEngineTorqueNm(
    loadPct, (float)rpm, mafGs, mapKpa, iatC,
    hRail ? (float)railBar * 100.0f : NAN,
    hV ? voltsV : NAN,
    hBaro ? baroKpa : NAN, false) : 0;
  float powerKw = (hLoad && hRpm && rpm > 0) ? (float)torque * (float)rpm / 9549.0f : 0;
  float powerCv = powerKw * 1.36f;
  float afr = hLam ? lambdaVal * 14.5f : 0;
  float lam = (hLam && lambdaVal > 0.5f) ? lambdaVal : 1.0f;
  float fuelGs = hMaf ? mafGs / (14.5f * lam) : 0;
  float fuelLh = hMaf ? (fuelGs * 3600.0f) / 835.0f : 0;
  float fuelL100 = (hMaf && hSpd && speedKmh > 3) ? (fuelLh / (float)speedKmh) * 100.0f : 0;
  int altitude = (hBaro && baroKpa > 0) ? (int)(44330.0f * (1.0f - pow(baroKpa / 101.325f, 0.1903f))) : 0;
  float airDens = (hMap && hIat) ? (mapKpa * 1000.0f) / (287.058f * (iatC + 273.15f)) : 0;
  float pr = (hMap && hBaro && baroKpa > 0) ? mapKpa / baroKpa : 0;
  int volEff = (hMaf && hRpm && hMap && hIat && rpm > 0 && airDens > 0) ? (int)((mafGs * 120.0f) / (2.698f * (float)rpm * airDens / 1000.0f)) : -1;
  int driftPedal = (hPD && hPE) ? abs(pedalD - pedalE) : 0;
  int deltaTP = (hThr && hPD) ? throttle - pedalD : 0;
  bool dfco = hLoad && (loadPct < 1.0f);
  float gearRatio = (hRpm && hSpd && speedKmh > 5 && rpm > 0) ? (float)rpm / ((float)speedKmh * 7.9f) : 0;
  int bsfc = (powerKw > 1.0f && hMaf) ? (int)((fuelGs * 3600.0f) / powerKw) : 0;

  // Intercooler
  int icEff = -1;
  if (hMap && hBaro && hIat && hAmb && mapKpa > baroKpa) {
    float tTeor = (ambC + 273.15f) * pow(mapKpa / baroKpa, 0.286f) - 273.15f;
    float den = tTeor - ambC;
    if (den > 1.0f) icEff = (int)(((tTeor - iatC) / den) * 100.0f);
  }

  // Accelerazione e variazione boost
  unsigned long nowMs = millis();
  float accel = 0, boostRate = 0;
  if (_prevDataMs > 0) {
    float dt = (float)(nowMs - _prevDataMs) / 1000.0f;
    if (dt > 0.05f) {
      accel = ((float)(speedKmh - _prevSpeedKmh) / 3.6f) / dt;
      boostRate = (boost - _prevBoostBar) / dt;
    }
  }
  _prevSpeedKmh = speedKmh;
  _prevBoostBar = boost;
  _prevDataMs = nowMs;

  // ---- Genera JSON ----
  // Uso snprintf a blocchi per limitare l'uso di RAM
  char json[768];
  int p = 0;
  p += snprintf(json + p, sizeof(json) - p,
    "{\"load\":%.1f,\"coolant\":%d,\"map\":%d,\"rpm\":%d,\"speed\":%d,"
    "\"iat\":%d,\"maf\":%.1f,\"railBar\":%d,\"lambda\":%.3f,\"o2v\":%.3f,"
    "\"egr\":%d,\"egrErr\":%d,\"baro\":%d,\"volts\":%.1f,"
    "\"ambient\":%d,\"pedalD\":%d,\"pedalE\":%d,\"throttle\":%d,",
    loadPct, (int)coolC, (int)mapKpa, rpm, speedKmh,
    (int)iatC, mafGs, railBar, lambdaVal, o2v,
    egrCmd, egrErr, (int)baroKpa, voltsV,
    (int)ambC, pedalD, pedalE, throttle);

  p += snprintf(json + p, sizeof(json) - p,
    "\"mil\":%s,\"dtcCount\":%d,\"runtime\":%lu,"
    "\"kmMil\":%d,\"starts\":%d,"
    "\"boost\":%.2f,\"torque\":%d,\"powerKw\":%.1f,\"powerCv\":%.1f,"
    "\"afr\":%.1f,\"fuelL100\":%.1f,\"altitude\":%d,",
    mil ? "true" : "false", dtcN, runtimeS,
    kmMil, starts,
    boost, torque, powerKw, powerCv,
    afr, fuelL100, altitude);

  p += snprintf(json + p, sizeof(json) - p,
    "\"airDensity\":%.3f,\"intercoolerEff\":%d,\"pressureRatio\":%.2f,"
    "\"volEff\":%d,\"driftPedal\":%d,"
    "\"deltaThrottlePedal\":%d,\"dfco\":%s,\"gearRatio\":%.2f,"
    "\"accel\":%.2f,\"boostRate\":%.2f,"
    "\"bsfc\":%d,",
    airDens, icEff, pr,
    volEff, driftPedal,
    deltaTP, dfco ? "true" : "false", gearRatio,
    accel, boostRate,
    bsfc);

  // Brightness state + DTC array
  const char* brmDbg = br.mode == BR_AUTO ? "auto" : br.mode == BR_MANUAL ? "manual" : "fading";
  p += snprintf(json + p, sizeof(json) - p,
    "\"lightLevel\":%d,\"contrast\":%u,\"brightnessMode\":\"%s\",\"debug\":true,\"dtc\":[",
    (int)br.ldrEma, br.currentContrast, brmDbg);
  for (int i = 0; i < dtcCount && i < 6; i++) {
    char code[6];
    decodeDTC(dtcCodes[i], code);
    const char* desc = getDTCDescription(dtcCodes[i]);
    char descBuf[22] = "";
    if (desc) { strncpy_P(descBuf, desc, 21); descBuf[21] = 0; }
    p += snprintf(json + p, sizeof(json) - p, "%s{\"code\":\"%s\",\"desc\":\"%s\"}",
      i > 0 ? "," : "", code, descBuf);
  }
  p += snprintf(json + p, sizeof(json) - p, "]}");

  server.send(200, "application/json", json);
}

// ============================================================
// Setup: registra gli endpoint sul web server
// ============================================================

/**
 * Registra gli endpoint /dashboard, /data, /debug, /brightness, /clear-dtc sul web server HTTP.
 * Chiamare dopo httpUpdater.setup() e prima di httpServer.begin().
 *
 * @param server riferimento al web server ESP8266
 * @see WIFI_conn.ino, CANbus_conn.ino — setup()
 * @since 01/04/26 Mattia Alesi
 */
void setupWebDashboard(ESP8266WebServer& server) {
  server.on("/dashboard", HTTP_GET, [&server]() {
    server.send_P(200, "text/html", DASHBOARD_HTML);
  });
  server.on("/data", HTTP_GET, [&server]() {
    handleData(server);
  });
  // Serial monitor live: ritorna i byte da ?since=N (cursor del client)
  // fino al piu' recente disponibile nel buffer circolare di TeeSerial.
  // Usa text/plain con header X-Seq (nuovo cursor) e X-Dropped (1 se il
  // client ha perso byte per overflow del buffer).
  server.on("/serial-data", HTTP_GET, [&server]() {
    uint32_t since = 0;
    bool firstFetch = !server.hasArg("since");
    if (!firstFetch) {
      since = (uint32_t)strtoul(server.arg("since").c_str(), nullptr, 10);
    } else {
      // Primo fetch: parto dal piu' vecchio byte ancora in buffer per
      // mostrare lo storico recente, non l'intera sequenza dall'avvio.
      uint32_t head = TeeSerial::logHeadSeq();
      size_t bs = TeeSerial::logBufferSize();
      since = head > bs ? head - bs : 0;
    }
    static uint8_t chunk[1024];
    uint32_t nextSeq;
    bool dropped = false;
    size_t n = TeeSerial::logReadFrom(since, chunk, sizeof(chunk),
                                      &nextSeq, &dropped);

    server.sendHeader(F("X-Seq"), String((unsigned long)nextSeq));
    server.sendHeader(F("X-Dropped"), dropped ? "1" : "0");
    server.sendHeader(F("Cache-Control"), F("no-store"));
    // String((const char*)chunk, n) non esiste in Arduino String: copio
    // byte-a-byte. n e' al massimo 1024, costo trascurabile.
    String body;
    body.reserve(n);
    for (size_t i = 0; i < n; i++) body += (char)chunk[i];
    server.send(200, "text/plain", body);
  });
  // Pagina e dati scan multi-ECU (solo build CAN: gli ECU multipli sono
  // raggiungibili solo via bus CAN diretto, non tramite ELM/WiFi)
  #ifdef OBD_CONN_CAN
    server.on("/scan", HTTP_GET, [&server]() {
      server.send_P(200, "text/html", SCAN_HTML);
    });
    server.on("/scan-data", HTTP_GET, [&server]() {
      handleScanData(server);
    });
  #endif
  // Toggle diagnostica seriale: /debug?on, /debug?off, /debug (stato)
  extern bool debugMode;
  server.on("/debug", HTTP_GET, [&server]() {
    extern bool debugMode;
    if (server.hasArg("on")) { debugMode = true; }
    else if (server.hasArg("off")) { debugMode = false; }
    char json[20];
    snprintf(json, sizeof(json), "{\"debug\":%s}", debugMode ? "true" : "false");
    server.send(200, "application/json", json);
  });
  // Brightness: stato + slider override (?set=N) + reset auto (?auto)
  server.on("/brightness", HTTP_GET, [&server]() {
    if (server.hasArg("set")) {
      int v = server.arg("set").toInt();
      if (v < 0) v = 0;
      if (v > 255) v = 255;
      br.manualContrast  = (uint8_t)v;
      br.currentContrast = (uint8_t)v;
      br.ldrAtOverride   = (int)br.ldrEma;
      br.mode            = BR_MANUAL;
      u8g2.setContrast((uint8_t)v);
    } else if (server.hasArg("auto")) {
      br.targetContrast = adcToContrast((int)br.ldrEma);
      br.mode = BR_FADING;
    }
    const char* m = br.mode == BR_AUTO ? "auto" : br.mode == BR_MANUAL ? "manual" : "fading";
    char j[140];
    snprintf(j, sizeof(j),
      "{\"ldr\":%d,\"contrast\":%u,\"mode\":\"%s\",\"ldrAtOverride\":%d}",
      (int)br.ldrEma, br.currentContrast, m, br.ldrAtOverride);
    server.send(200, "application/json", j);
  });
  // Cancellazione DTC (OBD2 Mode 04). Implementata in CANbus_conn.ino o WIFI_conn.ino.
  server.on("/clear-dtc", HTTP_GET, [&server]() {
    bool ok = false;
    #ifdef OBD_CONN_CAN
      extern bool clearDTCsViaCAN();
      ok = clearDTCsViaCAN();
    #endif
    #ifdef OBD_CONN_WIFI
      extern bool clearDTCsViaELM();
      ok = clearDTCsViaELM();
    #endif
    if (ok) {
      extern unsigned long lastDtcCheck;
      lastDtcCheck = 0;  // forza re-check immediato al prossimo monitor cycle
    }
    char json[24];
    snprintf(json, sizeof(json), "{\"ok\":%s}", ok ? "true" : "false");
    server.send(200, "application/json", json);
  });
}

#endif

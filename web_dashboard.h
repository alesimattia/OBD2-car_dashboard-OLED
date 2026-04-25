/**
 * Dashboard web live per OBD2 Monitor.
 *
 * Fornisce tre endpoint HTTP:
 *   /dashboard — pagina HTML dashboard con auto-refresh 500ms
 *   /data      — JSON con tutti i 47 parametri (diretti + calcolati)
 *   /debug     — toggle diagnostica seriale (?on/?off)
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
#btnClr{float:right;background:#1a0808;color:#ff8888;border:1px solid #cc3333;border-radius:4px;padding:2px 10px;font-size:.7em;font-weight:600;cursor:pointer;letter-spacing:.5px}
#btnClr:hover{background:#cc3333;color:#fff}
#btnClr:active{background:#881111}
</style>
</head>
<body>
<h1>OBD2 Dashboard</h1>
<p class="sub">Audi A5 B8 2.7 TDI CGKA</p>
<div class="tg"><label>Diagnostica Serial</label><label class="sw"><input type="checkbox" id="dbgTgl" onchange="fetch('/debug?'+(this.checked?'on':'off'))"><span class="sl"></span></label></div>
<p class="sub" id="dbgWarn" style="display:none;color:#ca4">Rallenta il refresh dei dati sul display</p>
<div class="s"><div class="st">Dati principali</div><div class="mg">
<div class="mc"><div class="l">BOOST</div><div class="v" id="d_boost">--</div><div class="u">bar</div></div>
<div class="mc"><div class="l">TEMP</div><div class="v" id="d_cool">--</div><div class="u">&deg;C</div></div>
<div class="mc"><div class="l">COPPIA</div><div class="v" id="d_torq">--</div><div class="u">Nm</div></div>
<div class="mc"><div class="l">EGR</div><div class="v" id="d_egr">--</div><div class="u"></div></div>
</div></div>
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
<div class="db hid" id="dtcBox"><div class="st">Errori DTC <span class="mil" id="milB">MIL</span><button id="btnClr" onclick="clrDtc()">CANCELLA</button></div><div id="dtcL"></div></div>
<div class="sb">Auto-refresh 500ms</div>
<script>
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
setInterval(u,500);u();
</script>
</body>
</html>
)rawliteral";

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
    p += snprintf(json + p, sizeof(json) - p,
      "{\"boost\":%.2f,\"coolant\":%d,\"torque\":%d,"
      "\"egr\":%d,\"egrErr\":%d,\"mil\":%s,\"dtcCount\":%d,\"debug\":false,\"dtc\":[",
      boostBar, coolantC, torqueNm,
      egrPct, egrErrPct, milOn ? "true" : "false", (int)dtcCount);
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

  // DTC array
  p += snprintf(json + p, sizeof(json) - p, "\"debug\":true,\"dtc\":[");
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
 * Registra gli endpoint /dashboard, /data e /debug sul web server HTTP.
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

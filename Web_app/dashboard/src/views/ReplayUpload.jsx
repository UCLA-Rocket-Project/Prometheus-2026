import React, { useState, useRef } from 'react';

// ---------------------------------------------------------------------------
// CSV parsing — produces raw receiver-format objects
// ---------------------------------------------------------------------------
function parseCSV(text) {
  const lines = text.trim().split(/\r?\n/);
  if (lines.length < 2) return [];
  const headers = lines[0].split(',').map((h) => h.trim());
  const packets = [];
  for (let i = 1; i < lines.length; i++) {
    const line = lines[i].trim();
    if (!line) continue;
    const values = line.split(',');
    const obj = {};
    headers.forEach((h, j) => {
      const v = (values[j] ?? '').trim();
      const n = parseFloat(v);
      obj[h] = isNaN(n) ? v : n;
    });
    packets.push(obj);
  }
  return packets;
}

// ---------------------------------------------------------------------------
// Timestamp normalisation — handles missing or aliased t columns
// ---------------------------------------------------------------------------
const TIME_ALIASES = ['time', 'timestamp', 'ts', 'elapsed', 'T'];
const DEFAULT_DT = 0.1; // fallback: assume 10 Hz

function ensureTimestamps(packets) {
  if (!packets.length) return;
  if (typeof packets[0].t === 'number' && !isNaN(packets[0].t)) return;
  for (const alias of TIME_ALIASES) {
    if (typeof packets[0][alias] === 'number' && !isNaN(packets[0][alias])) {
      packets.forEach((p) => { p.t = p[alias]; });
      return;
    }
  }
  packets.forEach((p, i) => { p.t = i * DEFAULT_DT; });
}

// ---------------------------------------------------------------------------
// Auto-detect resting accel magnitude so we can normalise to G
// regardless of whether ax/ay/az are in G or m/s²
// ---------------------------------------------------------------------------
function estimateRestingNorm(packets) {
  const n = Math.min(packets.length, 40);
  const norms = [];
  for (let i = 0; i < n; i++) {
    const { ax = 0, ay = 0, az = 0 } = packets[i];
    norms.push(Math.sqrt(ax * ax + ay * ay + az * az));
  }
  norms.sort((a, b) => a - b);
  const med = norms[Math.floor(norms.length / 2)];
  return med > 0.05 ? med : 1;
}

// ---------------------------------------------------------------------------
// Phase state machine — only advances forward
// alt in ft AGL, vel in ft/s, accel in G
// ---------------------------------------------------------------------------
function advancePhase(phase, accel, vel, alt) {
  switch (phase) {
    case 'pad':
      if (accel > 2.0) return 'powered';
      break;
    case 'powered':
      // burnout: accel drops back to ~1G while still climbing
      if (accel < 1.8 && vel > 10) return 'coasting';
      break;
    case 'coasting':
      if (vel <= 0) return 'apogee';
      break;
    case 'apogee':
      if (vel < -15) return 'drogue';
      break;
    case 'drogue':
      // main deploys when descent rate has slowed enough
      if (Math.abs(vel) < 45 && Math.abs(vel) > 3) return 'main';
      if (alt < 25 && Math.abs(vel) < 8) return 'landed';
      break;
    case 'main':
      if (alt < 25 && Math.abs(vel) < 8) return 'landed';
      break;
    default:
      break;
  }
  return phase;
}

// ---------------------------------------------------------------------------
// Core transform: raw receiver fields → full dashboard packet
// ---------------------------------------------------------------------------
// Compute quaternion rotating vector 'from' to vector 'to' (both unit vectors)
function quatBetween(from, to) {
  const dot = from[0]*to[0] + from[1]*to[1] + from[2]*to[2];
  if (dot > 0.9999) return { w:1, x:0, y:0, z:0 };
  if (dot < -0.9999) {
    const perp = Math.abs(from[0]) < 0.9 ? [1,0,0] : [0,1,0];
    const cx = from[1]*perp[2] - from[2]*perp[1];
    const cy = from[2]*perp[0] - from[0]*perp[2];
    const cz = from[0]*perp[1] - from[1]*perp[0];
    const cm = Math.sqrt(cx*cx+cy*cy+cz*cz);
    return { w:0, x:cx/cm, y:cy/cm, z:cz/cm };
  }
  const cx = from[1]*to[2] - from[2]*to[1];
  const cy = from[2]*to[0] - from[0]*to[2];
  const cz = from[0]*to[1] - from[1]*to[0];
  const w = 1 + dot;
  const m = Math.sqrt(w*w + cx*cx + cy*cy + cz*cz);
  return { w:w/m, x:cx/m, y:cy/m, z:cz/m };
}

function processRawPackets(rawPackets) {
  if (!rawPackets.length) return [];

  ensureTimestamps(rawPackets);
  rawPackets.sort((a, b) => a.t - b.t);

  const restNorm = estimateRestingNorm(rawPackets);

  // Find the last stable static window before any high-G event.
  // This gives the gravity vector in the body frame when the rocket is on the pad
  // (upright), regardless of how the IMU was oriented during early recording.
  let firstHighGIdx = rawPackets.length;
  for (let i = 0; i < rawPackets.length; i++) {
    const p = rawPackets[i];
    const m = Math.sqrt((p.ax??0)**2 + (p.ay??0)**2 + (p.az??0)**2);
    if (m > restNorm * 1.5) { firstHighGIdx = i; break; }
  }
  const refStart = Math.max(0, firstHighGIdx - 30);
  const refPkts  = rawPackets.slice(refStart, firstHighGIdx);
  let gRefX = 0, gRefY = 0, gRefZ = 0;
  for (const p of (refPkts.length ? refPkts : rawPackets.slice(0, 30))) {
    gRefX += p.ax ?? 0; gRefY += p.ay ?? 0; gRefZ += p.az ?? 0;
  }
  const refN = (refPkts.length || 30);
  gRefX /= refN; gRefY /= refN; gRefZ /= refN;
  const gRefMag = Math.sqrt(gRefX*gRefX + gRefY*gRefY + gRefZ*gRefZ);
  if (gRefMag > 0.1) { gRefX /= gRefMag; gRefY /= gRefMag; gRefZ /= gRefMag; }
  else               { gRefX = 0; gRefY = 0; gRefZ = 1; }

  // Initial quaternion: maps the measured pad gravity (gRef) to world "down" (0,0,1)
  // so the Mahony filter's estimated gravity equals gRef when the rocket is upright.
  // Taring (q0_conj * q) then makes the pad orientation appear as identity in 3D.
  const q0 = quatBetween([gRefX, gRefY, gRefZ], [0, 0, 1]);
  let qw = q0.w, qx = q0.x, qy = q0.y, qz = q0.z;

  // Mahony filter gains
  const Kp = 2.0, Ki = 0.005;
  let exInt = 0, eyInt = 0, ezInt = 0;

  let accelMax = 0, altMax = 0, phase = 'pad', vel = 0, prevAlt = null, prevT = null;

  return rawPackets.map((raw) => {
    const ax = raw.ax ?? 0;
    const ay = raw.ay ?? 0;
    const az = raw.az ?? 0;
    const gxDeg = raw.gx ?? 0;
    const gyDeg = raw.gy ?? 0;
    const gzDeg = raw.gz ?? 0;
    // Convert gyro °/s → rad/s
    const gxr = gxDeg * Math.PI / 180;
    const gyr = gyDeg * Math.PI / 180;
    const gzr = gzDeg * Math.PI / 180;

    const dt = prevT !== null ? Math.max(0.001, raw.t - prevT) : DEFAULT_DT;
    prevT = raw.t;

    const accelMag = Math.sqrt(ax*ax + ay*ay + az*az) / restNorm;
    accelMax = Math.max(accelMax, accelMag);

    // Estimated gravity direction in body frame from current quaternion
    // (third column of rotation matrix, i.e. q_conj rotates world-down (0,0,1) into body)
    const vx = 2*(qx*qz - qw*qy);
    const vy = 2*(qw*qx + qy*qz);
    const vz = qw*qw - qx*qx - qy*qy + qz*qz;

    let gxFilt = gxr, gyFilt = gyr, gzFilt = gzr;

    // Accel correction when not in high-G (Mahony proportional-integral feedback)
    const accelRaw = Math.sqrt(ax*ax + ay*ay + az*az);
    if (accelRaw > 0 && accelMag > 0.5 && accelMag < 1.6) {
      const ax_n = ax / accelRaw;
      const ay_n = ay / accelRaw;
      const az_n = az / accelRaw;
      // Error = measured_gravity × estimated_gravity (cross product)
      const ex = ay_n*vz - az_n*vy;
      const ey = az_n*vx - ax_n*vz;
      const ez = ax_n*vy - ay_n*vx;
      exInt += ex * Ki * dt;
      eyInt += ey * Ki * dt;
      ezInt += ez * Ki * dt;
      gxFilt = gxr + Kp*ex + exInt;
      gyFilt = gyr + Kp*ey + eyInt;
      gzFilt = gzr + Kp*ez + ezInt;
    }

    // Quaternion integration: dq = 0.5 * q ⊗ [0, ω]
    const dqw = 0.5*(-qx*gxFilt - qy*gyFilt - qz*gzFilt);
    const dqx = 0.5*( qw*gxFilt + qy*gzFilt - qz*gyFilt);
    const dqy = 0.5*( qw*gyFilt - qx*gzFilt + qz*gxFilt);
    const dqz = 0.5*( qw*gzFilt + qx*gyFilt - qy*gxFilt);
    qw += dqw*dt; qx += dqx*dt; qy += dqy*dt; qz += dqz*dt;
    const qm = Math.sqrt(qw*qw + qx*qx + qy*qy + qz*qz);
    qw /= qm; qx /= qm; qy /= qm; qz /= qm;

    // Tare: q_rel = q0_conjugate * q  →  pad position = identity in 3D
    const tw =  q0.w*qw + q0.x*qx + q0.y*qy + q0.z*qz;
    const tx =  q0.w*qx - q0.x*qw - q0.y*qz + q0.z*qy;
    const ty =  q0.w*qy + q0.x*qz - q0.y*qw - q0.z*qx;
    const tz =  q0.w*qz - q0.x*qy + q0.y*qx - q0.z*qw;
    const tm = Math.sqrt(tw*tw+tx*tx+ty*ty+tz*tz);
    const fqw=tw/tm, fqx=tx/tm, fqy=ty/tm, fqz=tz/tm;

    // Euler angles from tared quaternion
    const sinRcP = 2*(fqw*fqx + fqy*fqz);
    const cosRcP = 1 - 2*(fqx*fqx + fqy*fqy);
    const taredRoll  = Math.atan2(sinRcP, cosRcP) * 180/Math.PI;
    const sinP = Math.max(-1, Math.min(1, 2*(fqw*fqy - fqz*fqx)));
    const taredPitch = Math.asin(sinP) * 180/Math.PI;
    const sinYcP = 2*(fqw*fqz + fqx*fqy);
    const cosYcP = 1 - 2*(fqy*fqy + fqz*fqz);
    const taredYaw   = Math.atan2(sinYcP, cosYcP) * 180/Math.PI;

    // Altitude, velocity, phase
    const alt = raw.baro_alt ?? 0;
    altMax = Math.max(altMax, alt);
    if (prevAlt !== null && dt > 0) vel = (alt - prevAlt) / dt;
    prevAlt = alt;
    phase = advancePhase(phase, accelMag, vel, alt);

    return {
      t:         raw.t,
      phase,
      alt:       +alt.toFixed(1),
      baro_alt:  +alt.toFixed(1),
      vel:       +vel.toFixed(2),
      accel:     +accelMag.toFixed(3),
      accel_max: +accelMax.toFixed(3),
      alt_max:   +altMax.toFixed(1),
      qw: +fqw.toFixed(5), qx: +fqx.toFixed(5),
      qy: +fqy.toFixed(5), qz: +fqz.toFixed(5),
      roll:  +taredRoll.toFixed(2),
      pitch: +taredPitch.toFixed(2),
      yaw:   +taredYaw.toFixed(2),
      gyr_x: +gxDeg.toFixed(2),
      gyr_y: +gyDeg.toFixed(2),
      gyr_z: +gzDeg.toFixed(2),
      lat: raw.lat ?? 0,
      lon: raw.lon ?? 0,
      gps_alt: raw.gps_alt ?? null,
      gps_sats: raw.gps_fix ?? 0,
      hdop: 1.0,
      bat:  raw.bat  ?? 0,
      rssi: raw.rssi ?? -120,
      rec_drogue: ['drogue', 'main', 'landed'].includes(phase) ? 1 : 0,
      rec_main:   ['main', 'landed'].includes(phase) ? 1 : 0,
      temp: raw.altTemp ?? raw.temp ?? 0,
    };
  });
}

// ---------------------------------------------------------------------------
// Component
// ---------------------------------------------------------------------------
export default function ReplayUpload({ onPacketsLoaded, onBack }) {
  const [dragging, setDragging] = useState(false);
  const [error, setError]       = useState(null);
  const [loading, setLoading]   = useState(false);
  const inputRef = useRef(null);

  function processFile(file) {
    if (!file) return;
    if (!file.name.toLowerCase().endsWith('.csv')) {
      setError('Please select a .csv file.');
      return;
    }
    setLoading(true);
    setError(null);
    const reader = new FileReader();
    reader.onload = (e) => {
      try {
        const raw = parseCSV(e.target.result);
        if (raw.length === 0) {
          setError('No rows found — check that the CSV has a header row.');
          setLoading(false);
          return;
        }
        const packets = processRawPackets(raw);
        if (packets.length === 0) {
          setError('Processing produced no valid packets.');
          setLoading(false);
          return;
        }
        onPacketsLoaded(packets);
      } catch (err) {
        setError('Parse error: ' + err.message);
        setLoading(false);
      }
    };
    reader.onerror = () => { setError('Failed to read file.'); setLoading(false); };
    reader.readAsText(file);
  }

  function onDrop(e) {
    e.preventDefault();
    setDragging(false);
    processFile(e.dataTransfer.files[0]);
  }

  const borderColor = dragging ? '#58a6ff' : error ? '#f85149' : '#30363d';

  return (
    <div
      style={{
        height: '100vh', width: '100vw',
        background: '#0d1117', color: '#e6edf3',
        fontFamily: "'JetBrains Mono', 'Fira Code', 'Courier New', monospace",
        display: 'flex', flexDirection: 'column', alignItems: 'center',
        justifyContent: 'center', gap: 32,
      }}
    >
      <div style={{ textAlign: 'center' }}>
        <button
          onClick={onBack}
          style={{
            background: 'none', border: 'none', color: '#58a6ff',
            cursor: 'pointer', fontSize: 11, fontFamily: 'inherit',
            letterSpacing: 1, display: 'block', margin: '0 auto 20px',
          }}
        >
          ← BACK
        </button>
        <div style={{ fontSize: 10, color: '#58a6ff', letterSpacing: 4, marginBottom: 12 }}>REPLAY FLIGHT</div>
        <div style={{ fontSize: 22, fontWeight: 700, letterSpacing: 2, marginBottom: 8 }}>UPLOAD TELEMETRY LOG</div>
        <div style={{ fontSize: 11, color: '#8b949e' }}>
          CSV in Bridge.pi / nosecone receiver format — roll, pitch, yaw are computed on load
        </div>
      </div>

      {/* Drop zone */}
      <div
        onDragOver={(e) => { e.preventDefault(); setDragging(true); }}
        onDragLeave={() => setDragging(false)}
        onDrop={onDrop}
        onClick={() => !loading && inputRef.current?.click()}
        style={{
          width: 500, height: 180,
          border: `2px dashed ${borderColor}`,
          borderRadius: 12,
          background: dragging ? '#161b22' : 'transparent',
          display: 'flex', flexDirection: 'column', alignItems: 'center',
          justifyContent: 'center', gap: 12,
          cursor: loading ? 'default' : 'pointer',
          transition: 'all 0.15s',
        }}
      >
        <div style={{ fontSize: 38, color: dragging ? '#58a6ff' : '#484f58', transition: 'color 0.15s' }}>⊞</div>
        <div style={{ color: '#8b949e', fontSize: 13 }}>
          {loading ? 'Processing packets…' : 'Drag & drop a CSV, or click to browse'}
        </div>
        <div style={{ color: '#484f58', fontSize: 10 }}>.csv only</div>
        <input
          ref={inputRef} type="file" accept=".csv"
          style={{ display: 'none' }}
          onChange={(e) => processFile(e.target.files[0])}
        />
      </div>

      {error && (
        <div style={{ color: '#f85149', fontSize: 12, maxWidth: 500, textAlign: 'center' }}>{error}</div>
      )}

      {/* Format reference */}
      <div style={{ width: 500, background: '#161b22', border: '1px solid #30363d', borderRadius: 8, padding: '14px 18px' }}>
        <div style={{ color: '#484f58', fontSize: 10, letterSpacing: 1, marginBottom: 10 }}>
          EXPECTED CSV COLUMNS  (Bridge.pi nosecone receiver log)
        </div>
        <div style={{ color: '#484f58', fontSize: 10, fontFamily: 'monospace', lineHeight: 2.2 }}>
          <span style={{ color: '#58a6ff88' }}>t</span>
          {', baro_alt, ax, ay, az, gx, gy, gz,'}
          <br />
          {'altTemp, lat, lon, gps_alt, gps_fix, rssi, snr'}
        </div>
        <div style={{ color: '#30363d', fontSize: 9, marginTop: 10, lineHeight: 1.6 }}>
          roll / pitch / yaw / quaternion / vel / accel / phase are computed automatically.<br />
          "t" is added by the Bridge.pi logger; rows without it get synthetic timestamps at 10 Hz.
        </div>
      </div>
    </div>
  );
}

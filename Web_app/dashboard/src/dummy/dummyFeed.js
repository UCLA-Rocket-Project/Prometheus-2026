const PAD_LAT = 34.8742;
const PAD_LON = -117.3108;
const RATE_HZ = 10;
const DT = 1 / RATE_HZ;

let intervalId = null;
let simTime = 0;

function lerp(a, b, t) { return a + (b - a) * t; }
function clamp(v, lo, hi) { return Math.max(lo, Math.min(hi, v)); }

function easeInOut(t) { return t < 0.5 ? 2 * t * t : -1 + (4 - 2 * t) * t; }

function getPhase(t) {
  if (t < 3) return 'pad';
  if (t < 9) return 'powered';
  if (t < 35) return 'coasting';
  if (t < 35.5) return 'apogee';
  if (t < 90) return 'drogue';
  if (t < 140) return 'main';
  return 'landed';
}

function computeAltitude(t) {
  if (t < 3) return 0;
  if (t < 9) {
    const s = (t - 3) / 6;
    return 1600 * s * s;
  }
  if (t < 35) {
    const s = (t - 9) / 26;
    const sPow = 1 - (1 - s) * (1 - s);
    return lerp(640, 2241, sPow);
  }
  if (t < 35.5) return 2241;
  if (t < 90) {
    const s = (t - 35.5) / 54.5;
    return lerp(2241, 305, s);
  }
  if (t < 140) {
    const s = (t - 90) / 50;
    return lerp(305, 5, s);
  }
  return 0;
}

function computeVelocity(t) {
  if (t < 3) return 0;
  if (t < 9) {
    const s = (t - 3) / 6;
    return lerp(0, 142, s);
  }
  if (t < 35) {
    const s = (t - 9) / 26;
    return lerp(142, 0, easeInOut(s));
  }
  if (t < 35.5) return 0;
  if (t < 90) return -15;
  if (t < 140) return -5;
  return 0;
}

function computeAccel(t) {
  if (t < 3) return 0;
  if (t < 9) {
    const s = (t - 3) / 6;
    if (s < 0.5) return lerp(1, 18.3, s * 2);
    return lerp(18.3, 2, (s - 0.5) * 2);
  }
  if (t < 35) return -1.0 + Math.sin(t * 0.3) * 0.1;
  if (t < 35.5) return 0;
  if (t < 90) return -0.3 + Math.sin(t * 0.2) * 0.05;
  if (t < 140) return -0.1 + Math.sin(t * 0.1) * 0.02;
  if (t < 141) return 8 * Math.exp(-(t - 140) * 3);
  return 0;
}

function computeQuaternion(t) {
  const roll = t < 3 ? 0 : (t - 3) * 2.1 * Math.PI / 180;
  const pitch = t < 3 ? 0 : Math.sin(t * 0.15) * 12 * Math.PI / 180;
  const yaw = t < 3 ? 0 : (t - 3) * 0.8 * Math.PI / 180;

  const cr = Math.cos(roll / 2), sr = Math.sin(roll / 2);
  const cp = Math.cos(pitch / 2), sp = Math.sin(pitch / 2);
  const cy = Math.cos(yaw / 2), sy = Math.sin(yaw / 2);

  const qw = cr * cp * cy + sr * sp * sy;
  const qx = sr * cp * cy - cr * sp * sy;
  const qy = cr * sp * cy + sr * cp * sy;
  const qz = cr * cp * sy - sr * sp * cy;

  const norm = Math.sqrt(qw * qw + qx * qx + qy * qy + qz * qz);
  return { qw: qw / norm, qx: qx / norm, qy: qy / norm, qz: qz / norm };
}

function computeGPS(t) {
  if (t < 3) return { lat: PAD_LAT, lon: PAD_LON };
  const drift_n = (t - 3) * 0.00003;
  const drift_e = (t - 3) * -0.000012;
  const noise_n = (Math.random() - 0.5) * 0.000005;
  const noise_e = (Math.random() - 0.5) * 0.000005;
  return {
    lat: PAD_LAT + drift_n + noise_n,
    lon: PAD_LON + drift_e + noise_e,
  };
}

function buildPacket(t) {
  const phase = getPhase(t);
  const alt = Math.max(0, computeAltitude(t) + (Math.random() - 0.5) * 2);
  const vel = computeVelocity(t) + (Math.random() - 0.5) * 0.5;
  const accel = computeAccel(t) + (Math.random() - 0.5) * 0.05;
  const accel_max = 18.3;
  const q = computeQuaternion(t);
  const gps = computeGPS(t);

  const rollDeg = Math.atan2(
    2 * (q.qw * q.qx + q.qy * q.qz),
    1 - 2 * (q.qx * q.qx + q.qy * q.qy)
  ) * (180 / Math.PI);
  const pitchDeg = Math.asin(clamp(2 * (q.qw * q.qy - q.qz * q.qx), -1, 1)) * (180 / Math.PI);
  const yawDeg = Math.atan2(
    2 * (q.qw * q.qz + q.qx * q.qy),
    1 - 2 * (q.qy * q.qy + q.qz * q.qz)
  ) * (180 / Math.PI);

  const gyr_scale = t < 3 ? 0.01 : t < 9 ? 8 : t < 35 ? 3 : 0.5;

  return {
    t: parseFloat(t.toFixed(2)),
    phase,
    alt: parseFloat(alt.toFixed(1)),
    vel: parseFloat(vel.toFixed(1)),
    accel: parseFloat(accel.toFixed(2)),
    accel_max,
    qw: parseFloat(q.qw.toFixed(4)),
    qx: parseFloat(q.qx.toFixed(4)),
    qy: parseFloat(q.qy.toFixed(4)),
    qz: parseFloat(q.qz.toFixed(4)),
    roll: parseFloat(rollDeg.toFixed(1)),
    pitch: parseFloat(pitchDeg.toFixed(1)),
    yaw: parseFloat(yawDeg.toFixed(1)),
    gyr_x: parseFloat(((Math.random() - 0.5) * gyr_scale).toFixed(2)),
    gyr_y: parseFloat(((Math.random() - 0.5) * gyr_scale).toFixed(2)),
    gyr_z: parseFloat(((Math.random() - 0.5) * gyr_scale).toFixed(2)),
    lat: parseFloat(gps.lat.toFixed(6)),
    lon: parseFloat(gps.lon.toFixed(6)),
    gps_sats: t < 2 ? 0 : t < 3 ? 6 : 9,
    hdop: t < 3 ? 2.1 : 0.8,
    baro_alt: parseFloat((alt + (Math.random() - 0.5) * 5).toFixed(1)),
    bat: parseFloat((3.92 - t * 0.001).toFixed(2)),
    rssi: Math.round(-74 - alt * 0.01 + (Math.random() - 0.5) * 3),
    rec_drogue: t >= 35.5 ? 1 : 0,
    rec_main: t >= 90 ? 1 : 0,
    temp: parseFloat((18.4 - alt * 0.006 + (Math.random() - 0.5) * 0.2).toFixed(1)),
  };
}

export function startDummyFeed(onPacket) {
  simTime = 0;
  if (intervalId) clearInterval(intervalId);
  intervalId = setInterval(() => {
    const pkt = buildPacket(simTime);
    onPacket(pkt);
    simTime = parseFloat((simTime + DT).toFixed(3));
    if (simTime > 160) simTime = 0;
  }, 1000 / RATE_HZ);
}

export function stopDummyFeed() {
  if (intervalId) {
    clearInterval(intervalId);
    intervalId = null;
  }
}

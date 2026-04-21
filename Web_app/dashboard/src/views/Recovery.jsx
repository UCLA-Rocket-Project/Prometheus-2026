import React, { useMemo } from 'react';

const SIM_TARGETS = {
  apogee: 2241,
  maxVel: 142,
  maxG: 18.3,
  drift: 36,
};

const R_EARTH = 6371000;
function latLonDistance(lat1, lon1, lat2, lon2) {
  const dLat = (lat2 - lat1) * Math.PI / 180;
  const dLon = (lon2 - lon1) * Math.PI / 180;
  const a = Math.sin(dLat / 2) ** 2 +
    Math.cos(lat1 * Math.PI / 180) * Math.cos(lat2 * Math.PI / 180) * Math.sin(dLon / 2) ** 2;
  return R_EARTH * 2 * Math.atan2(Math.sqrt(a), Math.sqrt(1 - a));
}

function bearingDeg(lat1, lon1, lat2, lon2) {
  const dLon = (lon2 - lon1) * Math.PI / 180;
  const y = Math.sin(dLon) * Math.cos(lat2 * Math.PI / 180);
  const x = Math.cos(lat1 * Math.PI / 180) * Math.sin(lat2 * Math.PI / 180) -
    Math.sin(lat1 * Math.PI / 180) * Math.cos(lat2 * Math.PI / 180) * Math.cos(dLon);
  return ((Math.atan2(y, x) * 180 / Math.PI) + 360) % 360;
}

function SummaryCard({ label, value, unit, achieved }) {
  const borderColor = achieved === true ? '#3fb950' : achieved === false ? '#30363d' : '#58a6ff';
  return (
    <div
      style={{
        background: '#161b22',
        border: `1px solid ${borderColor}`,
        borderRadius: 8,
        padding: '14px 18px',
        flex: 1,
      }}
    >
      <div style={{ color: '#8b949e', fontSize: 11, letterSpacing: 1, marginBottom: 6 }}>{label}</div>
      <div style={{ color: '#e6edf3', fontSize: 26, fontFamily: 'monospace', fontWeight: 700 }}>
        {value}
        {unit && <span style={{ color: '#8b949e', fontSize: 12, marginLeft: 4 }}>{unit}</span>}
      </div>
    </div>
  );
}

function TimelineEvent({ time, label, status, color, active }) {
  const dotColor = status === 'done' ? color : status === 'pending' ? '#e3b341' : '#30363d';
  return (
    <div style={{ display: 'flex', gap: 12, alignItems: 'flex-start', opacity: active ? 1 : 0.45 }}>
      <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', paddingTop: 2 }}>
        <div style={{ width: 10, height: 10, borderRadius: '50%', background: dotColor, flexShrink: 0, border: `2px solid ${dotColor}44` }} />
        <div style={{ width: 1, height: 28, background: '#30363d', marginTop: 2 }} />
      </div>
      <div style={{ flex: 1, paddingBottom: 8 }}>
        <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
          <span style={{ color: dotColor, fontSize: 13, fontWeight: 600 }}>{label}</span>
          {time && <span style={{ color: '#8b949e', fontSize: 11, fontFamily: 'monospace' }}>{time}</span>}
        </div>
      </div>
    </div>
  );
}

function FinderCompass({ bearing, distance }) {
  const cx = 70, cy = 70, r = 52;
  const rad = (bearing - 90) * Math.PI / 180;
  const ax = cx + r * 0.75 * Math.cos(rad);
  const ay = cy + r * 0.75 * Math.sin(rad);

  return (
    <div style={{ textAlign: 'center' }}>
      <svg width={140} height={140} style={{ display: 'block', margin: '0 auto' }}>
        <circle cx={cx} cy={cy} r={r} fill="#0d1117" stroke="#30363d" strokeWidth={1.5} />
        <circle cx={cx} cy={cy} r={r * 0.6} fill="none" stroke="#21262d" strokeWidth={1} />
        {['N', 'NE', 'E', 'SE', 'S', 'SW', 'W', 'NW'].map((dir, i) => {
          const angle = (i * 45 - 90) * Math.PI / 180;
          const dist = dir.length === 1 ? r - 9 : r - 13;
          return (
            <text key={dir} x={cx + dist * Math.cos(angle)} y={cy + dist * Math.sin(angle) + 3}
              fill={dir === 'N' ? '#f85149' : '#8b949e'} fontSize={dir.length === 1 ? 9 : 7}
              textAnchor="middle" fontFamily="monospace">
              {dir}
            </text>
          );
        })}
        <line x1={cx} y1={cy} x2={ax} y2={ay} stroke="#58a6ff" strokeWidth={2.5} strokeLinecap="round" />
        <circle cx={ax} cy={ay} r={4} fill="#58a6ff" />
        <circle cx={cx} cy={cy} r={4} fill="#e6edf3" />
      </svg>
      <div style={{ color: '#e6edf3', fontFamily: 'monospace', fontSize: 16, marginTop: 6 }}>
        {bearing.toFixed(0)}°
      </div>
      <div style={{ color: '#58a6ff', fontSize: 13, marginTop: 2 }}>
        Walk {distance.toFixed(0)}m → {bearing.toFixed(0)}°
      </div>
    </div>
  );
}

export default function Recovery({ latest, history, padCoords }) {
  const padLat = padCoords?.lat ?? 34.8742;
  const padLon = padCoords?.lon ?? -117.3108;

  const milestones = useMemo(() => {
    const fmt = (t) => {
      if (t == null) return null;
      const m = Math.floor(t / 60).toString().padStart(2, '0');
      const s = Math.floor(t % 60).toString().padStart(2, '0');
      return `T+${m}:${s}`;
    };
    const findPhaseTime = (phase) => {
      const pkt = history.find((p) => p.phase === phase);
      return pkt ? pkt.t : null;
    };
    const launchT = findPhaseTime('powered');
    const burnoutT = findPhaseTime('coasting');
    const apogeeT = findPhaseTime('apogee');
    const drogueT = findPhaseTime('drogue');
    const mainT = findPhaseTime('main');
    const landedT = findPhaseTime('landed');

    const phase = latest?.phase ?? 'pad';
    const phaseOrder = ['pad', 'powered', 'coasting', 'apogee', 'drogue', 'main', 'landed'];
    const pi = phaseOrder.indexOf(phase);

    return [
      { label: 'Launch detected', time: fmt(launchT), color: '#f0883e', done: pi >= 1 },
      { label: 'Motor burnout', time: fmt(burnoutT), color: '#58a6ff', done: pi >= 2 },
      { label: 'Apogee detected', time: fmt(apogeeT), color: '#bc8cff', done: pi >= 3 },
      { label: 'Drogue deploy commanded', time: fmt(drogueT), color: '#e3b341', done: pi >= 4 },
      { label: 'Drogue confirmed', time: fmt(drogueT), color: '#e3b341', done: pi >= 4 && latest?.rec_drogue === 1 },
      { label: 'Main deploy commanded', time: fmt(mainT), color: '#3fb950', done: pi >= 5 },
      { label: 'Main confirmed', time: fmt(mainT), color: '#3fb950', done: pi >= 5 && latest?.rec_main === 1 },
      { label: 'Touchdown', time: fmt(landedT), color: '#3fb950', done: pi >= 6 },
    ];
  }, [history, latest]);

  const maxAlt = useMemo(() => Math.max(0, ...history.map((p) => p.alt ?? 0)), [history]);
  const maxVel = useMemo(() => Math.max(0, ...history.map((p) => Math.abs(p.vel ?? 0))), [history]);
  const maxG = useMemo(() => Math.max(0, ...history.map((p) => p.accel_max ?? 0)), [history]);

  const phase = latest?.phase ?? 'pad';
  const phaseOrder = ['pad', 'powered', 'coasting', 'apogee', 'drogue', 'main', 'landed'];
  const pi = phaseOrder.indexOf(phase);

  const lat = latest?.lat ?? padLat;
  const lon = latest?.lon ?? padLon;
  const dist = latLonDistance(padLat, padLon, lat, lon);
  const bearing = bearingDeg(padLat, padLon, lat, lon);

  const postApogee = pi >= 3;
  const landed = phase === 'landed';

  return (
    <div style={{ height: '100%', display: 'flex', flexDirection: 'column', gap: 16, padding: 20, overflowY: 'auto' }}>
      <div style={{ display: 'flex', gap: 12 }}>
        <SummaryCard label="APOGEE" value={maxAlt.toFixed(0)} unit="m" achieved={pi >= 3} />
        <SummaryCard label="MAX VELOCITY" value={maxVel.toFixed(1)} unit="m/s" achieved={maxVel > 0} />
        <SummaryCard label="MAX G" value={maxG.toFixed(2)} unit="G" achieved={maxG > 0} />
        <SummaryCard
          label="LANDING DRIFT"
          value={landed ? dist.toFixed(0) : '—'}
          unit={landed ? 'm' : ''}
          achieved={landed}
        />
      </div>

      <div style={{ display: 'flex', gap: 16, flex: 1, minHeight: 0 }}>
        <div
          style={{
            flex: 1,
            background: '#161b22',
            border: '1px solid #30363d',
            borderRadius: 8,
            padding: '16px 20px',
            overflowY: 'auto',
          }}
        >
          <div style={{ color: '#8b949e', fontSize: 11, letterSpacing: 1, marginBottom: 14 }}>RECOVERY SEQUENCE</div>
          {milestones.map((m, i) => (
            <TimelineEvent
              key={i}
              time={m.time}
              label={m.label}
              status={m.done ? 'done' : 'pending'}
              color={m.color}
              active={m.done || pi >= 1}
            />
          ))}
        </div>

        <div style={{ flex: 1, display: 'flex', flexDirection: 'column', gap: 14 }}>
          <div
            style={{
              background: '#161b22',
              border: '1px solid #30363d',
              borderRadius: 8,
              padding: '16px 20px',
            }}
          >
            <div style={{ color: '#8b949e', fontSize: 11, letterSpacing: 1, marginBottom: 14 }}>FINDER COMPASS</div>
            <FinderCompass bearing={bearing} distance={dist} />
          </div>

          <div
            style={{
              background: '#161b22',
              border: '1px solid #30363d',
              borderRadius: 8,
              padding: '16px 20px',
              flex: 1,
            }}
          >
            <div style={{ color: '#8b949e', fontSize: 11, letterSpacing: 1, marginBottom: 12 }}>POST-FLIGHT COMPARISON</div>
            <table style={{ width: '100%', borderCollapse: 'collapse', fontSize: 12 }}>
              <thead>
                <tr>
                  {['Metric', 'Actual', 'Simulated', 'Delta'].map((h) => (
                    <th key={h} style={{ color: '#8b949e', fontWeight: 500, textAlign: 'left', padding: '5px 6px', borderBottom: '1px solid #30363d' }}>
                      {h}
                    </th>
                  ))}
                </tr>
              </thead>
              <tbody>
                {[
                  { metric: 'Apogee (m)', actual: maxAlt.toFixed(0), sim: SIM_TARGETS.apogee, delta: (maxAlt - SIM_TARGETS.apogee).toFixed(0) },
                  { metric: 'Max vel (m/s)', actual: maxVel.toFixed(1), sim: SIM_TARGETS.maxVel, delta: (maxVel - SIM_TARGETS.maxVel).toFixed(1) },
                  { metric: 'Max G', actual: maxG.toFixed(2), sim: SIM_TARGETS.maxG, delta: (maxG - SIM_TARGETS.maxG).toFixed(2) },
                  { metric: 'Drift (m)', actual: landed ? dist.toFixed(0) : '—', sim: SIM_TARGETS.drift, delta: landed ? (dist - SIM_TARGETS.drift).toFixed(0) : '—' },
                ].map((row, i) => {
                  const delta = parseFloat(row.delta);
                  const dColor = isNaN(delta) ? '#8b949e' : Math.abs(delta) < 5 ? '#3fb950' : '#e3b341';
                  return (
                    <tr key={i} style={{ borderBottom: '1px solid #21262d' }}>
                      <td style={{ color: '#8b949e', padding: '7px 6px' }}>{row.metric}</td>
                      <td style={{ color: '#e6edf3', padding: '7px 6px', fontFamily: 'monospace' }}>{row.actual}</td>
                      <td style={{ color: '#8b949e', padding: '7px 6px', fontFamily: 'monospace' }}>{row.sim}</td>
                      <td style={{ color: dColor, padding: '7px 6px', fontFamily: 'monospace' }}>
                        {isNaN(delta) ? row.delta : (delta >= 0 ? '+' : '')}{row.delta}
                      </td>
                    </tr>
                  );
                })}
              </tbody>
            </table>
          </div>
        </div>
      </div>
    </div>
  );
}

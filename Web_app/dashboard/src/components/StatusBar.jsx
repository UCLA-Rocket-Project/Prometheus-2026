import React from 'react';

const PHASE_COLORS = {
  pad:      { bg: '#21262d', text: '#8b949e', label: 'PAD HOLD' },
  powered:  { bg: '#3d2600', text: '#f0883e', label: 'POWERED' },
  coasting: { bg: '#0c2d48', text: '#58a6ff', label: 'COASTING' },
  apogee:   { bg: '#2e1f5e', text: '#bc8cff', label: 'APOGEE' },
  drogue:   { bg: '#3d2200', text: '#e3b341', label: 'DROGUE' },
  main:     { bg: '#0d2818', text: '#3fb950', label: 'MAIN' },
  landed:   { bg: '#0d2818', text: '#3fb950', label: 'LANDED' },
};

function formatFlightTime(seconds) {
  const abs = Math.abs(seconds);
  const m = Math.floor(abs / 60).toString().padStart(2, '0');
  const s = Math.floor(abs % 60).toString().padStart(2, '0');
  return `T+${m}:${s}`;
}

function RssiBar({ rssi }) {
  const normalized = Math.max(0, Math.min(1, (rssi + 120) / 80));
  const bars = Math.ceil(normalized * 4);
  const color = bars >= 3 ? '#3fb950' : bars === 2 ? '#e3b341' : '#f85149';
  return (
    <div style={{ display: 'flex', alignItems: 'center', gap: 6 }}>
      <div style={{ display: 'flex', alignItems: 'flex-end', gap: 2, height: 16 }}>
        {[1, 2, 3, 4].map((b) => (
          <div
            key={b}
            style={{
              width: 4,
              height: 4 + b * 3,
              background: b <= bars ? color : '#30363d',
              borderRadius: 1,
            }}
          />
        ))}
      </div>
      <span style={{ color: '#8b949e', fontSize: 12 }}>{rssi} dBm</span>
    </div>
  );
}

export default function StatusBar({ latest, flightTime, connected, replayMode }) {
  const phase = latest?.phase ?? 'pad';
  const phaseStyle = PHASE_COLORS[phase] ?? PHASE_COLORS.pad;
  const rssi = latest?.rssi ?? -120;

  return (
    <div
      style={{
        background: '#161b22',
        borderBottom: '1px solid #30363d',
        padding: '0 20px',
        height: 44,
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'flex-end',
        flexShrink: 0,
      }}
    >
      <div style={{ display: 'flex', alignItems: 'center', gap: 24 }}>
        <div
          style={{
            background: phaseStyle.bg,
            color: phaseStyle.text,
            padding: '3px 10px',
            borderRadius: 4,
            fontSize: 11,
            fontWeight: 700,
            letterSpacing: 1.5,
            border: `1px solid ${phaseStyle.text}44`,
          }}
        >
          {phaseStyle.label}
        </div>

        <span style={{ color: '#e6edf3', fontSize: 14, fontWeight: 600, letterSpacing: 1 }}>
          {formatFlightTime(flightTime)}
        </span>

        <RssiBar rssi={rssi} />

        {replayMode ? (
          <div style={{ display: 'flex', alignItems: 'center', gap: 6 }}>
            <div style={{ width: 8, height: 8, borderRadius: '50%', background: '#58a6ff' }} />
            <span style={{ fontSize: 11, color: '#58a6ff', letterSpacing: 0.5 }}>REPLAY</span>
          </div>
        ) : (
          <div style={{ display: 'flex', alignItems: 'center', gap: 6 }}>
            <div
              style={{
                width: 8,
                height: 8,
                borderRadius: '50%',
                background: connected ? '#3fb950' : '#f85149',
                position: 'relative',
              }}
              className={connected ? 'pulse-dot' : ''}
            />
            <span style={{ fontSize: 11, color: connected ? '#3fb950' : '#f85149' }}>
              {connected ? 'LIVE' : 'NO SIGNAL'}
            </span>
          </div>
        )}
      </div>
    </div>
  );
}

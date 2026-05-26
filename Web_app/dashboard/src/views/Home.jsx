import React, { useState } from 'react';

function ModeCard({ icon, title, description, onClick, accent }) {
  const [hovered, setHovered] = useState(false);
  return (
    <button
      onClick={onClick}
      onMouseEnter={() => setHovered(true)}
      onMouseLeave={() => setHovered(false)}
      style={{
        background: hovered ? '#161b22' : 'transparent',
        border: `2px solid ${hovered ? accent : '#30363d'}`,
        borderRadius: 12,
        padding: '48px 40px',
        cursor: 'pointer',
        color: '#e6edf3',
        fontFamily: "'JetBrains Mono', 'Fira Code', 'Courier New', monospace",
        textAlign: 'center',
        width: 300,
        transition: 'all 0.15s ease',
        display: 'flex',
        flexDirection: 'column',
        alignItems: 'center',
        gap: 20,
      }}
    >
      <div style={{ fontSize: 52, lineHeight: 1, color: hovered ? accent : '#484f58', transition: 'color 0.15s' }}>
        {icon}
      </div>
      <div>
        <div
          style={{
            fontSize: 16,
            fontWeight: 700,
            color: hovered ? accent : '#e6edf3',
            letterSpacing: 2,
            marginBottom: 10,
            transition: 'color 0.15s',
          }}
        >
          {title}
        </div>
        <div style={{ fontSize: 11, color: '#8b949e', lineHeight: 1.7, whiteSpace: 'pre-line' }}>
          {description}
        </div>
      </div>
      <div
        style={{
          marginTop: 4,
          padding: '6px 22px',
          border: `1px solid ${hovered ? accent : '#30363d'}`,
          borderRadius: 6,
          fontSize: 10,
          color: hovered ? accent : '#484f58',
          letterSpacing: 2,
          transition: 'all 0.15s',
        }}
      >
        SELECT →
      </div>
    </button>
  );
}

export default function Home({ onSelectMode }) {
  return (
    <div
      style={{
        height: '100vh',
        width: '100vw',
        background: '#0d1117',
        color: '#e6edf3',
        fontFamily: "'JetBrains Mono', 'Fira Code', 'Courier New', monospace",
        display: 'flex',
        flexDirection: 'column',
        alignItems: 'center',
        justifyContent: 'center',
        gap: 72,
        overflow: 'hidden',
      }}
    >
      <div style={{ textAlign: 'center' }}>
        <div style={{ fontSize: 10, color: '#58a6ff', letterSpacing: 5, marginBottom: 14 }}>
          PROMETHEUS 2026
        </div>
        <div style={{ fontSize: 32, fontWeight: 700, letterSpacing: 3, marginBottom: 10 }}>
          GROUND STATION
        </div>
        <div style={{ width: 60, height: 1, background: '#30363d', margin: '0 auto 14px' }} />
        <div style={{ fontSize: 11, color: '#8b949e', letterSpacing: 1 }}>
          Select an operating mode to continue
        </div>
      </div>

      <div style={{ display: 'flex', gap: 48 }}>
        <ModeCard
          icon="◎"
          title="LIVE TELEMETRY"
          description={"Connect to the live ground\nstation over WebSocket.\nReal-time flight monitoring."}
          onClick={() => onSelectMode('live')}
          accent="#3fb950"
        />
        <ModeCard
          icon="⊞"
          title="REPLAY FLIGHT"
          description={"Upload a CSV telemetry log\nto simulate a past flight\nthrough the full interface."}
          onClick={() => onSelectMode('replay')}
          accent="#58a6ff"
        />
      </div>

      <div style={{ fontSize: 9, color: '#30363d', letterSpacing: 2 }}>
        UCLA ROCKET PROJECT — LAUNCH VEHICLE SYSTEMS
      </div>
    </div>
  );
}

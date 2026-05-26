import React from 'react';

const SPEEDS = [0.5, 1, 2, 5, 10];

function formatDur(seconds) {
  if (!seconds || isNaN(seconds) || seconds < 0) return '0:00';
  const s = Math.floor(seconds);
  const m = Math.floor(s / 60);
  return `${m}:${(s % 60).toString().padStart(2, '0')}`;
}

function Btn({ onClick, children, active, title, style = {} }) {
  return (
    <button
      onClick={onClick}
      title={title}
      style={{
        background: active ? '#21262d' : 'none',
        border: `1px solid ${active ? '#58a6ff' : '#30363d'}`,
        borderRadius: 4,
        color: active ? '#58a6ff' : '#8b949e',
        cursor: 'pointer',
        padding: '4px 10px',
        fontFamily: "'JetBrains Mono', 'Fira Code', 'Courier New', monospace",
        fontSize: 11,
        letterSpacing: 0.5,
        lineHeight: 1.4,
        transition: 'all 0.1s',
        whiteSpace: 'nowrap',
        ...style,
      }}
    >
      {children}
    </button>
  );
}

export default function ReplayControls({
  playing,
  togglePlay,
  speed,
  setSpeed,
  seek,
  restart,
  currentIndex,
  totalPackets,
  currentDuration,
  totalDuration,
  packetRateMs,
  onBack,
}) {
  const scrubMax = Math.max(1, totalPackets - 1);

  return (
    <div
      style={{
        background: '#161b22',
        borderTop: '1px solid #30363d',
        padding: '0 20px',
        height: 52,
        display: 'flex',
        alignItems: 'center',
        gap: 16,
        flexShrink: 0,
        overflow: 'hidden',
      }}
    >
      {/* Exit */}
      <Btn onClick={onBack} title="Back to home screen">
        ← EXIT
      </Btn>

      {/* Restart */}
      <Btn onClick={restart} title="Restart from beginning" style={{ fontSize: 16, padding: '2px 8px' }}>
        ⟳
      </Btn>

      {/* Play / Pause */}
      <button
        onClick={togglePlay}
        style={{
          background: playing ? '#161b22' : '#238636',
          border: `1px solid ${playing ? '#30363d' : '#2ea043'}`,
          borderRadius: 4,
          color: '#e6edf3',
          cursor: 'pointer',
          padding: '5px 16px',
          fontFamily: "'JetBrains Mono', 'Fira Code', 'Courier New', monospace",
          fontSize: 11,
          letterSpacing: 0.5,
          minWidth: 76,
          transition: 'all 0.1s',
        }}
      >
        {playing ? '⏸ PAUSE' : '▶  PLAY'}
      </button>

      {/* Speed selector */}
      <div style={{ display: 'flex', alignItems: 'center', gap: 4 }}>
        <span style={{ color: '#484f58', fontSize: 9, letterSpacing: 1, marginRight: 4 }}>SPEED</span>
        {SPEEDS.map((s) => (
          <Btn key={s} onClick={() => setSpeed(s)} active={speed === s}>
            {s}×
          </Btn>
        ))}
      </div>

      {/* Divider */}
      <div style={{ width: 1, height: 28, background: '#30363d', flexShrink: 0 }} />

      {/* Scrubber */}
      <div style={{ flex: 1, display: 'flex', alignItems: 'center', gap: 10, minWidth: 0 }}>
        <span style={{ color: '#8b949e', fontSize: 11, fontFamily: 'monospace', whiteSpace: 'nowrap' }}>
          {formatDur(currentDuration)}
        </span>
        <input
          type="range"
          min={0}
          max={scrubMax}
          value={currentIndex}
          onChange={(e) => seek(+e.target.value)}
          style={{ flex: 1, accentColor: '#58a6ff', cursor: 'pointer', minWidth: 0 }}
        />
        <span style={{ color: '#484f58', fontSize: 11, fontFamily: 'monospace', whiteSpace: 'nowrap' }}>
          {formatDur(totalDuration)}
        </span>
      </div>

      {/* Divider */}
      <div style={{ width: 1, height: 28, background: '#30363d', flexShrink: 0 }} />

      {/* Packet stats */}
      <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'flex-end', gap: 2, flexShrink: 0 }}>
        <span style={{ color: '#484f58', fontSize: 9, letterSpacing: 1 }}>NOSE CONE TX/RX</span>
        <span style={{ color: '#8b949e', fontSize: 11, fontFamily: 'monospace' }}>
          {packetRateMs !== null ? `~${packetRateMs} ms / pkt` : '— ms / pkt'}
        </span>
      </div>

      {/* Packet index */}
      <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'flex-end', gap: 2, flexShrink: 0 }}>
        <span style={{ color: '#484f58', fontSize: 9, letterSpacing: 1 }}>PACKET</span>
        <span style={{ color: '#8b949e', fontSize: 11, fontFamily: 'monospace' }}>
          {currentIndex + 1}/{totalPackets}
        </span>
      </div>
    </div>
  );
}

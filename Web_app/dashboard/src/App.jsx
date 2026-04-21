import React, { useState } from 'react';
import useTelemetry from './hooks/useTelemetry';
import StatusBar from './components/StatusBar';
import Overview from './views/Overview';
import Attitude from './views/Attitude';
import MapView from './views/MapView';
import Recovery from './views/Recovery';

const NAV_ITEMS = [
  { id: 'overview', label: 'Overview', icon: '◈' },
  { id: 'attitude', label: 'Attitude', icon: '⊕' },
  { id: 'map', label: 'Map', icon: '◎' },
  { id: 'recovery', label: 'Recovery', icon: '⊗' },
];

export default function App() {
  const [activeTab, setActiveTab] = useState('overview');
  const { latest, history, connected, flightTime, padCoords } = useTelemetry();

  return (
    <div
      style={{
        height: '100vh',
        width: '100vw',
        display: 'flex',
        flexDirection: 'column',
        background: '#0d1117',
        color: '#e6edf3',
        fontFamily: "'JetBrains Mono', 'Fira Code', 'Courier New', monospace",
        overflow: 'hidden',
      }}
    >
      <StatusBar latest={latest} flightTime={flightTime} connected={connected} />

      <div style={{ display: 'flex', flex: 1, minHeight: 0 }}>
        <nav
          style={{
            width: 180,
            background: '#161b22',
            borderRight: '1px solid #30363d',
            padding: '12px 0',
            display: 'flex',
            flexDirection: 'column',
            gap: 2,
            flexShrink: 0,
          }}
        >
          {NAV_ITEMS.map((item) => {
            const active = activeTab === item.id;
            return (
              <button
                key={item.id}
                onClick={() => setActiveTab(item.id)}
                style={{
                  display: 'flex',
                  alignItems: 'center',
                  gap: 10,
                  padding: '10px 20px',
                  background: active ? '#21262d' : 'transparent',
                  color: active ? '#e6edf3' : '#8b949e',
                  border: 'none',
                  borderLeft: active ? '3px solid #58a6ff' : '3px solid transparent',
                  cursor: 'pointer',
                  fontSize: 13,
                  fontFamily: 'inherit',
                  textAlign: 'left',
                  width: '100%',
                  transition: 'all 0.12s',
                }}
                onMouseEnter={(e) => { if (!active) e.currentTarget.style.color = '#c9d1d9'; }}
                onMouseLeave={(e) => { if (!active) e.currentTarget.style.color = '#8b949e'; }}
              >
                <span style={{ fontSize: 14, opacity: 0.8 }}>{item.icon}</span>
                {item.label}
              </button>
            );
          })}

          <div style={{ marginTop: 'auto', padding: '12px 20px', borderTop: '1px solid #30363d' }}>
            <div style={{ color: '#8b949e', fontSize: 10, letterSpacing: 0.5, marginBottom: 6 }}>TELEMETRY</div>
            <div style={{ color: '#8b949e', fontSize: 11 }}>
              {history.length} pkts
            </div>
            <div style={{ color: '#8b949e', fontSize: 11, marginTop: 2 }}>
              {latest?.t?.toFixed(1) ?? '0.0'} s
            </div>
          </div>
        </nav>

        <main style={{ flex: 1, minWidth: 0, overflow: 'hidden' }}>
          {activeTab === 'overview' && <Overview latest={latest} history={history} />}
          {activeTab === 'attitude' && <Attitude latest={latest} />}
          {activeTab === 'map' && <MapView latest={latest} history={history} padCoords={padCoords} />}
          {activeTab === 'recovery' && <Recovery latest={latest} history={history} padCoords={padCoords} />}
        </main>
      </div>
    </div>
  );
}

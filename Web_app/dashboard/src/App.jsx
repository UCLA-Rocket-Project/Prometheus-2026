import React, { useState } from 'react';
import useTelemetry from './hooks/useTelemetry';
import useReplay from './hooks/useReplay';
import StatusBar from './components/StatusBar';
import ReplayControls from './components/ReplayControls';
import Overview from './views/Overview';
import Attitude from './views/Attitude';
import MapView from './views/MapView';
import Recovery from './views/Recovery';
import Home from './views/Home';
import ReplayUpload from './views/ReplayUpload';

const NAV_ITEMS = [
  { id: 'overview', label: 'Overview', icon: '◈' },
  { id: 'attitude', label: 'Attitude', icon: '⊕' },
  { id: 'map', label: 'Map', icon: '◎' },
  { id: 'recovery', label: 'Recovery', icon: '⊗' },
];

function DashboardLayout({ latest, history, connected, flightTime, padCoords, replayMode, onBack, bottomBar }) {
  const [activeTab, setActiveTab] = useState('overview');

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
      <StatusBar
        latest={latest}
        flightTime={flightTime}
        connected={connected}
        replayMode={replayMode}
      />

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

          <div style={{ marginTop: 'auto', padding: '12px 20px', borderTop: '1px solid #30363d', display: 'flex', flexDirection: 'column', gap: 4 }}>
            {replayMode && (
              <div
                style={{
                  color: '#58a6ff',
                  fontSize: 9,
                  letterSpacing: 2,
                  background: '#58a6ff14',
                  border: '1px solid #58a6ff44',
                  borderRadius: 4,
                  padding: '3px 8px',
                  marginBottom: 6,
                  textAlign: 'center',
                }}
              >
                REPLAY MODE
              </div>
            )}
            {onBack && (
              <button
                onClick={onBack}
                style={{
                  background: 'none',
                  border: '1px solid #30363d',
                  borderRadius: 4,
                  color: '#8b949e',
                  cursor: 'pointer',
                  fontSize: 10,
                  fontFamily: 'inherit',
                  padding: '4px 0',
                  letterSpacing: 0.5,
                  marginBottom: 8,
                  transition: 'color 0.12s',
                }}
                onMouseEnter={(e) => { e.currentTarget.style.color = '#e6edf3'; }}
                onMouseLeave={(e) => { e.currentTarget.style.color = '#8b949e'; }}
              >
                ← HOME
              </button>
            )}
            <div style={{ color: '#8b949e', fontSize: 10, letterSpacing: 0.5, marginBottom: 4 }}>TELEMETRY</div>
            <div style={{ color: '#8b949e', fontSize: 11 }}>{history.length} pkts</div>
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

      {bottomBar}
    </div>
  );
}

function LiveDashboard({ onBack }) {
  const { latest, history, connected, flightTime, padCoords } = useTelemetry();
  return (
    <DashboardLayout
      latest={latest}
      history={history}
      connected={connected}
      flightTime={flightTime}
      padCoords={padCoords}
      replayMode={false}
      onBack={onBack}
    />
  );
}

function ReplayDashboard({ packets, onBack }) {
  const {
    latest, history, connected, flightTime, padCoords,
    playing, togglePlay, speed, setSpeed,
    seek, restart,
    currentIndex, totalPackets,
    packetRateMs, totalDuration, currentDuration,
  } = useReplay(packets);

  const controls = (
    <ReplayControls
      playing={playing}
      togglePlay={togglePlay}
      speed={speed}
      setSpeed={setSpeed}
      seek={seek}
      restart={restart}
      currentIndex={currentIndex}
      totalPackets={totalPackets}
      currentDuration={currentDuration}
      totalDuration={totalDuration}
      packetRateMs={packetRateMs}
      onBack={onBack}
    />
  );

  return (
    <DashboardLayout
      latest={latest}
      history={history}
      connected={connected}
      flightTime={flightTime}
      padCoords={padCoords}
      replayMode={true}
      onBack={null}
      bottomBar={controls}
    />
  );
}

export default function App() {
  const [mode, setMode] = useState('home');
  const [replayPackets, setReplayPackets] = useState(null);

  if (mode === 'home') {
    return <Home onSelectMode={setMode} />;
  }

  if (mode === 'live') {
    return <LiveDashboard onBack={() => setMode('home')} />;
  }

  // replay mode
  if (!replayPackets) {
    return (
      <ReplayUpload
        onPacketsLoaded={setReplayPackets}
        onBack={() => setMode('home')}
      />
    );
  }

  return (
    <ReplayDashboard
      packets={replayPackets}
      onBack={() => { setReplayPackets(null); setMode('home'); }}
    />
  );
}

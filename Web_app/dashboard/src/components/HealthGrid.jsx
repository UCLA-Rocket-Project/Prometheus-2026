import React from 'react';

function HealthRow({ label, value, status, unit }) {
  const dotColor = status === 'ok' ? '#3fb950' : status === 'warn' ? '#e3b341' : '#f85149';
  return (
    <div
      style={{
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        padding: '9px 0',
        borderBottom: '1px solid #21262d',
      }}
    >
      <div style={{ display: 'flex', alignItems: 'center', gap: 10 }}>
        <div style={{ width: 8, height: 8, borderRadius: '50%', background: dotColor, flexShrink: 0 }} />
        <span style={{ color: '#8b949e', fontSize: 13 }}>{label}</span>
      </div>
      <span style={{ color: '#e6edf3', fontSize: 13, fontFamily: 'monospace' }}>
        {value}
        {unit && <span style={{ color: '#8b949e', marginLeft: 3, fontSize: 11 }}>{unit}</span>}
      </span>
    </div>
  );
}

function BatteryBar({ voltage }) {
  const pct = Math.max(0, Math.min(1, (voltage - 3.3) / (4.2 - 3.3)));
  const color = pct > 0.5 ? '#3fb950' : pct > 0.2 ? '#e3b341' : '#f85149';
  return (
    <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', padding: '9px 0' }}>
      <div style={{ display: 'flex', alignItems: 'center', gap: 10 }}>
        <div style={{ width: 8, height: 8, borderRadius: '50%', background: color, flexShrink: 0 }} />
        <span style={{ color: '#8b949e', fontSize: 13 }}>Battery</span>
      </div>
      <div style={{ display: 'flex', alignItems: 'center', gap: 8 }}>
        <div style={{ width: 80, height: 8, background: '#21262d', borderRadius: 4, overflow: 'hidden' }}>
          <div style={{ width: `${pct * 100}%`, height: '100%', background: color, borderRadius: 4 }} />
        </div>
        <span style={{ color: '#e6edf3', fontSize: 13, fontFamily: 'monospace', minWidth: 50, textAlign: 'right' }}>
          {voltage?.toFixed(2)} <span style={{ color: '#8b949e', fontSize: 11 }}>V</span>
        </span>
      </div>
    </div>
  );
}

export default function HealthGrid({ latest }) {
  if (!latest) {
    return (
      <div style={{ background: '#161b22', border: '1px solid #30363d', borderRadius: 8, padding: '16px 20px' }}>
        <div style={{ color: '#8b949e', fontSize: 12, letterSpacing: 1, marginBottom: 12 }}>SYSTEM HEALTH</div>
        <div style={{ color: '#30363d', textAlign: 'center', padding: 20, fontSize: 13 }}>Awaiting telemetry...</div>
      </div>
    );
  }

  const gpsStatus = latest.gps_sats >= 6 ? 'ok' : latest.gps_sats >= 3 ? 'warn' : 'error';
  const recDrogue = latest.rec_drogue === 1;
  const recMain = latest.rec_main === 1;
  const phase = latest.phase ?? 'pad';

  let recoveryStatus = 'ok';
  let recoveryLabel = 'armed';
  if (recDrogue && recMain) { recoveryStatus = 'ok'; recoveryLabel = 'main deployed'; }
  else if (recDrogue) { recoveryStatus = 'warn'; recoveryLabel = 'drogue deployed'; }
  else if (['drogue', 'main', 'landed'].includes(phase)) { recoveryStatus = 'error'; recoveryLabel = 'not deployed'; }
  else { recoveryStatus = 'warn'; recoveryLabel = 'armed'; }

  return (
    <div style={{ background: '#161b22', border: '1px solid #30363d', borderRadius: 8, padding: '16px 20px' }}>
      <div style={{ color: '#8b949e', fontSize: 12, letterSpacing: 1, marginBottom: 4 }}>SYSTEM HEALTH</div>
      <HealthRow label="GPS lock" value={`${latest.gps_sats} sats`} status={gpsStatus} />
      <HealthRow label="IMU" value="200" unit="Hz" status="ok" />
      <HealthRow label="Barometer" value="50" unit="Hz" status="ok" />
      <HealthRow label="Temp" value={latest.temp?.toFixed(1)} unit="°C" status={latest.temp > 60 ? 'warn' : 'ok'} />
      <HealthRow label="HDOP" value={latest.hdop?.toFixed(1)} status={latest.hdop < 1.5 ? 'ok' : 'warn'} />
      <HealthRow label="Recovery" value={recoveryLabel} status={recoveryStatus} />
      <BatteryBar voltage={latest.bat} />
    </div>
  );
}

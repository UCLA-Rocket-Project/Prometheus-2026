import React, { useMemo } from 'react';
import QuatCanvas from '../components/QuatCanvas';

function AngleBar({ label, value, color, range = 180 }) {
  const pct = Math.abs(value) / range;
  const sign = value >= 0 ? '+' : '-';
  return (
    <div style={{ marginBottom: 12 }}>
      <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: 4 }}>
        <span style={{ color: '#8b949e', fontSize: 12 }}>{label}</span>
        <span style={{ color, fontSize: 13, fontFamily: 'monospace' }}>
          {sign}{Math.abs(value).toFixed(1)}°
        </span>
      </div>
      <div style={{ height: 6, background: '#21262d', borderRadius: 3, overflow: 'hidden', position: 'relative' }}>
        <div
          style={{
            position: 'absolute',
            left: value >= 0 ? '50%' : `${(0.5 - pct * 0.5) * 100}%`,
            width: `${pct * 50}%`,
            height: '100%',
            background: color,
            borderRadius: 3,
          }}
        />
        <div style={{ position: 'absolute', left: '50%', top: 0, width: 1, height: '100%', background: '#30363d' }} />
      </div>
    </div>
  );
}

function QuatRow({ label, value, color }) {
  return (
    <div
      style={{
        display: 'flex',
        justifyContent: 'space-between',
        alignItems: 'center',
        padding: '6px 0',
        borderBottom: '1px solid #21262d',
      }}
    >
      <span style={{ color: '#8b949e', fontSize: 13 }}>{label}</span>
      <span
        style={{
          color,
          fontSize: 16,
          fontFamily: 'monospace',
          fontWeight: 600,
          background: `${color}18`,
          padding: '2px 10px',
          borderRadius: 4,
          minWidth: 90,
          textAlign: 'right',
        }}
      >
        {value >= 0 ? '+' : ''}{value.toFixed(4)}
      </span>
    </div>
  );
}

function RateRow({ label, value }) {
  const abs = Math.abs(value);
  const color = abs > 50 ? '#f85149' : abs > 20 ? '#e3b341' : '#58a6ff';
  return (
    <div style={{ display: 'flex', justifyContent: 'space-between', padding: '5px 0', borderBottom: '1px solid #21262d' }}>
      <span style={{ color: '#8b949e', fontSize: 12 }}>{label}</span>
      <span style={{ color, fontSize: 13, fontFamily: 'monospace' }}>
        {value >= 0 ? '+' : ''}{value.toFixed(2)} °/s
      </span>
    </div>
  );
}

export default function Attitude({ latest }) {
  const qw = latest?.qw ?? 1;
  const qx = latest?.qx ?? 0;
  const qy = latest?.qy ?? 0;
  const qz = latest?.qz ?? 0;

  const norm = Math.sqrt(qw * qw + qx * qx + qy * qy + qz * qz);

  const axisAngle = useMemo(() => {
    const angle = 2 * Math.acos(Math.min(1, Math.abs(qw))) * (180 / Math.PI);
    const s = Math.sqrt(Math.max(0, 1 - qw * qw));
    if (s < 0.001) return { axis: [1, 0, 0], angle: 0 };
    return { axis: [qx / s, qy / s, qz / s], angle };
  }, [qw, qx, qy, qz]);

  const pitch = latest?.pitch ?? 0;
  const gimbalLock = Math.abs(pitch) > 85;

  return (
    <div style={{ height: '100%', display: 'flex', gap: 16, padding: 20, overflow: 'hidden' }}>
      <div
        style={{
          flex: 2,
          background: '#161b22',
          border: '1px solid #30363d',
          borderRadius: 8,
          overflow: 'hidden',
        }}
      >
        <QuatCanvas qw={qw} qx={qx} qy={qy} qz={qz} />
      </div>

      <div style={{ flex: 1, display: 'flex', flexDirection: 'column', gap: 12, overflowY: 'auto', minWidth: 260 }}>
        {gimbalLock && (
          <div
            style={{
              background: '#3d0000',
              border: '1px solid #f85149',
              borderRadius: 6,
              padding: '8px 14px',
              color: '#f85149',
              fontSize: 12,
              letterSpacing: 0.5,
              display: 'flex',
              alignItems: 'center',
              gap: 8,
            }}
          >
            <span style={{ fontSize: 16 }}>⚠</span> GIMBAL LOCK — |pitch| &gt; 85°
          </div>
        )}

        <div style={{ background: '#161b22', border: '1px solid #30363d', borderRadius: 8, padding: '14px 18px' }}>
          <div style={{ color: '#8b949e', fontSize: 11, letterSpacing: 1, marginBottom: 10 }}>QUATERNION</div>
          <QuatRow label="w" value={qw} color="#e6edf3" />
          <QuatRow label="x" value={qx} color="#ff6666" />
          <QuatRow label="y" value={qy} color="#6bdd9a" />
          <QuatRow label="z" value={qz} color="#79c0ff" />
          <div style={{ display: 'flex', justifyContent: 'space-between', padding: '6px 0', marginTop: 4 }}>
            <span style={{ color: '#8b949e', fontSize: 12 }}>|q| norm</span>
            <span style={{
              fontSize: 13,
              fontFamily: 'monospace',
              color: Math.abs(norm - 1) < 0.01 ? '#3fb950' : '#f85149',
            }}>
              {norm.toFixed(6)}
            </span>
          </div>
        </div>

        <div style={{ background: '#161b22', border: '1px solid #30363d', borderRadius: 8, padding: '14px 18px' }}>
          <div style={{ color: '#8b949e', fontSize: 11, letterSpacing: 1, marginBottom: 10 }}>EULER ANGLES</div>
          <AngleBar label="Roll" value={latest?.roll ?? 0} color="#ff6666" />
          <AngleBar label="Pitch" value={latest?.pitch ?? 0} color="#6bdd9a" />
          <AngleBar label="Yaw" value={latest?.yaw ?? 0} color="#79c0ff" />
        </div>

        <div style={{ background: '#161b22', border: '1px solid #30363d', borderRadius: 8, padding: '14px 18px' }}>
          <div style={{ color: '#8b949e', fontSize: 11, letterSpacing: 1, marginBottom: 8 }}>ANGULAR RATES</div>
          <RateRow label="ω_x (roll)" value={latest?.gyr_x ?? 0} />
          <RateRow label="ω_y (pitch)" value={latest?.gyr_y ?? 0} />
          <RateRow label="ω_z (yaw)" value={latest?.gyr_z ?? 0} />
        </div>

        <div style={{ background: '#161b22', border: '1px solid #30363d', borderRadius: 8, padding: '14px 18px' }}>
          <div style={{ color: '#8b949e', fontSize: 11, letterSpacing: 1, marginBottom: 8 }}>AXIS-ANGLE</div>
          <div style={{ display: 'flex', justifyContent: 'space-between', padding: '5px 0', borderBottom: '1px solid #21262d' }}>
            <span style={{ color: '#8b949e', fontSize: 12 }}>Axis</span>
            <span style={{ color: '#e6edf3', fontSize: 12, fontFamily: 'monospace' }}>
              [{axisAngle.axis.map((v) => v.toFixed(3)).join(', ')}]
            </span>
          </div>
          <div style={{ display: 'flex', justifyContent: 'space-between', padding: '5px 0' }}>
            <span style={{ color: '#8b949e', fontSize: 12 }}>Angle</span>
            <span style={{ color: '#e3b341', fontSize: 13, fontFamily: 'monospace' }}>
              {axisAngle.angle.toFixed(2)}°
            </span>
          </div>
        </div>
      </div>
    </div>
  );
}

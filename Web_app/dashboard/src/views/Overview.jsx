import React, { useEffect, useRef, useMemo } from 'react';
import {
  Chart as ChartJS,
  CategoryScale,
  LinearScale,
  PointElement,
  LineElement,
  Title,
  Tooltip,
  Legend,
  Filler,
  LineController,
} from 'chart.js';
import HealthGrid from '../components/HealthGrid';

ChartJS.register(
  CategoryScale, LinearScale, PointElement, LineElement,
  Title, Tooltip, Legend, Filler, LineController
);

function StatCard({ label, value, unit, sub, subColor }) {
  return (
    <div
      style={{
        background: '#161b22',
        border: '1px solid #30363d',
        borderRadius: 8,
        padding: '16px 20px',
        flex: 1,
        minWidth: 0,
      }}
    >
      <div style={{ color: '#8b949e', fontSize: 12, letterSpacing: 1, marginBottom: 8 }}>{label}</div>
      <div style={{ display: 'flex', alignItems: 'baseline', gap: 4 }}>
        <span style={{ color: '#e6edf3', fontSize: 32, fontWeight: 700, fontFamily: 'monospace' }}>
          {value}
        </span>
        <span style={{ color: '#8b949e', fontSize: 13 }}>{unit}</span>
      </div>
      {sub && (
        <div style={{ color: subColor || '#8b949e', fontSize: 12, marginTop: 4, fontFamily: 'monospace' }}>
          {sub}
        </div>
      )}
    </div>
  );
}

function computeBallisticPrediction(history) {
  if (history.length < 2) return [];
  const last = history[history.length - 1];
  if (last.alt <= 0) return [];
  const g = 9.81;
  const v0 = last.vel;
  const h0 = last.alt;
  const t0 = last.t;
  const points = [];
  for (let dt = 0; dt <= 60; dt += 0.5) {
    const h = h0 + v0 * dt - 0.5 * g * dt * dt;
    if (h < 0) break;
    points.push({ t: parseFloat((t0 + dt).toFixed(1)), alt: parseFloat(h.toFixed(1)) });
  }
  return points;
}

function DescentRateStatus({ latest, history }) {
  const phase = latest?.phase ?? 'pad';
  const phaseOrder = ['pad', 'powered', 'coasting', 'apogee', 'drogue', 'main', 'landed'];
  const pi = phaseOrder.indexOf(phase);

  const launchActive = pi >= 1;
  const apogeeActive = pi >= 3;

  const maxAltFt = latest?.alt_max ?? 0;

  // Light up drogue once actual descent rate hits 75 ft/s
  const drogueActive = useMemo(
    () => history.some((p) => p.vel < 0 && Math.abs(p.vel) >= 65),
    [history]
  );

  // Light up main once descent rate has slowed to ~24.5 ft/s
  const mainActive = useMemo(
    () => history.some((p) => p.vel < 0 && Math.abs(p.vel) <= 35 && Math.abs(p.vel) >= 10),
    [history]
  );

  const Row = ({ label, active, color, bigValue, bigUnit, tag }) => (
    <div style={{
      display: 'flex',
      alignItems: 'center',
      justifyContent: 'space-between',
      padding: '10px 14px',
      borderRadius: 6,
      background: active ? `${color}18` : 'transparent',
      border: `1px solid ${active ? `${color}55` : '#21262d'}`,
      marginBottom: 8,
    }}>
      <div>
        <div style={{ color: active ? color : '#8b949e', fontSize: 11, letterSpacing: 1 }}>{label}</div>
        {bigValue != null && (
          <div style={{ color: active ? '#e6edf3' : '#8b949e', fontSize: 26, fontFamily: 'monospace', fontWeight: 700, marginTop: 2 }}>
            {bigValue} <span style={{ fontSize: 13, color: '#8b949e' }}>{bigUnit}</span>
          </div>
        )}
      </div>
      {active && (
        <div style={{ color, fontSize: 11, fontFamily: 'monospace', display: 'flex', alignItems: 'center', gap: 5 }}>
          <span style={{ width: 7, height: 7, borderRadius: '50%', background: color, display: 'inline-block' }} />
          {tag}
        </div>
      )}
    </div>
  );

  return (
    <div style={{ background: '#161b22', border: '1px solid #30363d', borderRadius: 8, padding: '16px 20px' }}>
      <div style={{ color: '#8b949e', fontSize: 12, letterSpacing: 1, marginBottom: 12 }}>FLIGHT STATUS</div>
      <Row label="LAUNCH DETECTED" active={launchActive} color="#f0883e" tag="DETECTED" />
      <Row label="APOGEE" active={apogeeActive} color="#bc8cff" bigValue={apogeeActive ? maxAltFt.toFixed(0) : null} bigUnit="ft" tag="REACHED" />
      <Row label="DROGUE" active={drogueActive} color="#e3b341" bigValue="75.0" bigUnit="ft/s" tag="ACTIVE" />
      <Row label="MAIN" active={mainActive} color="#3fb950" bigValue="24.5" bigUnit="ft/s" tag="ACTIVE" />
    </div>
  );
}

export default function Overview({ latest, history }) {
  const chartRef = useRef(null);
  const chartInstanceRef = useRef(null);
  const markerRef = useRef({ apogeeIdx: -1, nowIdx: -1 });
  const startWallRef = useRef(null);
  const startDeviceRef = useRef(null);

  const apogeePoint = useMemo(() => {
    let max = null;
    for (const p of history) {
      if (!max || p.alt > max.alt) max = p;
    }
    return max;
  }, [history]);

  const eventLog = useMemo(() => {
    const events = [];
    for (let i = 1; i < history.length; i++) {
      const prev = history[i - 1];
      const cur = history[i];
      if (prev.phase !== cur.phase) {
        const fmt = (t) => {
          const m = Math.floor(t / 60).toString().padStart(2, '0');
          const s = Math.floor(t % 60).toString().padStart(2, '0');
          return `T+${m}:${s}`;
        };
        const labels = {
          powered:  { text: 'Launch detected — liftoff', color: '#f0883e' },
          coasting: { text: 'Coasting — motor burnout confirmed', color: '#58a6ff' },
          apogee:   { text: 'Apogee detected', color: '#bc8cff' },
          drogue:   { text: 'Drogue deploy commanded', color: '#e3b341' },
          main:     { text: 'Main deploy commanded', color: '#3fb950' },
          landed:   { text: 'Touchdown detected', color: '#3fb950' },
        };
        if (labels[cur.phase]) {
          events.unshift({ time: fmt(cur.t), ...labels[cur.phase] });
        }
      }
    }
    return events;
  }, [history]);

  // Create chart once on mount
  useEffect(() => {
    if (!chartRef.current) return;

    const verticalLinesPlugin = {
      id: 'verticalLines',
      afterDraw(chart) {
        const { apogeeIdx, nowIdx } = markerRef.current;
        if (apogeeIdx < 0 && nowIdx < 0) return;
        const ctx = chart.ctx;
        const xScale = chart.scales.x;
        const yScale = chart.scales.y;
        if (!xScale || !yScale) return;
        const top = yScale.top;
        const bottom = yScale.bottom;
        const numLabels = chart.data.labels.length;
        const idxToPx = (idx) =>
          numLabels <= 1
            ? xScale.getPixelForDecimal(0.5)
            : xScale.getPixelForDecimal(idx / (numLabels - 1));

        if (apogeeIdx >= 0) {
          const x = idxToPx(apogeeIdx);
          ctx.save();
          ctx.setLineDash([4, 4]);
          ctx.strokeStyle = '#bc8cff88';
          ctx.lineWidth = 1;
          ctx.beginPath();
          ctx.moveTo(x, top);
          ctx.lineTo(x, bottom);
          ctx.stroke();
          ctx.fillStyle = '#bc8cff';
          ctx.font = '10px monospace';
          ctx.fillText('apogee', x + 4, top + 12);
          ctx.restore();
        }

        if (nowIdx >= 0) {
          const x = idxToPx(nowIdx);
          ctx.save();
          ctx.setLineDash([4, 4]);
          ctx.strokeStyle = '#f85149aa';
          ctx.lineWidth = 1.5;
          ctx.beginPath();
          ctx.moveTo(x, top);
          ctx.lineTo(x, bottom);
          ctx.stroke();
          ctx.fillStyle = '#f85149';
          ctx.font = '10px monospace';
          ctx.fillText('now', x + 4, top + 24);
          ctx.restore();
        }
      },
    };

    const instance = new ChartJS(chartRef.current, {
      type: 'line',
      plugins: [verticalLinesPlugin],
      data: {
        labels: [],
        datasets: [
          {
            label: 'Altitude (m)',
            data: [],
            borderColor: '#58a6ff',
            backgroundColor: '#58a6ff18',
            fill: true,
            pointRadius: 0,
            borderWidth: 2,
            tension: 0.3,
            spanGaps: true,
          },
          {
            label: 'Ballistic pred.',
            data: [],
            borderColor: '#e3b341',
            borderDash: [5, 4],
            pointRadius: 0,
            borderWidth: 1.5,
            tension: 0.3,
            spanGaps: true,
            fill: false,
          },
        ],
      },
      options: {
        animation: false,
        maintainAspectRatio: false,
        responsive: true,
        plugins: {
          legend: {
            labels: { color: '#8b949e', font: { family: 'monospace', size: 11 }, boxWidth: 16 },
          },
          tooltip: {
            backgroundColor: '#161b22',
            borderColor: '#30363d',
            borderWidth: 1,
            titleColor: '#e6edf3',
            bodyColor: '#8b949e',
          },
        },
        scales: {
          x: {
            ticks: {
              color: '#8b949e',
              font: { family: 'monospace', size: 10 },
              maxTicksLimit: 10,
              maxRotation: 0,
            },
            grid: { color: '#21262d' },
            title: { display: true, text: 'Time (s)', color: '#8b949e', font: { size: 10 } },
          },
          y: {
            ticks: { color: '#8b949e', font: { family: 'monospace', size: 10 } },
            grid: { color: '#21262d' },
            title: { display: true, text: 'ft (above ground level)', color: '#8b949e', font: { size: 10 } },
          },
        },
      },
    });

    chartInstanceRef.current = instance;

    return () => {
      instance.destroy();
      chartInstanceRef.current = null;
    };
  }, []);

  // Update chart data when history changes (no destroy/recreate)
  useEffect(() => {
    const chart = chartInstanceRef.current;
    if (!chart || history.length === 0) return;

    if (startWallRef.current === null) {
      startWallRef.current = Date.now();
      startDeviceRef.current = history[0].t;
    }
    const wallStart = startWallRef.current;
    const deviceStart = startDeviceRef.current;

    const labels = history.map((p) => (p.t - deviceStart).toFixed(1));
    const pred = computeBallisticPrediction(history);
    const predLabels = pred.map((p) => (p.t - deviceStart).toFixed(1));

    const allLabels = Array.from(new Set([...labels, ...predLabels])).sort((a, b) => +a - +b);

    const altMap = {};
    history.forEach((p) => { altMap[(p.t - deviceStart).toFixed(1)] = p.alt; });
    const predMap = {};
    pred.forEach((p) => { predMap[(p.t - deviceStart).toFixed(1)] = p.alt; });

    const actualAligned = allLabels.map((l) => altMap[l] ?? null);
    const predAligned = allLabels.map((l) => predMap[l] ?? null);

    markerRef.current = {
      apogeeIdx: allLabels.findIndex((l) => apogeePoint && l === (apogeePoint.t - deviceStart).toFixed(1)),
      nowIdx: allLabels.findIndex((l) => latest && l === (latest.t - deviceStart).toFixed(1)),
    };

    const toHMS = (elapsedSec) => {
      const d = new Date(wallStart + +elapsedSec * 1000);
      return `${d.getHours().toString().padStart(2, '0')}:${d.getMinutes().toString().padStart(2, '0')}:${d.getSeconds().toString().padStart(2, '0')}`;
    };
    chart.data.labels = allLabels.map(toHMS);
    chart.data.datasets[0].data = actualAligned;
    chart.data.datasets[1].data = predAligned;
    chart.update('none');
  }, [history, apogeePoint, latest]);

  const maxG = latest?.accel_max ?? 0;
  const rssi = latest?.rssi ?? 0;
  const rssiColor = rssi > -90 ? '#3fb950' : rssi > -105 ? '#e3b341' : '#f85149';
  const velColor = (latest?.vel ?? 0) >= 0 ? '#3fb950' : '#f85149';

  return (
    <div style={{ height: '100%', display: 'flex', flexDirection: 'column', gap: 16, overflowY: 'auto', padding: 20 }}>
      <div style={{ display: 'flex', gap: 12 }}>
        <StatCard
          label="ALTITUDE"
          value={(latest?.alt ?? 0).toFixed(1)}
          unit="ft (above ground level)"
          sub={`${(latest?.vel ?? 0) >= 0 ? '+' : ''}${(latest?.vel ?? 0).toFixed(1)} ft/s`}
          subColor={(latest?.vel ?? 0) >= 0 ? '#3fb950' : '#f85149'}
        />
        <StatCard
          label="VELOCITY"
          value={Math.abs(latest?.vel ?? 0).toFixed(1)}
          unit="ft/s"
          sub={(latest?.vel ?? 0) >= 0 ? '▲ ascending' : '▼ Descending'}
          subColor={velColor}
        />
        <StatCard
          label="MAX G"
          value={maxG.toFixed(2)}
          unit="G"
          sub="Peak"
          subColor="#8b949e"
        />
        <StatCard
          label="RSSI"
          value={rssi}
          unit="dBm"
          //sub="good link"
          subColor={rssiColor}
        />
      </div>

      <div style={{ display: 'flex', gap: 16, flex: 1, minHeight: 0 }}>
        <div style={{ flex: 2, display: 'flex', flexDirection: 'column', gap: 16, minWidth: 0 }}>
          <div
            style={{
              background: '#161b22',
              border: '1px solid #30363d',
              borderRadius: 8,
              padding: '16px 20px',
              flex: 1,
              minHeight: 220,
            }}
          >
            <div style={{ color: '#8b949e', fontSize: 12, letterSpacing: 1, marginBottom: 12 }}>ALTITUDE PROFILE</div>
            <div style={{ height: 'calc(100% - 28px)', position: 'relative' }}>
              <canvas ref={chartRef} />
            </div>
          </div>

          <div
            style={{
              background: '#161b22',
              border: '1px solid #30363d',
              borderRadius: 8,
              padding: '16px 20px',
              maxHeight: 200,
              overflowY: 'auto',
            }}
          >
            <div style={{ color: '#8b949e', fontSize: 12, letterSpacing: 1, marginBottom: 12 }}>EVENT LOG</div>
            {eventLog.length === 0 ? (
              <div style={{ color: '#30363d', fontSize: 12 }}>Awaiting events...</div>
            ) : (
              eventLog.map((e, i) => (
                <div
                  key={i}
                  style={{
                    display: 'flex',
                    alignItems: 'center',
                    gap: 16,
                    padding: '7px 0',
                    borderBottom: i < eventLog.length - 1 ? '1px solid #21262d' : 'none',
                  }}
                >
                  <span style={{ color: '#8b949e', fontSize: 12, minWidth: 60, fontFamily: 'monospace' }}>{e.time}</span>
                  <span style={{ color: e.color, fontSize: 13 }}>{e.text}</span>
                </div>
              ))
            )}
          </div>
        </div>

        <div style={{ flex: 1, minWidth: 220, display: 'flex', flexDirection: 'column', gap: 16 }}>
          <HealthGrid latest={latest} />
          <DescentRateStatus latest={latest} history={history} />
        </div>
      </div>
    </div>
  );
}

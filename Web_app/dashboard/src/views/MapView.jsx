import React, { useEffect, useRef, useMemo } from 'react';
import L from 'leaflet';

delete L.Icon.Default.prototype._getIconUrl;
L.Icon.Default.mergeOptions({
  iconRetinaUrl: 'https://unpkg.com/leaflet@1.9.4/dist/images/marker-icon-2x.png',
  iconUrl: 'https://unpkg.com/leaflet@1.9.4/dist/images/marker-icon.png',
  shadowUrl: 'https://unpkg.com/leaflet@1.9.4/dist/images/marker-shadow.png',
});

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

function predictLanding(lat, lon, alt, vel) {
  const timeToGround = vel < 0 ? Math.abs(alt / vel) : alt / 15;
  const driftRate = 0.0001;
  return {
    lat: lat + driftRate * timeToGround * 0.3,
    lon: lon - driftRate * timeToGround * 0.15,
    uncertainty: Math.max(10, alt * 0.01 + 20),
  };
}

function CompassRose({ bearing, label }) {
  const r = 45;
  const cx = 55, cy = 55;
  const arrowRad = (bearing - 90) * Math.PI / 180;
  const ax = cx + r * 0.7 * Math.cos(arrowRad);
  const ay = cy + r * 0.7 * Math.sin(arrowRad);

  return (
    <svg width={110} height={110} style={{ display: 'block' }}>
      <circle cx={cx} cy={cy} r={r} fill="#0d1117" stroke="#30363d" strokeWidth={1} />
      {['N', 'E', 'S', 'W'].map((dir, i) => {
        const rad = (i * 90 - 90) * Math.PI / 180;
        return (
          <text
            key={dir}
            x={cx + (r - 8) * Math.cos(rad)}
            y={cy + (r - 8) * Math.sin(rad) + 4}
            fill="#8b949e"
            fontSize={9}
            textAnchor="middle"
            fontFamily="monospace"
          >
            {dir}
          </text>
        );
      })}
      <line
        x1={cx} y1={cy}
        x2={ax} y2={ay}
        stroke="#58a6ff"
        strokeWidth={2}
        strokeLinecap="round"
      />
      <circle cx={ax} cy={ay} r={3} fill="#58a6ff" />
      <circle cx={cx} cy={cy} r={3} fill="#e6edf3" />
      <text x={cx} y={cy + r + 14} fill="#8b949e" fontSize={9} textAnchor="middle" fontFamily="monospace">
        {label}
      </text>
    </svg>
  );
}

export default function MapView({ latest, history, padCoords }) {
  const mapContainerRef = useRef(null);
  const mapRef = useRef(null);
  const layersRef = useRef({});

  const padLat = padCoords?.lat ?? 34.8742;
  const padLon = padCoords?.lon ?? -117.3108;

  useEffect(() => {
    if (!mapContainerRef.current || mapRef.current) return;

    const map = L.map(mapContainerRef.current, {
      center: [padLat, padLon],
      zoom: 16,
      zoomControl: true,
      attributionControl: false,
    });

    L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
      attribution: '© OpenStreetMap',
      className: 'map-tiles',
    }).addTo(map);

    const padIcon = L.divIcon({
      className: '',
      html: `<div style="width:14px;height:14px;border-radius:50%;background:#f85149;border:2px solid #ff8080;box-shadow:0 0 6px #f8514988;"></div>`,
      iconAnchor: [7, 7],
    });
    layersRef.current.padMarker = L.marker([padLat, padLon], { icon: padIcon }).addTo(map);

    const rocketIcon = L.divIcon({
      className: 'pulse-dot',
      html: `<div style="width:12px;height:12px;border-radius:50%;background:#3fb950;border:2px solid #6bdd9a;position:relative;box-shadow:0 0 8px #3fb95088;"></div>`,
      iconAnchor: [6, 6],
    });
    layersRef.current.rocketMarker = L.marker([padLat, padLon], { icon: rocketIcon }).addTo(map);

    layersRef.current.ascentTrail = L.polyline([], {
      color: '#58a6ff', weight: 2.5, opacity: 0.85,
    }).addTo(map);

    layersRef.current.descentPred = L.polyline([], {
      color: '#e3b341', weight: 1.5, opacity: 0.7, dashArray: '6 5',
    }).addTo(map);

    layersRef.current.landingZone = L.circle([padLat, padLon], {
      radius: 0, color: '#bc8cff', fillColor: '#bc8cff', fillOpacity: 0.08, weight: 1.5,
    }).addTo(map);

    mapRef.current = map;

    return () => {
      map.remove();
      mapRef.current = null;
    };
  }, []);

  useEffect(() => {
    const map = mapRef.current;
    if (!map || !latest) return;

    const lat = latest.lat ?? padLat;
    const lon = latest.lon ?? padLon;
    const alt = latest.alt ?? 0;
    const vel = latest.vel ?? -15;

    layersRef.current.rocketMarker?.setLatLng([lat, lon]);

    const trail = history
      .filter((p) => p.lat && p.lon)
      .map((p) => [p.lat, p.lon]);
    layersRef.current.ascentTrail?.setLatLngs(trail);

    const pred = predictLanding(lat, lon, alt, vel);
    layersRef.current.descentPred?.setLatLngs([
      [lat, lon],
      [pred.lat, pred.lon],
    ]);

    layersRef.current.landingZone?.setLatLng([pred.lat, pred.lon]);
    layersRef.current.landingZone?.setRadius(pred.uncertainty);

    if (latest.phase === 'landed' && !layersRef.current.landingMarker) {
      const starIcon = L.divIcon({
        className: '',
        html: `<div style="width:16px;height:16px;background:#bc8cff;clip-path:polygon(50% 0%,61% 35%,98% 35%,68% 57%,79% 91%,50% 70%,21% 91%,32% 57%,2% 35%,39% 35%);"></div>`,
        iconAnchor: [8, 8],
      });
      layersRef.current.landingMarker = L.marker([lat, lon], { icon: starIcon }).addTo(map);
      layersRef.current.driftLine = L.polyline([[padLat, padLon], [lat, lon]], {
        color: '#bc8cff', dashArray: '4 3', weight: 1.5,
      }).addTo(map);
    }
  }, [latest, history]);

  const stats = useMemo(() => {
    if (!latest) return null;
    const lat = latest.lat ?? padLat;
    const lon = latest.lon ?? padLon;
    const dist = latLonDistance(padLat, padLon, lat, lon);
    const bearing = bearingDeg(padLat, padLon, lat, lon);
    const driftN = (lat - padLat) * 111320;
    const driftE = (lon - padLon) * 111320 * Math.cos(padLat * Math.PI / 180);
    const pred = predictLanding(lat, lon, latest.alt ?? 0, latest.vel ?? -15);
    const predDist = latLonDistance(padLat, padLon, pred.lat, pred.lon);
    return { lat, lon, dist, bearing, driftN, driftE, predDist, predUncert: pred.uncertainty };
  }, [latest, padLat, padLon]);

  const bearing = stats?.bearing ?? 0;

  return (
    <div style={{ height: '100%', display: 'flex', gap: 0, overflow: 'hidden' }}>
      <div ref={mapContainerRef} style={{ flex: 1, minWidth: 0 }} />

      <div
        style={{
          width: 240,
          background: '#161b22',
          borderLeft: '1px solid #30363d',
          padding: '16px 14px',
          overflowY: 'auto',
          display: 'flex',
          flexDirection: 'column',
          gap: 14,
          flexShrink: 0,
        }}
      >
        <div>
          <div style={{ color: '#8b949e', fontSize: 11, letterSpacing: 1, marginBottom: 8 }}>CURRENT POSITION</div>
          {[
            { label: 'Lat', value: `${(latest?.lat ?? padLat).toFixed(4)}° N` },
            { label: 'Lon', value: `${Math.abs(latest?.lon ?? padLon).toFixed(4)}° W` },
            { label: 'Alt (GPS)', value: latest?.gps_alt != null ? `${latest.gps_alt.toFixed(0)} m` : '— m' },
          ].map((r) => (
            <div key={r.label} style={{ display: 'flex', justifyContent: 'space-between', padding: '5px 0', borderBottom: '1px solid #21262d' }}>
              <span style={{ color: '#8b949e', fontSize: 12 }}>{r.label}</span>
              <span style={{ color: '#e6edf3', fontSize: 13, fontFamily: 'monospace' }}>{r.value}</span>
            </div>
          ))}
        </div>

        <div>
          <div style={{ color: '#8b949e', fontSize: 11, letterSpacing: 1, marginBottom: 8 }}>DRIFT FROM PAD</div>
          <div style={{ display: 'flex', gap: 12 }}>
            <div>
              <div style={{ color: '#8b949e', fontSize: 10, marginBottom: 2 }}>N/S</div>
              <span style={{ color: (stats?.driftN ?? 0) >= 0 ? '#3fb950' : '#f85149', fontSize: 14, fontFamily: 'monospace' }}>
                {(stats?.driftN ?? 0) >= 0 ? '+' : ''}{(stats?.driftN ?? 0).toFixed(0)} m
              </span>
            </div>
            <div>
              <div style={{ color: '#8b949e', fontSize: 10, marginBottom: 2 }}>E/W</div>
              <span style={{ color: (stats?.driftE ?? 0) >= 0 ? '#3fb950' : '#f85149', fontSize: 14, fontFamily: 'monospace' }}>
                {(stats?.driftE ?? 0) >= 0 ? '+' : ''}{(stats?.driftE ?? 0).toFixed(0)} m
              </span>
            </div>
          </div>
        </div>

        <div>
          <div style={{ color: '#8b949e', fontSize: 11, letterSpacing: 1, marginBottom: 6 }}>PREDICTED LANDING</div>
          <div style={{ color: '#bc8cff', fontSize: 20, fontFamily: 'monospace', fontWeight: 700, marginBottom: 4 }}>
            {(stats?.predDist ?? 0).toFixed(0)} m from pad
          </div>
          <div style={{ color: '#8b949e', fontSize: 11 }}>Wind-corrected est.</div>
          <div style={{ color: '#8b949e', fontSize: 11 }}>r = {(stats?.predUncert ?? 20).toFixed(0)} m uncertainty</div>
        </div>

        <div>
          <div style={{ color: '#8b949e', fontSize: 11, letterSpacing: 1, marginBottom: 6 }}>RECOVERY STATUS</div>
          {[
            { label: 'Drogue', val: latest?.rec_drogue },
            { label: 'Main', val: latest?.rec_main },
          ].map((r) => {
            const deployed = r.val === 1;
            const color = deployed ? '#3fb950' : '#e3b341';
            const text = deployed ? 'deployed' : 'armed';
            return (
              <div key={r.label} style={{ display: 'flex', alignItems: 'center', gap: 8, padding: '4px 0' }}>
                <div style={{ width: 8, height: 8, borderRadius: '50%', background: color }} />
                <span style={{ color: '#8b949e', fontSize: 12 }}>{r.label}</span>
                <span style={{ color, fontSize: 12, marginLeft: 'auto' }}>{text}</span>
              </div>
            );
          })}
        </div>

        <div>
          <div style={{ color: '#8b949e', fontSize: 11, letterSpacing: 1, marginBottom: 6 }}>BEARING TO ROCKET</div>
          <div style={{ display: 'flex', justifyContent: 'center' }}>
            <CompassRose bearing={bearing} label={`${bearing.toFixed(0)}°`} />
          </div>
          <div style={{ textAlign: 'center', color: '#8b949e', fontSize: 11, marginTop: 4 }}>
            Walk {(stats?.dist ?? 0).toFixed(0)}m bearing {bearing.toFixed(0)}°
          </div>
        </div>
      </div>
    </div>
  );
}

import { useState, useEffect, useRef, useCallback } from 'react';

const MAX_HISTORY = 500;
const PAD_LAT = 34.8742;
const PAD_LON = -117.3108;

export default function useTelemetry() {
  const [latest, setLatest] = useState(null);
  const [history, setHistory] = useState([]);
  const [connected, setConnected] = useState(false);
  const [flightTime, setFlightTime] = useState(0);
  const [padCoords, setPadCoords] = useState({ lat: PAD_LAT, lon: PAD_LON });

  const launchTimeRef = useRef(null);
  const padCoordsSetRef = useRef(false);
  const wsRef = useRef(null);

  const handlePacket = useCallback((pkt) => {
    if (!padCoordsSetRef.current && pkt.lat && pkt.lon) {
      setPadCoords({ lat: pkt.lat, lon: pkt.lon });
      padCoordsSetRef.current = true;
    }

    if (launchTimeRef.current === null && Math.abs(pkt.accel) > 2) {
      launchTimeRef.current = pkt.t;
    }

    if (launchTimeRef.current !== null) {
      setFlightTime(parseFloat((pkt.t - launchTimeRef.current).toFixed(2)));
    }

    setLatest(pkt);
    setHistory((prev) => {
      const next = [...prev, pkt];
      return next.length > MAX_HISTORY ? next.slice(next.length - MAX_HISTORY) : next;
    });
  }, []);

  useEffect(() => {
    let ws;
    let retryTimeout;

    function connect() {
      ws = new WebSocket(`ws://${window.location.hostname}:8765`);
      wsRef.current = ws;

      ws.onopen = () => setConnected(true);
      ws.onclose = () => {
        setConnected(false);
        retryTimeout = setTimeout(connect, 2000);
      };
      ws.onerror = () => ws.close();
      ws.onmessage = (evt) => {
        try {
          const pkt = JSON.parse(evt.data);
          handlePacket(pkt);
        } catch (_) {}
      };
    }

    connect();
    return () => {
      clearTimeout(retryTimeout);
      if (ws) ws.close();
    };
  }, [handlePacket]);

  return { latest, history, connected, flightTime, padCoords };
}

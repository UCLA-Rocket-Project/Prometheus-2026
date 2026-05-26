import { useState, useEffect, useRef, useMemo, useCallback } from 'react';

const MAX_HISTORY = 500;
const DEFAULT_PAD = { lat: 34.8742, lon: -117.3108 };

export default function useReplay(packets) {
  const [idxState, setIdxState] = useState(0);
  const [playing, setPlaying] = useState(false);
  const [speed, setSpeedState] = useState(1);

  const idxRef = useRef(0);
  const speedRef = useRef(1);
  const playingRef = useRef(false);
  const playStartWallRef = useRef(null);
  const playStartTRef = useRef(null);
  const rafRef = useRef(null);

  useEffect(() => { playingRef.current = playing; }, [playing]);

  // Reset everything when a new packet set is loaded
  useEffect(() => {
    idxRef.current = 0;
    setIdxState(0);
    setPlaying(false);
    setSpeedState(1);
    speedRef.current = 1;
    playStartWallRef.current = null;
    playStartTRef.current = null;
  }, [packets]);

  const padCoords = useMemo(() => {
    if (!packets?.length) return DEFAULT_PAD;
    const first = packets.find((p) => p.lat && p.lon);
    return first ? { lat: first.lat, lon: first.lon } : DEFAULT_PAD;
  }, [packets]);

  const launchT = useMemo(() => {
    if (!packets?.length) return null;
    const p = packets.find((p) => Math.abs(p.accel ?? 0) > 2);
    return p ? p.t : null;
  }, [packets]);

  // Median inter-packet interval in ms — reflects actual nose cone TX/RX rate
  const packetRateMs = useMemo(() => {
    if (!packets || packets.length < 2) return null;
    const intervals = [];
    const n = Math.min(packets.length, 501);
    for (let i = 1; i < n; i++) {
      const dt = (packets[i].t - packets[i - 1].t) * 1000;
      if (dt > 0 && dt < 10000) intervals.push(dt);
    }
    if (!intervals.length) return null;
    intervals.sort((a, b) => a - b);
    return +intervals[Math.floor(intervals.length / 2)].toFixed(1);
  }, [packets]);

  // RAF-driven playback — advances through packets respecting real t-delta × speed
  useEffect(() => {
    if (!playing || !packets?.length) return;

    playStartWallRef.current = Date.now();
    playStartTRef.current = packets[idxRef.current]?.t ?? 0;
    let running = true;

    function tick() {
      if (!running) return;
      const wallElapsedS = (Date.now() - playStartWallRef.current) / 1000;
      const targetT = playStartTRef.current + wallElapsedS * speedRef.current;

      let newIdx = idxRef.current;
      const pkts = packets;
      while (newIdx + 1 < pkts.length && pkts[newIdx + 1].t <= targetT) {
        newIdx++;
      }

      if (newIdx !== idxRef.current) {
        idxRef.current = newIdx;
        setIdxState(newIdx);
      }

      if (newIdx >= pkts.length - 1) {
        running = false;
        playingRef.current = false;
        setPlaying(false);
        return;
      }

      rafRef.current = requestAnimationFrame(tick);
    }

    rafRef.current = requestAnimationFrame(tick);
    return () => {
      running = false;
      if (rafRef.current) cancelAnimationFrame(rafRef.current);
    };
  }, [playing, packets]);

  const togglePlay = useCallback(() => {
    setPlaying((p) => !p);
  }, []);

  const setSpeed = useCallback((s) => {
    speedRef.current = s;
    setSpeedState(s);
    // Recalibrate so current position becomes the new time origin
    playStartWallRef.current = Date.now();
    playStartTRef.current = packets?.[idxRef.current]?.t ?? 0;
  }, [packets]);

  const seek = useCallback((newIdx) => {
    const clamped = Math.max(0, Math.min(newIdx, (packets?.length ?? 1) - 1));
    idxRef.current = clamped;
    setIdxState(clamped);
    playStartWallRef.current = Date.now();
    playStartTRef.current = packets?.[clamped]?.t ?? 0;
  }, [packets]);

  const restart = useCallback(() => {
    idxRef.current = 0;
    setIdxState(0);
    playStartWallRef.current = Date.now();
    playStartTRef.current = packets?.[0]?.t ?? 0;
  }, [packets]);

  const latest = packets?.[idxState] ?? null;

  const history = useMemo(() => {
    if (!packets?.length) return [];
    const end = idxState + 1;
    const start = Math.max(0, end - MAX_HISTORY);
    return packets.slice(start, end);
  }, [packets, idxState]);

  const flightTime = useMemo(() => {
    if (!latest || launchT === null) return 0;
    return parseFloat((latest.t - launchT).toFixed(2));
  }, [latest, launchT]);

  const totalDuration = packets?.length
    ? packets[packets.length - 1].t - packets[0].t
    : 0;

  const currentDuration = latest && packets?.length
    ? latest.t - packets[0].t
    : 0;

  return {
    latest,
    history,
    connected: true,
    flightTime,
    padCoords,
    playing,
    togglePlay,
    speed,
    setSpeed,
    seek,
    restart,
    currentIndex: idxState,
    totalPackets: packets?.length ?? 0,
    packetRateMs,
    totalDuration,
    currentDuration,
  };
}

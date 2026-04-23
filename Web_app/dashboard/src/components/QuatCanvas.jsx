import React, { useEffect, useRef } from 'react';
import * as THREE from 'three';

export default function QuatCanvas({ qw = 1, qx = 0, qy = 0, qz = 0 }) {
  const mountRef = useRef(null);
  const sceneRef = useRef(null);
  const quaternionRef = useRef(new THREE.Quaternion(0, 0, 0, 1));

  useEffect(() => {
    quaternionRef.current.set(qx, qy, qz, qw).normalize();
  }, [qw, qx, qy, qz]);

  useEffect(() => {
    const el = mountRef.current;
    if (!el) return;

    const renderer = new THREE.WebGLRenderer({ antialias: true, alpha: true });
    renderer.setPixelRatio(window.devicePixelRatio);
    renderer.setClearColor(0x0d1117, 1);
    renderer.setSize(el.clientWidth, el.clientHeight);
    el.appendChild(renderer.domElement);

    const scene = new THREE.Scene();
    const camera = new THREE.PerspectiveCamera(50, el.clientWidth / el.clientHeight, 0.1, 100);
    camera.position.set(3, 2, 4);
    camera.lookAt(0, 0, 0);

    const ambient = new THREE.AmbientLight(0xffffff, 0.4);
    scene.add(ambient);
    const dir = new THREE.DirectionalLight(0xffffff, 0.8);
    dir.position.set(5, 10, 5);
    scene.add(dir);

    const rocketGroup = new THREE.Group();

    const bodyMat = new THREE.MeshPhongMaterial({ color: 0x58a6ff, shininess: 60 });
    const noseMat = new THREE.MeshPhongMaterial({ color: 0xe6edf3, shininess: 80 });
    const finMat = new THREE.MeshPhongMaterial({ color: 0xf0883e, shininess: 40, side: THREE.DoubleSide });

    const body = new THREE.Mesh(new THREE.CylinderGeometry(0.18, 0.18, 1.6, 20), bodyMat);
    body.position.y = 0;
    rocketGroup.add(body);

    const nose = new THREE.Mesh(new THREE.ConeGeometry(0.18, 0.7, 20), noseMat);
    nose.position.y = 1.15;
    rocketGroup.add(nose);

    const finShape = new THREE.Shape();
    finShape.moveTo(0, 0);
    finShape.lineTo(-0.35, -0.5);
    finShape.lineTo(0, -0.45);
    finShape.closePath();

    const extrudeSettings = { depth: 0.025, bevelEnabled: false };
    const finGeo = new THREE.ExtrudeGeometry(finShape, extrudeSettings);

    for (let i = 0; i < 4; i++) {
      const fin = new THREE.Mesh(finGeo, finMat);
      fin.position.y = -0.6;
      fin.rotation.y = (i * Math.PI) / 2 + Math.PI / 2;
      fin.position.x = Math.sin((i * Math.PI) / 2) * 0.18;
      fin.position.z = Math.cos((i * Math.PI) / 2) * 0.18;
      rocketGroup.add(fin);
    }

    scene.add(rocketGroup);

    const worldAxesGroup = new THREE.Group();
    const dashed = new THREE.LineDashedMaterial({ color: 0xff4444, dashSize: 0.1, gapSize: 0.05, linewidth: 1 });
    const makeWorldAxis = (color, dir) => {
      const mat = new THREE.LineDashedMaterial({ color, dashSize: 0.12, gapSize: 0.06 });
      const geo = new THREE.BufferGeometry().setFromPoints([new THREE.Vector3(0, 0, 0), dir]);
      const line = new THREE.Line(geo, mat);
      line.computeLineDistances();
      return line;
    };
    worldAxesGroup.add(makeWorldAxis(0xff4444, new THREE.Vector3(1.8, 0, 0)));
    worldAxesGroup.add(makeWorldAxis(0x3fb950, new THREE.Vector3(0, 1.8, 0)));
    worldAxesGroup.add(makeWorldAxis(0x58a6ff, new THREE.Vector3(0, 0, 1.8)));
    scene.add(worldAxesGroup);

    const bodyAxesGroup = new THREE.Group();
    const makeBodyAxis = (color, dir) => {
      const mat = new THREE.LineBasicMaterial({ color, linewidth: 2 });
      const geo = new THREE.BufferGeometry().setFromPoints([new THREE.Vector3(0, 0, 0), dir]);
      return new THREE.Line(geo, mat);
    };
    const bx = makeBodyAxis(0xff6666, new THREE.Vector3(1.4, 0, 0));
    const by = makeBodyAxis(0x6bdd9a, new THREE.Vector3(0, 1.4, 0));
    const bz = makeBodyAxis(0x79c0ff, new THREE.Vector3(0, 0, 1.4));
    bodyAxesGroup.add(bx, by, bz);
    scene.add(bodyAxesGroup);

    let autoYaw = 0;
    let frameId;

    function animate() {
      frameId = requestAnimationFrame(animate);
      autoYaw += 0.003;
      const camR = 5.5;
      camera.position.x = Math.sin(autoYaw) * camR;
      camera.position.z = Math.cos(autoYaw) * camR;
      camera.position.y = 2.5;
      camera.lookAt(0, 0.2, 0);

      const q = quaternionRef.current;
      rocketGroup.quaternion.copy(q);
      bodyAxesGroup.quaternion.copy(q);

      renderer.render(scene, camera);
    }

    animate();
    sceneRef.current = { renderer, scene, camera };

    const resizeObs = new ResizeObserver(() => {
      if (!el) return;
      const w = el.clientWidth, h = el.clientHeight;
      renderer.setSize(w, h);
      camera.aspect = w / h;
      camera.updateProjectionMatrix();
    });
    resizeObs.observe(el);

    return () => {
      cancelAnimationFrame(frameId);
      resizeObs.disconnect();
      renderer.dispose();
      if (el.contains(renderer.domElement)) el.removeChild(renderer.domElement);
    };
  }, []);

  return (
    <div
      ref={mountRef}
      style={{ width: '100%', height: '100%', minHeight: 320, borderRadius: 8, overflow: 'hidden' }}
    />
  );
}

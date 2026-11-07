// Entrées : boutons tactiles (multi-touch), clavier, inclinaison du téléphone en option.
const held = { left: false, right: false, gas: false, brake: false };
const keys = new Set();
let tilt = 0, tiltOn = false, steer = 0;

export function initInput(onCamera, onPause) {
  document.querySelectorAll('[data-ctl]').forEach(el => {
    const name = el.dataset.ctl, ids = new Set();
    const upd = () => { held[name] = ids.size > 0; el.classList.toggle('on', held[name]); };
    el.addEventListener('pointerdown', e => { e.preventDefault(); ids.add(e.pointerId); el.setPointerCapture?.(e.pointerId); upd(); });
    for (const t of ['pointerup', 'pointercancel', 'lostpointercapture']) el.addEventListener(t, e => { ids.delete(e.pointerId); upd(); });
  });
  addEventListener('keydown', e => {
    if (e.target?.tagName === 'INPUT') return;   // saisie du nom de l'écurie
    keys.add(e.code);
    if (e.code === 'KeyC') onCamera();
    if (e.code === 'Escape' || e.code === 'KeyP') onPause();
    if (e.code.startsWith('Arrow') || e.code === 'Space') e.preventDefault();
  });
  addEventListener('keyup', e => keys.delete(e.code));
  addEventListener('blur', () => { keys.clear(); for (const k in held) held[k] = false; });
  addEventListener('deviceorientation', e => {
    if (e.gamma == null) return;
    const ang = (screen.orientation && screen.orientation.angle) ?? window.orientation ?? 0;
    let a = e.gamma;
    if (ang === 90) a = e.beta; else if (ang === 270 || ang === -90) a = -e.beta;
    tilt = Math.max(-1, Math.min(1, a / 25));
  });
}

// À appeler depuis un geste (clic) : iOS demande une autorisation pour les capteurs.
export async function enableTilt(on) {
  tiltOn = on;
  if (on && typeof DeviceOrientationEvent !== 'undefined' && DeviceOrientationEvent.requestPermission) {
    try { tiltOn = (await DeviceOrientationEvent.requestPermission()) === 'granted'; } catch { tiltOn = false; }
  }
  document.body.classList.toggle('tilt', tiltOn);
  return tiltOn;
}

export function readInput(car, dt) {
  const L = held.left || keys.has('ArrowLeft') || keys.has('KeyA');
  const R = held.right || keys.has('ArrowRight') || keys.has('KeyD');
  const want = tiltOn && !L && !R ? tilt : (R ? 1 : 0) - (L ? 1 : 0);
  // les boutons tout-ou-rien sont adoucis pour éviter les coups de volant
  const rate = want === 0 ? 7 : (Math.sign(want) !== Math.sign(steer) ? 8 : 3.2);
  steer += Math.max(-rate * dt, Math.min(rate * dt, want - steer));
  car.steer = tiltOn ? want * 0.6 + steer * 0.4 : steer;
  car.throttle = held.gas || keys.has('ArrowUp') || keys.has('KeyW') ? 1 : 0;
  car.brake = held.brake || keys.has('ArrowDown') || keys.has('KeyS') || keys.has('Space') ? 1 : 0;
}

export function resetInput() { steer = 0; }

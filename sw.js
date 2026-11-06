// Cache hors-ligne. Incrémenter VERSION à chaque mise à jour du jeu.
const VERSION = 'gpm-v24';
const FILES = ['./', 'index.html', 'style.css', 'manifest.json', 'icon-192.png', 'icon-512.png',
  'lib/three.module.min.js', 'js/main.js', 'js/data.js', 'js/track.js', 'js/car.js', 'js/ai.js', 'js/race.js', 'js/input.js', 'js/hud.js', 'js/scenery.js', 'js/carmodel.js', 'js/textures.js', 'js/garage.js', 'js/team.js', 'js/press.js', 'js/season.js', 'js/career.js', 'js/strategy.js', 'js/safety.js', 'js/audio.js', 'js/fx.js', 'js/infra.js', 'js/market.js', 'js/ui.js',
  'assets/asphalte.jpg', 'assets/asphalte-normal.jpg', 'assets/herbe.jpg', 'assets/sable.jpg', 'assets/gravier.jpg', 'assets/nuages.jpg', 'assets/nuages-alpha.jpg',
  'sons/valdrin.wav', 'sons/orsetti.wav', 'sons/hexadyne.wav', 'sons/kiyora.wav'];
self.addEventListener('install', e => { e.waitUntil(caches.open(VERSION).then(c => c.addAll(FILES))); self.skipWaiting(); });
self.addEventListener('activate', e => {
  e.waitUntil(caches.keys().then(ks => Promise.all(ks.filter(k => k !== VERSION).map(k => caches.delete(k)))));
  self.clients.claim();
});
// réseau d'abord (pour voir les mises à jour), cache si hors-ligne
self.addEventListener('fetch', e => {
  e.respondWith(fetch(e.request).then(r => {
    const copy = r.clone(); caches.open(VERSION).then(c => c.put(e.request, copy)); return r;
  }).catch(() => caches.match(e.request)));
});

// Boîte mail de la carrière : l'écurie reçoit ses infos (presse, mercato, événements, direction, règlement,
// sponsors, usine) sous forme de messages datés, lus ou non lus. Sauvegarde gpm-mail.
import { season, today, fmtDate } from './season.js';

const KEY = 'gpm-mail', MAX = 150;
export const TAGS = {
  direction: ['🏛️', 'Direction', '#ffd84d'],
  presse: ['🎙️', 'Presse', '#ffb02e'],
  mercato: ['🔁', 'Mercato', '#22d3c5'],
  evenement: ['⚡', 'Événements', '#ff3d2e'],
  reglement: ['📜', 'Règlement', '#b06cff'],
  sponsor: ['🤝', 'Sponsors', '#4fd17a'],
  usine: ['🏭', 'Usine', '#6fa8ff'],
  equipe: ['📨', 'Équipe', '#8d96a3'],
};
let box = load();
function load() { try { const b = JSON.parse(localStorage.getItem(KEY)); if (b && Array.isArray(b.list)) return b; } catch {} return { seq: 0, list: [] }; }
const saveMail = () => { try { localStorage.setItem(KEY, JSON.stringify(box)); } catch {} };

// envoie un message ; body : texte (retours à la ligne gardés) ou HTML simple
export function sendMail({ from, subject, body = '', tag = 'equipe' } = {}) {
  if (!subject) return null;
  const d = today();
  const m = { id: ++box.seq, from: from || 'Ton écurie', subject, body: String(body).replace(/\n/g, '<br>'), tag: TAGS[tag] ? tag : 'equipe',
    date: fmtDate(d), y: season?.year || d.getFullYear(), ts: +d, read: false };
  box.list.unshift(m);
  if (box.list.length > MAX) box.list.length = MAX;
  saveMail();
  return m;
}
export const mails = () => box.list;
export const unread = () => box.list.filter(m => !m.read).length;
export function markRead(id) { const m = box.list.find(x => x.id === id); if (m && !m.read) { m.read = true; saveMail(); } }
export function markAll() { box.list.forEach(m => m.read = true); saveMail(); }
export function clearMail() { box = { seq: 0, list: [] }; saveMail(); }
// petit libellé pour un bouton : « ✉️ Boîte mail · 3 »
export const mailLabel = () => { const n = unread(); return `✉️ Boîte mail${n ? ` <span class="mlbadge">${n}</span>` : ''}`; };

// écran de la boîte : filtres par type, liste à gauche, message ouvert à droite (en portrait : l'un ou l'autre)
let filt = 'all', open = 0;
export function openInbox(el, onBack) {
  const esc = s => String(s).replace(/[&<>]/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;' }[c]));
  if (!box.list.some(m => m.id === open)) open = 0;
  const render = () => {
    const all = box.list, n = unread();
    const list = all.filter(m => filt === 'all' || (filt === 'new' ? !m.read : m.tag === filt));
    const tags = Object.keys(TAGS).filter(t => all.some(m => m.tag === t));
    const cur = all.find(m => m.id === open);
    const chip = (k, label, cnt) => `<button data-mf="${k}" class="${filt === k ? 'sel' : ''}">${label}${cnt ? ` <span class="mlbadge">${cnt}</span>` : ''}</button>`;
    el.innerHTML = `<div class="card wide mailbox ${cur ? 'reading' : ''}">
      <div class="ghead"><h2>✉️ Boîte mail</h2><span class="mlcount">${n ? `<b>${n}</b> non lu${n > 1 ? 's' : ''}` : 'Tout est lu'} · ${all.length} message${all.length > 1 ? 's' : ''}</span>
        ${n ? '<button id="mlAll">Tout marquer lu</button>' : ''}<button id="mlBack">Retour</button></div>
      <div class="mltabs">${chip('all', 'Tous')}${chip('new', 'Non lus', n)}${tags.map(t => chip(t, TAGS[t][0] + ' ' + TAGS[t][1], all.filter(m => m.tag === t && !m.read).length)).join('')}</div>
      <div class="mlbody">
        <div class="mllist">${list.length ? list.map(m => `<button class="mli ${m.read ? '' : 'new'} ${m.id === open ? 'on' : ''}" data-mid="${m.id}" style="--c:${TAGS[m.tag][2]}">
          <span class="mlic">${TAGS[m.tag][0]}</span><span class="mlt"><small>${esc(m.from)}</small><b>${esc(m.subject)}</b></span><em>${m.date}${m.y !== season?.year ? ' ' + m.y : ''}</em></button>`).join('')
          : `<p class="dim mlempty">${all.length ? 'Aucun message dans cette catégorie.' : 'Pas encore de message. Les nouvelles de l’écurie arriveront ici après chaque Grand Prix.'}</p>`}</div>
        <div class="mlread">${cur ? `<button id="mlList" class="linkish">← Messages</button>
          <div class="mlhead" style="--c:${TAGS[cur.tag][2]}"><span class="mlic">${TAGS[cur.tag][0]}</span><div><small>${TAGS[cur.tag][1]} · ${cur.date} ${cur.y}</small><h3>${esc(cur.subject)}</h3><p class="dim">De : ${esc(cur.from)}</p></div></div>
          <div class="mltxt">${cur.body}</div>` : `<p class="dim mlempty">Choisis un message.</p>`}</div>
      </div></div>`;
    el.querySelectorAll('[data-mf]').forEach(b => b.onclick = () => { filt = b.dataset.mf; render(); });
    el.querySelectorAll('[data-mid]').forEach(b => b.onclick = () => { open = +b.dataset.mid; markRead(open); render(); });
    const a = el.querySelector('#mlAll'); if (a) a.onclick = () => { markAll(); render(); };
    const l = el.querySelector('#mlList'); if (l) l.onclick = () => { open = 0; render(); };
    el.querySelector('#mlBack').onclick = () => { open = 0; onBack(); };
  };
  // grand écran : on ouvre d'office le message non lu le plus récent
  if (!open && matchMedia('(orientation: landscape)').matches) { const f = box.list.find(m => !m.read) || box.list[0]; if (f) { open = f.id; markRead(open); } }
  render();
}

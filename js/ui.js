// Petits composants d'interface partagés : cartes pilote / moteur / niveau d'écurie, étapes.
const hex = c => typeof c === 'number' ? '#' + c.toString(16).padStart(6, '0') : c;
export const initials = n => n.replace(/^[A-Z]\.\s*/, '').slice(0, 3).toUpperCase();
// note générale d'un pilote (même pondération que le salaire)
export const rating = s => Math.round(s.spd * 0.6 + s.strat * 0.15 + s.tyre * 0.15 + s.pot * 0.1);
const tierOf = v => v >= 88 ? 'gold' : v >= 80 ? 'good' : v >= 70 ? 'mid' : 'low';
const mini = (l, v) => `<span class="ms ${tierOf(v)}"><small>${l}</small><b>${Math.round(v)}</b><i><em style="width:${v}%"></em></i></span>`;

// carte pilote : s = stats { name, spd, strat, tyre, pot }, from = origine, price = texte du prix, color = couleur d'écurie
export function driverCard(s, { k, from = '', price = '', color, pick, disabled, cur } = {}) {
  const c = color ? hex(color) : /Formule B/.test(from) ? '#22d3c5' : /Vétéran/.test(from) ? '#b07cff' : '#8d96a3';
  const r = rating(s);
  return `<button class="copt drv ${pick ? 'pick' : ''} ${cur ? 'cur' : ''}" data-v="${k}" ${disabled ? 'disabled' : ''} style="--c:${c}">
    <span class="dtop"><span class="av">${initials(s.name)}</span><span class="dn"><b>${s.name}</b><small>${from}</small></span><span class="ovr ${tierOf(r)}">${r}<small>note</small></span></span>
    <span class="dstats">${mini('VIT', s.spd)}${mini('STR', s.strat)}${mini('PNE', s.tyre)}${mini('POT', s.pot)}</span>
    <em class="price">${price}</em></button>`;
}

// carte motoriste
export function engineCard(e, { pick, disabled, cur, price } = {}) {
  return `<button class="copt eng ${pick ? 'pick' : ''}" data-v="${e.id}" ${disabled ? 'disabled' : ''} style="--c:${e.color}">
    <span class="eband"><b>${e.name}</b>${cur ? '<small>contrat actuel</small>' : ''}</span>
    <span class="ebig"><span><small>Performance</small><b>${e.perf}</b><i><em style="width:${e.perf}%"></em></i></span><span><small>Fiabilité</small><b>${e.fiab}</b><i><em style="width:${e.fiab}%"></em></i></span></span>
    ${e.conso ? `<span class="econ">⛽ Consommation <b class="${e.conso > 100 ? 'bad' : e.conso < 100 ? 'good' : ''}">${e.conso === 100 ? 'moyenne' : (e.conso > 100 ? '+' : '−') + Math.abs(e.conso - 100) + ' %'}</b></span>` : ''}
    <em class="price">${price ?? e.price + ' M€ / saison'}</em></button>`;
}

// carte niveau d'écurie
export function tierCard(t, k, pick) {
  const car = Math.round(60 + (t.perf - 0.95) * 1000);
  return `<button class="copt tier ${pick ? 'pick' : ''}" data-v="${t.id}" style="--k:${k}">
    <span class="stars">${'★'.repeat(k + 1)}<i>${'★'.repeat(2 - k)}</i></span>
    <b>${t.name}</b><small>${t.desc}</small>
    <span class="tstats"><span><small>Voiture</small><b>${car}</b></span><span><small>Réputation</small><b>${t.rep}</b></span><span><small>Moral</small><b>${t.moral}</b></span></span>
    <em class="price">${t.budget} M€<small> de budget</small></em></button>`;
}

// fil d'étapes en haut de l'écran
export function steps(el, labels, cur) {
  el.innerHTML = labels.length < 2 ? '' : labels.map((l, k) => `<span class="${k < cur ? 'done' : k === cur ? 'on' : ''}"><b>${k < cur ? '✓' : k + 1}</b>${l}</span>`).join('<i></i>');
}

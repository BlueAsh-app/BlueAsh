// Builds companion/story.json, the story-chapter data for the Tarkov map's story helper.
//   node source/build-story.mjs           (reports lines missing from companion/story.ko.tsv)
// Sources (licences checked 2026-11-03; nothing is invented here, every value comes from one of them):
//  - Chapters, ordered objectives, mutually exclusive sub-quests, endings, notes:
//    tarkov-data-overlay storyChapters.json5 (TarkovTracker.org, MIT), read from the public copy
//    miwidot/stammtisch-tracker-overlay pinned to one commit (the tarkovtracker-org repo is no longer public).
//  - Positions: szepiz/tarkov-quest-data api/firstparty/story-marks.json (CC0, placed by hand, checked in game).
//  - Which maps an objective is on: szepiz api/maps.json story.chapters (same overlay, older snapshot; ids only).
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const OVERLAY_SHA = '3cd9a38c07590a91153c096a0cd72e5d049b175e', OVERLAY_DATE = '2026-09-16';
const SZEPIZ_SHA = '26e334a29f68743eedce4bd175a6e608354afb59', SZEPIZ_DATE = '2026-08-26';
const overlayUrl = `https://raw.githubusercontent.com/miwidot/stammtisch-tracker-overlay/${OVERLAY_SHA}/src/additions/storyChapters.json5`;
const marksUrl = `https://raw.githubusercontent.com/szepiz/tarkov-quest-data/${SZEPIZ_SHA}/api/firstparty/story-marks.json`;
const mapsUrl = `https://raw.githubusercontent.com/szepiz/tarkov-quest-data/${SZEPIZ_SHA}/api/maps.json`;
const root = path.join(path.dirname(fileURLToPath(import.meta.url)), '..');

// Map names in the sources -> BlueAsh map ids (maps/catalog.json).
const MAP = { 'Customs': 'bigmap', 'Woods': 'woods', 'Shoreline': 'shoreline', 'Interchange': 'interchange', 'Reserve': 'rezervbase',
  'Lighthouse': 'lighthouse', 'Streets of Tarkov': 'tarkovstreets', 'The Lab': 'laboratory', 'Ground Zero': 'sandbox', 'Factory': 'factory4_day',
  'The Labyrinth': 'labyrinth', 'Icebreaker': 'icebreaker', 'Terminal': 'terminal' };

// The overlay file is JSON5 (comments, single quotes, bare keys, trailing commas): a small reader for exactly that.
function json5(src) {
  let i = 0;
  const ws = () => { for (;;) { while (/\s/.test(src[i])) i++; if (src.startsWith('//', i)) { while (i < src.length && src[i] !== '\n') i++; } else if (src.startsWith('/*', i)) { i = src.indexOf('*/', i) + 2; } else return; } };
  const str = () => { const q = src[i++]; let s = ''; while (src[i] !== q) { if (src[i] === '\\') { const c = src[++i]; s += c === 'n' ? '\n' : c === 't' ? '\t' : c === 'u' ? String.fromCharCode(parseInt(src.substr(i + 1, 4), 16)) : c; if (c === 'u') i += 4; i++; } else s += src[i++]; if (i >= src.length) throw new Error('unterminated string'); } i++; return s; };
  const val = () => {
    ws(); const c = src[i];
    if (c === '{') { i++; const o = {}; for (;;) { ws(); if (src[i] === '}') { i++; return o; } const k = src[i] === '"' || src[i] === "'" ? str() : src.slice(i, i = src.slice(i).search(/[^A-Za-z0-9_$]/) + i); ws(); if (src[i++] !== ':') throw new Error('expected : at ' + i); o[k] = val(); ws(); if (src[i] === ',') i++; } }
    if (c === '[') { i++; const a = []; for (;;) { ws(); if (src[i] === ']') { i++; return a; } a.push(val()); ws(); if (src[i] === ',') i++; } }
    if (c === '"' || c === "'") return str();
    const m = /^(true|false|null|-?\d+(\.\d+)?([eE][+-]?\d+)?)/.exec(src.slice(i)); if (!m) throw new Error('unexpected ' + src.substr(i, 20));
    i += m[0].length; return JSON.parse(m[0]);
  };
  const v = val(); ws(); if (i < src.length) throw new Error('trailing data'); return v;
}

const get = async (u) => { const r = await fetch(u); if (!r.ok) throw new Error(u + ' ' + r.status); return r.text(); };
const overlay = json5(await get(overlayUrl));
const marks = JSON.parse(await get(marksUrl));
const older = JSON.parse(await get(mapsUrl)).story.chapters;
const oldMaps = {}; for (const c of older) for (const o of c.objectives) oldMaps[o.id] = o.maps || [];
const marksOf = {}; for (const m of marks.marks) (marksOf[m.objectiveId] ||= []).push(m);

const ours = (n) => MAP[n] || null;
let placed = 0, steps = 0;
const chapters = Object.values(overlay).sort((a, b) => a.order - b.order).map((c) => {
  const out = {
    id: c.id, name: c.name, order: c.order, wiki: c.wikiLink, autoStart: !!c.autoStart,
    requires: (c.chapterRequirements || []).map((r) => r.id), activation: c.activation?.summary || '', description: c.description || '',
    notes: c.notes || '', rewards: c.rewards?.description || '',
    coverage: { referenced: c.referenceCoverage.referencedSubquests, resolved: c.referenceCoverage.resolvedSubquests, missingTexts: c.referenceCoverage.missingObjectiveTexts || 0, partial: !!c.referenceCoverage.partial },
    branches: (c.mutuallyExclusiveQuestPairs || []).map(([a, b]) => [a, b]),
    endings: (c.endings || []).map((e) => ({ id: e.id, system: e.systemName, gate: e.gateQuestId, objectives: e.objectiveCount })),
    unlocks: { maps: (c.mapUnlocks || []).map((m) => m.name), traders: (c.traderUnlocks || []).map((t) => t.name) },
    steps: c.objectives.map((o) => {
      steps++;
      const ms = marksOf[o.id] || [];
      const maps = new Set([...(oldMaps[o.id] || []), ...ms.flatMap((m) => m.maps || [])].map(ours).filter(Boolean));
      const pts = ms.flatMap((m) => m.points).map((p) => ({ map: ours(p.map), floor: p.floor, area: p.kind === 'area', pts: p.pts.map((q) => [q.x, q.z]) })).filter((p) => p.map);
      if (pts.length) placed++;
      const s = { id: o.id, text: o.description, quest: o.sourceQuestId, optional: o.type === 'optional', maps: [...maps], marks: pts };
      if (o.endingId) s.ending = o.endingId; if (o.indent) s.indent = o.indent;
      return s;
    }),
  };
  return out;
});

const data = {
  checked: new Date().toLocaleDateString('sv'), // local date
  sources: [
    { name: 'tarkov-data-overlay (TarkovTracker.org)', license: 'MIT', asOf: OVERLAY_DATE, url: overlayUrl, what: 'chapters, objectives, branches, endings' },
    { name: 'szepiz/tarkov-quest-data story-marks', license: 'CC0-1.0', asOf: SZEPIZ_DATE, url: marksUrl, what: 'positions' },
  ],
  sharedModes: 'The overlay states the storyline is shared between PVP and PVE; BlueAsh keeps progress per mode and does not assume more.',
  chapters,
};
fs.writeFileSync(path.join(root, 'companion', 'story.json'), JSON.stringify(data));
console.log(`chapters ${chapters.length}, steps ${steps}, steps with positions ${placed}`);

// Text per UI language: companion/story.<lang>.tsv (English <TAB> translation), written by hand (Korean first; the
// others through source/story-i18n.mjs); report what is missing in each.
const want = new Set(); for (const c of chapters) { for (const s of c.steps) want.add(s.text); for (const t of [c.activation, c.description, c.notes, c.rewards]) if (t) want.add(t); }
for (const lang of ['ko', 'zh-CN', 'ja', 'ru', 'es', 'pt-BR', 'de', 'fr', 'it', 'pl', 'tr', 'zh-TW']) {
  const tsv = path.join(root, 'companion', `story.${lang}.tsv`);
  const have = new Set(fs.existsSync(tsv) ? fs.readFileSync(tsv, 'utf8').replace(/^﻿/, '').split(/\r?\n/).map((l) => l.split('\t')[0]) : []);
  const missing = [...want].filter((t) => !have.has(t));
  if (missing.length) { fs.writeFileSync(path.join(root, 'source', `story-missing.${lang}.txt`), missing.join('\n')); console.log(`missing ${lang}: ${missing.length} (source/story-missing.${lang}.txt)`); }
  else console.log(`${lang}: complete`);
}

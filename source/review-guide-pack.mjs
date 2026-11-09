// Second pass over the translation packs (relay/public/quest-guide/tr/<lang>.json, made by build-guide-pack.mjs with the small
// model): a larger model on the developer's PC reads each English line beside its translation and returns it corrected
// (game terms, natural wording, half-translated words), or unchanged. Guides reviewed by people on the server (by=review)
// are left as they are; everything machine-made is checked: other guides, picture captions, key locations.
// Model: Qwen3-30B-A3B-Instruct-2507 Q4_K_M (Apache-2.0) in %LOCALAPPDATA%\ModeSwitch\ai\review — a mixture of experts,
// so the expert weights of some layers sit in system memory (--n-cpu-moe) and a 16 GB graphics card still runs it fast.
// Resumable: every checked pair is kept in %TEMP%\blueash-pack\review-<lang>.json.
//   node source/review-guide-pack.mjs --server s.json --sources src.json [--lang ko] [--ncmoe 12]
import fs from "node:fs";
import path from "node:path";
import os from "node:os";
import { spawn } from "node:child_process";
import { fileURLToPath } from "node:url";
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const arg = (n, d = "") => { const i = process.argv.indexOf(n); return i > 0 ? process.argv[i + 1] : d; };
const only = arg("--lang");
const NAMES = { ko: "Korean", ja: "Japanese", "zh-CN": "Simplified Chinese", ru: "Russian", de: "German", es: "Spanish", fr: "French", "pt-BR": "Brazilian Portuguese", it: "Italian", pl: "Polish", tr: "Turkish", "zh-TW": "Traditional Chinese (Taiwan)" };
const read = (f) => JSON.parse(fs.readFileSync(f, "utf8").replace(/^﻿/, ""));
const reviewed = {}; for (const r of read(arg("--server"))[0].results) if (r.by === "review") (reviewed[r.lang] ??= new Set()).add(r.id);
const sources = read(arg("--sources"));
const srcDir = path.join(root, "relay/public/quest-guide/src");
const english = {}; for (const f of fs.readdirSync(srcDir)) { const j = read(path.join(srcDir, f)); if (j.text) english[f.replace(/\.json$/, "")] = j.text; }
// Game names, as in build-guide-pack.mjs (companion/ai-glossary.tsv).
const glossary = {}; for (const l of fs.readFileSync(path.join(root, "companion/ai-glossary.tsv"), "utf8").replace(/^﻿/, "").split(/\r?\n/)) { const p = l.split("\t"); if (p.length === 3 && !l.startsWith("#")) (glossary[p[0]] ??= []).push([p[1], p[2]]); }
for (const g of Object.values(glossary)) g.sort((a, b) => b[0].length - a[0].length);
const word = (c) => /[\p{L}\p{N}']/u.test(c || "");
function namesIn(lang, text) { const hits = [], taken = [];
  for (const [en, tr] of glossary[lang] || []) { const exact = en[0] === en[0].toUpperCase() && en[0] !== en[0].toLowerCase(); const hay = exact ? text : text.toLowerCase(), needle = exact ? en : en.toLowerCase(); let at = 0;
    while ((at = hay.indexOf(needle, at)) >= 0) { const end = at + en.length; if (!word(text[at - 1]) && !word(text[end]) && !taken.some(([a, b]) => at < b && end > a)) { taken.push([at, end]); hits.push(en + " = " + tr); break; } at = end; }
    if (hits.length >= 40) break; }
  return hits.join("; "); }
const broken = (t) => /[぀-ヿ㐀-鿿가-힣][a-z]{2}/.test(t);
// The model: llama-server on a port of its own.
const ai = path.join(os.homedir(), "AppData/Local/ModeSwitch/ai");
const model = path.join(ai, "review/Qwen3-30B-A3B-Instruct-2507-Q4_K_M.gguf");
const port = 51892, base = `http://127.0.0.1:${port}`;
async function up() { try { return (await fetch(base + "/health")).ok; } catch { return false; } }
let proc = null;
async function start() { if (await up()) return;
  proc = spawn(path.join(ai, "llama-b12065/llama-server.exe"), ["-m", model, "-ngl", "99", "--n-cpu-moe", arg("--ncmoe", "12"), "-c", "8192", "-np", "1", "-fa", "on", "--host", "127.0.0.1", "--port", String(port), "--no-webui"], { cwd: path.join(ai, "llama-b12065"), stdio: "ignore", windowsHide: true });
  for (let i = 0; i < 600; i++) { if (await up()) return; await new Promise((r) => setTimeout(r, 500)); } throw new Error("llama-server did not start"); }
async function ask(system, pairs) {
  const r = await fetch(base + "/v1/chat/completions", { method: "POST", headers: { "content-type": "application/json" }, body: JSON.stringify({ messages: [{ role: "system", content: system }, { role: "user", content: JSON.stringify(pairs.map(([en, tr]) => ({ en, tr }))) }], temperature: 0.1, max_tokens: 4096 }) });
  if (!r.ok) return null; const t = (await r.json()).choices?.[0]?.message?.content || ""; const a = t.indexOf("["), b = t.lastIndexOf("]"); if (a < 0 || b < a) return null;
  try { const out = JSON.parse(t.slice(a, b + 1)); return Array.isArray(out) && out.length === pairs.length ? out.map((x) => (typeof x === "string" ? x : x?.tr != null ? String(x.tr) : null)) : null; } catch { return null; } }
function system(lang, context, names) {
  let s = `You review ${NAMES[lang]} translations of text from the Escape from Tarkov wiki, for players of the game. You get a JSON array of {"en": English, "tr": translation}. For each item return the translation corrected: it must say exactly what the English says, read naturally to a native ${NAMES[lang]} gamer, use the names the ${NAMES[lang]} version of the game uses, and have no English words left half-translated. If it is already good, return it unchanged. Keep numbers, units and a leading "• ". Answer with only a JSON array of the corrected translation strings, in the same order.`;
  if (context) s += " The items belong to this text (for meaning only): " + context.slice(0, 900);
  if (names) s += ` Game names (English = ${NAMES[lang]}): ${names}.`; return s; }
const cacheDir = path.join(os.tmpdir(), "blueash-pack");
// Pairs -> corrected translations through the cache; batches of up to 16 pairs / 2400 characters, one at a time when a batch fails.
async function review(lang, cache, pairs, context) {
  const key = ([en, tr]) => en + "\u0001" + tr;
  const todo = [...new Map(pairs.filter(([en, tr]) => en.trim() && tr && !(key([en, tr]) in cache)).map((p) => [key(p), p])).values()];
  for (let i = 0; i < todo.length;) { const batch = []; let n = 0; while (i < todo.length && batch.length < 16 && (!batch.length || n + todo[i][0].length + todo[i][1].length < 2400)) { n += todo[i][0].length + todo[i][1].length; batch.push(todo[i++]); }
    const sys = system(lang, context, namesIn(lang, batch.map((p) => p[0]).join("\n"))); let got = await ask(sys, batch) || batch.map(() => null);
    for (let k = 0; k < batch.length; k++) if (!got[k] || broken(got[k])) { const one = await ask(sys, [batch[k]]); got[k] = one && one[0] && !broken(one[0]) ? one[0] : null; }
    batch.forEach((p, k) => { let t = (got[k] || "").trim(); if (!t) t = p[1]; if (p[0].startsWith("• ") && !t.startsWith("•")) t = "• " + t.replace(/^[-*\s]+/, ""); cache[key(p)] = t; }); }
  return pairs.map((p) => (p[0].trim() && p[1] ? cache[key(p)] ?? p[1] : p[1])); }
// Line pairs of a text and its translation; texts whose line counts differ are reviewed whole.
function linePairs(en, tr) { const a = en.split("\n"), b = tr.split("\n"); return a.length === b.length ? a.map((l, i) => [l, b[i]]) : [[en, tr]]; }
await start();
try {
  for (const lang of Object.keys(NAMES)) { if (only && lang !== only) continue;
    const pf = path.join(root, "relay/public/quest-guide/tr", lang + ".json"); if (!fs.existsSync(pf)) continue; const pack = read(pf);
    const cf = path.join(cacheDir, "review-" + lang + ".json"), cache = fs.existsSync(cf) ? read(cf) : {}; const save = () => fs.writeFileSync(cf, JSON.stringify(cache));
    let changed = 0, checked = 0; const t0 = Date.now();
    for (const [id, tr] of Object.entries(pack.guides)) { if (reviewed[lang]?.has(id) || !english[id]) continue; const pairs = linePairs(english[id], tr);
      const out = await review(lang, cache, pairs, english[id]); const joined = pairs.length === 1 && pairs[0][0] === english[id] ? out[0] : out.join("\n"); checked++; if (joined !== tr) { pack.guides[id] = joined; changed++; } save(); }
    const caps = Object.keys(pack.caps).map((c) => [c, pack.caps[c]]); const capOut = await review(lang, cache, caps, "Captions of screenshots in Escape from Tarkov quest and key guides.");
    caps.forEach(([c, tr], k) => { checked++; if (capOut[k] !== tr) { pack.caps[c] = capOut[k]; changed++; } }); save();
    for (const [id, tr] of Object.entries(pack.keys)) { const en = sources.keys[id]; if (!en) continue; const pairs = linePairs(en, tr); const out = await review(lang, cache, pairs, en);
      const joined = pairs.length === 1 && pairs[0][0] === en ? out[0] : out.join("\n"); checked++; if (joined !== tr) { pack.keys[id] = joined; changed++; } } save();
    pack.reviewed = new Date().toISOString().slice(0, 10); fs.writeFileSync(pf, JSON.stringify(pack));
    console.log(`${lang}: checked ${checked}, corrected ${changed}, ${Math.round((Date.now() - t0) / 1000)} s`); }
} finally { if (proc) proc.kill(); }

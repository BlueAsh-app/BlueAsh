// Translation packs for the quest and key cards: relay/public/quest-guide/tr/<lang>.json, one file per language that the app
// downloads (a static file: no worker run, no database read) and keeps a day. Made on the developer's PC with the same
// local translator as 모드 번역 (llama.cpp + Qwen3-4B in %LOCALAPPDATA%\BlueAsh\ai), so users neither download the
// model nor load their graphics card for this.
//   guides  quest id (and "<id>-b" briefings) -> text: the server's translations (reviewed ones first) as they are;
//           only those the server lacks are translated here, line by line with the whole text as context
//   caps    English picture caption (spaces collapsed) -> caption, from quest guides and key lock pictures
//   keys    key item id -> where the key is found (the wiki's Key Location section)
// Inputs: --server <json>  rows of `wrangler d1 execute modepulse --remote --json --command "SELECT lang,id,text,by FROM guide WHERE by<>'error'"`
//         --links <json>   English wiki url -> {lang: url}, the language wikis that card buttons open (wiki-langlinks.mjs)
//         --sources <json> {captions:[...], keys:{id:text}} pulled from the wiki with the app's own parser (CompanionUI QuestWiki/KeyWiki)
// Resumable: every translated line is kept in %TEMP%\blueash-pack\<lang>.json. Run:  node source/build-guide-pack.mjs --server s.json --sources src.json
import fs from "node:fs";
import path from "node:path";
import os from "node:os";
import { spawn } from "node:child_process";
import { fileURLToPath } from "node:url";
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const arg = (n) => { const i = process.argv.indexOf(n); return i > 0 ? process.argv[i + 1] : ""; };
const only = arg("--lang");
const NAMES = { ko: "Korean", ja: "Japanese", "zh-CN": "Simplified Chinese", ru: "Russian", de: "German", es: "Spanish", fr: "French", "pt-BR": "Brazilian Portuguese", it: "Italian", pl: "Polish", tr: "Turkish", "zh-TW": "Traditional Chinese (Taiwan)" };
const read = (f) => JSON.parse(fs.readFileSync(f, "utf8").replace(/^﻿/, ""));
const server = {}; for (const r of read(arg("--server"))[0].results) { (server[r.lang] ??= {})[r.id] = r.text; }
const sources = read(arg("--sources"));
const links = arg("--links") ? read(arg("--links")) : {}; // English wiki url -> {lang: url} (source/wiki-langlinks.mjs)
const srcDir = path.join(root, "relay/public/quest-guide/src");
const guides = {}; for (const f of fs.readdirSync(srcDir)) { const j = read(path.join(srcDir, f)); if (j.text) guides[f.replace(/\.json$/, "")] = j.text; }
// Game names: the same glossary the app's own translator uses (companion/ai-glossary.tsv), longest first.
const glossary = {}; for (const l of fs.readFileSync(path.join(root, "companion/ai-glossary.tsv"), "utf8").replace(/^﻿/, "").split(/\r?\n/)) { const p = l.split("\t"); if (p.length === 3 && !l.startsWith("#")) (glossary[p[0]] ??= []).push([p[1], p[2]]); }
for (const g of Object.values(glossary)) g.sort((a, b) => b[0].length - a[0].length);
const word = (c) => /[\p{L}\p{N}']/u.test(c || "");
function namesIn(lang, text) { const hits = [], taken = [];
  for (const [en, tr] of glossary[lang] || []) { const exact = en[0] === en[0].toUpperCase() && en[0] !== en[0].toLowerCase(); const hay = exact ? text : text.toLowerCase(), needle = exact ? en : en.toLowerCase(); let at = 0;
    while ((at = hay.indexOf(needle, at)) >= 0) { const end = at + en.length; if (!word(text[at - 1]) && !word(text[end]) && !taken.some(([a, b]) => at < b && end > a)) { taken.push([at, end]); hits.push(en + " = " + tr); break; } at = end; }
    if (hits.length >= 40) break; }
  return hits.join("; "); }
// The local translator: llama-server on a port of its own, on the graphics card.
const ai = (fs.existsSync(path.join(os.homedir(),"AppData/Local/BlueAsh"))?path.join(os.homedir(),"AppData/Local/BlueAsh/ai"):path.join(os.homedir(),"AppData/Local/ModeSwitch/ai")) /* data folder renamed to BlueAsh in 2.28 */;
const port = 51891, base = `http://127.0.0.1:${port}`;
async function up() { try { return (await fetch(base + "/health")).ok; } catch { return false; } }
let server_ = null;
async function start() { if (await up()) return;
  server_ = spawn(path.join(ai, "llama-b12065/llama-server.exe"), ["-m", path.join(ai, "Qwen3-4B-Instruct-2507-Q4_K_M.gguf"), "-ngl", "99", "-c", "8192", "-np", "1", "--host", "127.0.0.1", "--port", String(port), "--no-webui"], { cwd: path.join(ai, "llama-b12065"), stdio: "ignore", windowsHide: true });
  for (let i = 0; i < 240; i++) { if (await up()) return; await new Promise((r) => setTimeout(r, 500)); } throw new Error("llama-server did not start"); }
async function ask(system, lines) {
  const r = await fetch(base + "/v1/chat/completions", { method: "POST", headers: { "content-type": "application/json" }, body: JSON.stringify({ messages: [{ role: "system", content: system }, { role: "user", content: JSON.stringify(lines) }], temperature: 0.2, max_tokens: 3072 }) });
  if (!r.ok) return null; const t = (await r.json()).choices?.[0]?.message?.content || ""; const a = t.indexOf("["), b = t.lastIndexOf("]"); if (a < 0 || b < a) return null;
  try { const out = JSON.parse(t.slice(a, b + 1)); return Array.isArray(out) && out.length === lines.length ? out.map((x) => (x == null ? null : String(x))) : null; } catch { return null; } }
function system(lang, context, names) {
  let s = `You translate text from the Escape from Tarkov wiki into ${NAMES[lang]} for players of the game. Use the names the ${NAMES[lang]} version of the game uses for maps, traders, items, quests and extracts; keep names you are not sure of in English. Keep numbers, units and a leading "• ". Translate every string; never add or merge strings. Answer with only a JSON array of the translated strings, in the same order.`;
  if (context) s += " The strings belong to this text (for meaning only, do not translate it): " + context.slice(0, 900);
  if (names) s += ` Use exactly these game names (English = ${NAMES[lang]}): ${names}.`; return s; }
// Lines -> translations through the cache; batches of up to 20 lines / 2400 characters, one line at a time when a batch fails.
// A broken answer: half-translated words such as "패ockets" (a CJK letter glued to lowercase Latin). Not kept, retried alone.
const broken = (t) => /[\u3040-\u30ff\u3400-\u9fff\uac00-\ud7a3][a-z]{2}/.test(t);
const cacheDir = path.join(os.tmpdir(), "blueash-pack"); fs.mkdirSync(cacheDir, { recursive: true });
async function translate(lang, cache, lines, context) {
  const todo = [...new Set(lines.filter((l) => l.trim() && !(l in cache)))];
  for (let i = 0; i < todo.length;) { const batch = []; let n = 0; while (i < todo.length && batch.length < 20 && (!batch.length || n + todo[i].length < 2400)) { n += todo[i].length; batch.push(todo[i++]); }
    const sys = system(lang, context, namesIn(lang, batch.join("\n"))); let got = await ask(sys, batch);
    if (!got) got = batch.map(() => null);
    for (let k = 0; k < batch.length; k++) if (!got[k] || broken(got[k])) { const one = await ask(sys, [batch[k]]); got[k] = one && !broken(one[0] || "") ? one[0] : null; }
    batch.forEach((l, k) => { let t = (got[k] || "").trim(); if (!t) return; if (l.startsWith("• ") && !t.startsWith("•")) t = "• " + t.replace(/^[-*\s]+/, ""); cache[l] = t; }); }
  return lines.map((l) => (l.trim() ? cache[l] ?? null : l)); }
await start();
try {
  for (const lang of Object.keys(NAMES)) { if (only && lang !== only) continue;
    const cf = path.join(cacheDir, lang + ".json"), cache = fs.existsSync(cf) ? read(cf) : {}; for (const [k, v] of Object.entries(cache)) if (broken(v)) delete cache[k]; const save = () => fs.writeFileSync(cf, JSON.stringify(cache));
    const pack = { v: 1, lang, made: new Date().toISOString().slice(0, 10), guides: {}, caps: {}, keys: {}, wiki: {} }; for (const [u, m] of Object.entries(links)) if (m[lang]) pack.wiki[u] = m[lang]; let mine = 0, t0 = Date.now();
    for (const [id, text] of Object.entries(guides)) { const have = server[lang]?.[id]; if (have) { pack.guides[id] = have; continue; }
      const out = await translate(lang, cache, text.split("\n"), text); if (out.every((x) => x != null)) { pack.guides[id] = out.join("\n"); mine++; } save(); }
    const caps = sources.captions; const out = await translate(lang, cache, caps, "Captions of screenshots in Escape from Tarkov quest and key guides."); caps.forEach((c, k) => { if (out[k]) pack.caps[c] = out[k]; }); save();
    for (const [id, text] of Object.entries(sources.keys)) { const o = await translate(lang, cache, text.split("\n"), text); if (o.every((x) => x != null)) pack.keys[id] = o.join("\n"); } save();
    const dir = path.join(root, "relay/public/quest-guide/tr"); fs.mkdirSync(dir, { recursive: true }); fs.writeFileSync(path.join(dir, lang + ".json"), JSON.stringify(pack));
    console.log(`${lang}: guides ${Object.keys(pack.guides).length}/${Object.keys(guides).length} (translated here ${mine}), captions ${Object.keys(pack.caps).length}/${caps.length}, keys ${Object.keys(pack.keys).length}/${Object.keys(sources.keys).length}, ${Math.round((Date.now() - t0) / 1000)} s`); }
} finally { if (server_) server_.kill(); }

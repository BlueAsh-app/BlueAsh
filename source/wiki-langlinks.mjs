// Language versions of the wiki pages the quest and key cards open (위키에서 보기): the English Escape from Tarkov wiki's
// interlanguage links (Fandom API prop=langlinks), so a player whose UI language has its own wiki (ru, fr, de, pt-br, …)
// gets that page. Output {englishUrl: {uiLang: url}} for build-guide-pack.mjs --links. Cached in %TEMP%\blueash-pack\links.json.
//   node source/wiki-langlinks.mjs <out.json>
import fs from "node:fs";
import path from "node:path";
import os from "node:os";
import { fileURLToPath } from "node:url";
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
// Fandom language codes -> BlueAsh UI languages
const UI = { ko: "ko", ja: "ja", zh: "zh-CN", "zh-tw": "zh-TW", "zh-hant": "zh-TW", ru: "ru", es: "es", "pt-br": "pt-BR", pt: "pt-BR", de: "de", fr: "fr", it: "it", pl: "pl", tr: "tr" };
const read = (f) => JSON.parse(fs.readFileSync(f, "utf8").replace(/^﻿/, ""));
const pages = {};
for (const f of fs.readdirSync(path.join(root, "relay/public/quest-guide/src"))) { if (f.endsWith("-b.json")) continue; const j = read(path.join(root, "relay/public/quest-guide/src", f)); if (j.wiki) pages[f.replace(/\.json$/, "")] = j.wiki; }
const places = path.join(os.homedir(), "AppData/Local/ModeSwitch/quest-places-pve.json");
if (fs.existsSync(places)) for (const [id, k] of Object.entries(read(places).keys || {})) if (k.wiki) pages[id] = k.wiki;
const cacheFile = path.join(os.tmpdir(), "blueash-pack", "links.json"); fs.mkdirSync(path.dirname(cacheFile), { recursive: true });
const cache = fs.existsSync(cacheFile) ? read(cacheFile) : {};
const out = {};
for (const [id, url] of Object.entries(pages)) {
  const title = decodeURIComponent(url.slice(url.indexOf("/wiki/") + 6));
  if (!(title in cache)) {
    try { const r = await fetch("https://escapefromtarkov.fandom.com/api.php?action=parse&format=json&formatversion=2&prop=langlinks&redirects=1&page=" + encodeURIComponent(title));
      cache[title] = ((await r.json()).parse?.langlinks || []).map((l) => [l.lang, l.url]); } catch { continue; }
    fs.writeFileSync(cacheFile, JSON.stringify(cache)); await new Promise((r) => setTimeout(r, 120)); }
  for (const [lang, link] of cache[title] || []) { const ui = UI[lang]; if (ui && /^https:\/\/escapefromtarkov\.fandom\.com\//.test(link)) (out[url] ??= {})[ui] = link; }
}
fs.writeFileSync(process.argv[2], JSON.stringify(out));
const by = {}; for (const m of Object.values(out)) for (const l of Object.keys(m)) by[l] = (by[l] || 0) + 1;
console.log(Object.keys(pages).length, "pages;", JSON.stringify(by));

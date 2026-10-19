// Sources for the quest-guide translations (relay/guide.js): every quest's English Guide text from the Escape from
// Tarkov Wiki (Fandom, CC BY-SA; extracted like CompanionUI QuestWiki.Load), with a glossary per language of the game
// names the text uses (items, maps, traders, quests from tarkov.dev's locale files, plus common game terms), so the
// translation keeps the names the game shows. Writes relay/public/quest-guide/ids.json (low-level quests first) and
// src/<quest id>.json {text, src (hash of the text), wiki, glossary:{<ui language>: [[english, local], ...]}}.
// Then `npx wrangler deploy` in relay/; the minute cron translates what is missing. Pages are cached in %TEMP%\blueash-guides.
// node source/quest-guides.mjs
import fs from "fs";import path from "path";import crypto from "crypto";import { fileURLToPath } from "url";
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const out = path.join(root, "relay/public/quest-guide"), cache = path.join(process.env.TEMP || "/tmp", "blueash-guides");
fs.mkdirSync(path.join(out, "src"), { recursive: true });fs.mkdirSync(cache, { recursive: true });
const get = async (u) => (await (await fetch(u)).json()).data;
// UI language -> tarkov.dev locale (Traditional Chinese uses the Simplified names; the model writes them in Traditional).
const LOC = { ko: "ko", ja: "ja", "zh-CN": "zh", ru: "ru", de: "de", es: "es", fr: "fr", "pt-BR": "pt", it: "it", pl: "pl", tr: "tr", "zh-TW": "zh" };
// Words the game uses for common things, where a model tends to pick a dictionary word instead.
const TERMS = { ko: { "secure container": "보안 컨테이너", "Scav": "스캐브", "Scavs": "스캐브", "PMC": "PMC", "raid": "레이드", "extract": "탈출", "extraction": "탈출", "extracting": "탈출", "flea market": "플리마켓", "stash": "창고", "hideout": "은신처", "Rogues": "로그", "Raiders": "레이더", "Cultists": "광신도", "keycard": "키카드", "dorms": "기숙사", "sawmill": "제재소", "health resort": "요양소", "checkpoint": "검문소", "Interchange": "인터체인지", "Ultra mall": "울트라 쇼핑몰" },
  ja: { "secure container": "セキュアコンテナ", "Scav": "Scav", "raid": "レイド", "extract": "脱出", "extraction": "脱出", "flea market": "フリーマーケット", "stash": "スタッシュ", "hideout": "ハイドアウト" },
  "zh-CN": { "secure container": "安全箱", "Scav": "Scav", "raid": "战局", "extraction": "撤离", "flea market": "跳蚤市场", "stash": "仓库", "hideout": "藏身处" },
  "zh-TW": { "secure container": "安全箱", "Scav": "Scav", "raid": "戰局", "extraction": "撤離", "flea market": "跳蚤市場", "stash": "倉庫", "hideout": "藏身處", "Customs": "海關", "Woods": "森林", "Shoreline": "海岸線", "Interchange": "立交橋", "Reserve": "儲備站", "Lighthouse": "燈塔", "Streets of Tarkov": "塔科夫街區", "Ground Zero": "中心區", "Factory": "工廠", "The Lab": "實驗室", "Prapor": "Prapor", "Therapist": "Therapist", "Skier": "Skier", "Peacekeeper": "Peacekeeper", "Mechanic": "Mechanic", "Ragman": "Ragman", "Jaeger": "Jaeger" } };
// Russian, Polish, Turkish decline names: a fixed form put into the sentence breaks the grammar, so those get only the
// names the game keeps in English (left as they are); Traditional Chinese gets no Simplified names (tarkov.dev has none).
const ENGLISH_ONLY = new Set(["ru", "pl", "tr", "zh-TW"]);
const tasks = [];for (const mode of ["pve", "regular"]) for (const t of Object.values((await get("https://json.tarkov.dev/" + mode + "/tasks")).tasks)) if (!tasks.find((x) => x.id === t.id)) tasks.push(t);
const en = { items: await get("https://json.tarkov.dev/regular/items_en"), maps: await get("https://json.tarkov.dev/regular/maps_en"), tasks: await get("https://json.tarkov.dev/regular/tasks_en"), traders: await get("https://json.tarkov.dev/regular/traders_en") };
const loc = {};for (const l of new Set(Object.values(LOC))) loc[l] = { items: await get("https://json.tarkov.dev/regular/items_" + l), maps: await get("https://json.tarkov.dev/regular/maps_" + l), tasks: await get("https://json.tarkov.dev/regular/tasks_" + l), traders: await get("https://json.tarkov.dev/regular/traders_" + l) };
// English name -> per-locale name, from keys that end in " Name"/" name"/" Nickname" (items, maps, quests, traders).
// Names are kept even when the language uses the English one (so "Shoreline" or "Machinery key" stay as the game shows
// them instead of being translated as words). Maps, traders, quests and one-word items match only as written ("Woods"
// is not "the woods", "The Door" not "the door"); longer item names match any case.
const pairs = {};for (const l of Object.keys(loc)) { const m = new Map();
  for (const kind of ["maps", "traders", "items", "tasks"]) for (const [k, v] of Object.entries(en[kind] || {})) { if (!/ (Name|name|Nickname)$/.test(k) || typeof v !== "string" || v.length < 4) continue;const w = loc[l][kind]?.[k];const item = kind === "items" || (kind === "tasks" && / Name$/.test(k)), proper = !item || !/\s/.test(v);
    if (typeof w !== "string" || !w) continue;if (!m.has(v.toLowerCase())) m.set(v.toLowerCase(), [v, w, proper]);
    // Guides often shorten long item names ("Bronze pocket watch on a chain" -> "bronze pocket watch"): the part before
    // " on a ", " with ", " (" also counts when it is two words or more. Quest items are in the tasks files (" Name").
    if (item) { const short = v.split(/ on an? | with | \(/)[0].trim();if (short !== v && /\s/.test(short) && short.length >= 8 && !m.has(short.toLowerCase())) m.set(short.toLowerCase(), [short, w, false]); } }
  pairs[l] = [...m.values()].sort((a, b) => b[0].length - a[0].length); }
const esc = (s) => s.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
function glossary(ui, text) {
  const hits = [], lower = text.toLowerCase();let rest = lower, restCase = text;
  for (const [a, b, proper] of pairs[LOC[ui]]) { const k = a.toLowerCase();if (!rest.includes(k)) continue;
    if (proper ? !new RegExp("(^|[^A-Za-z0-9])" + esc(a) + "([^A-Za-z0-9]|$)").test(restCase) : !new RegExp("(^|[^a-z0-9])" + esc(k) + "([^a-z0-9]|$)").test(rest)) continue;
    hits.push([a, b]);rest = rest.split(k).join(" ");restCase = restCase.split(a).join(" ");if (hits.length >= 45) break; }
  for (const [a, b] of Object.entries(TERMS[ui] || {})) if (new RegExp("(^|[^a-z])" + esc(a.toLowerCase()) + "([^a-z]|$)").test(lower)) hits.push([a, b]);
  return hits;
}
const clean = (h) => h.replace(/<br\s*\/?>/g, "\n").replace(/<[^>]+>/g, "").replace(/&#(\d+);/g, (_, n) => String.fromCharCode(+n)).replace(/&#x([0-9a-f]+);/gi, (_, n) => String.fromCharCode(parseInt(n, 16))).replace(/&amp;/g, "&").replace(/&quot;/g, '"').replace(/&#039;|&apos;/g, "'").replace(/&lt;/g, "<").replace(/&gt;/g, ">").replace(/&nbsp;/g, " ").replace(/[ \t]+/g, " ").trim();
const sptEn = await (await fetch("https://raw.githubusercontent.com/sp-tarkov/server/master/project/assets/database/locales/global/en.json")).json(), tasksEn = en.tasks;
const ids = [];let chars = 0;const keyMarks = {};
// A key is required when a sentence naming it says so ("is required", "you need", "requires"), unless the page says it
// is not needed, one of several keys will do, or there is another way in; a key the page never names is optional.
function keyNeed(body, name) {
  if (!name) return "optional";const low = body.toLowerCase();if (/only one of the keys|either (?:of the )?keys?|one of the (?:two |three )?keys/.test(low)) return "optional";
  const n = name.toLowerCase(), sentences = low.split(/(?<=[.!?])\s+|\n/).filter((s) => s.includes(n));
  if (!sentences.length) return "optional";
  const no = /optional|not (?:be )?(?:required|needed|necessary)|isn't (?:required|needed)|without (?:the |a |its |using )?(?:key|it)|not requiring|no key|alternatively|or (?:you can|by|through)/;
  const yes = /\b(?:is|are) (?:required|needed|necessary)\b|\brequires?\b|\bneed(?:s|ed)? (?:the|a|an|to)\b|\byou(?:'ll| will)? (?:need|must)\b|\bmust (?:have|use|unlock)\b/;
  if (sentences.some((s) => no.test(s))) return "optional";return sentences.some((s) => yes.test(s)) ? "required" : "optional";
}
for (const t of tasks.sort((a, b) => (a.minPlayerLevel || 0) - (b.minPlayerLevel || 0))) {
  if (!t.wikiLink) continue;const file = path.join(cache, t.id + ".html");let html;
  if (fs.existsSync(file)) html = fs.readFileSync(file, "utf8");
  else { const title = decodeURIComponent(t.wikiLink.split("/wiki/")[1]);
    const r = await fetch("https://escapefromtarkov.fandom.com/api.php?action=parse&format=json&formatversion=2&prop=text&redirects=1&page=" + encodeURIComponent(title), { headers: { "User-Agent": "Mozilla/5.0 BlueAsh" } });
    try { html = (await r.json()).parse.text; } catch { html = ""; }fs.writeFileSync(file, html);await new Promise((r) => setTimeout(r, 300)); }
  // One source: the Guide text (id), or the trader's briefing (id + "-b") when the game's text (SPT) for this quest
  // is missing or belongs to another quest since (an id reused): the first quote of the wiki's Dialogue section.
  const write = (key, text) => { if (!text) return;const src = crypto.createHash("sha256").update(text).digest("base64url").slice(0, 16);
    // Those languages get the other names as hints (the model may decline them); Traditional Chinese has its own map names.
    const g = {}, hints = {};for (const ui of Object.keys(LOC)) { const all = glossary(ui, text), fixed = (a, b) => !ENGLISH_ONLY.has(ui) || a === b || (TERMS[ui] && TERMS[ui][a] === b);
      g[ui] = all.filter(([a, b]) => fixed(a, b));if (ENGLISH_ONLY.has(ui)) hints[ui] = all.filter(([a, b]) => !fixed(a, b) && ui !== "zh-TW"); }
    fs.writeFileSync(path.join(out, "src", key + ".json"), JSON.stringify({ text, src, wiki: t.wikiLink, glossary: g, hints }));ids.push({ id: key, src });chars += text.length; };
  const norm = (x) => String(x || "").trim().toLowerCase().replace(/ - part 1$/, "");
  if (norm(sptEn[t.id + " name"]) !== norm(tasksEn[t.name])) { const d = html.indexOf('id="Dialogue"');if (d >= 0) { const q = html.slice(d).match(/class="custom-quote(?:&#95;|_){2}text"[^>]*>(.*?)<\/p>/s);if (q) write(t.id + "-b", clean(q[1]).slice(0, 4000)); } }
  // Keys tarkov.dev lists for the quest (neededKeys): required or optional, read from the wiki page's own words.
  // Only the Guide section counts (the objective list says "(Optional)" about other things).
  if ((t.neededKeys || []).length) { const g0 = html.indexOf('id="Guide"'), nav = html.indexOf("navbox");
    const body = g0 < 0 ? "" : clean(html.slice(g0, nav > g0 ? nav : html.length).replace(/<table.*?<\/table>/gs, "").replace(/<\/(p|li)>/g, "\n"));
    const marks = {};for (const nk of t.neededKeys) for (const k of nk.keys || []) { const id = typeof k === "string" ? k : k.id;marks[id] = keyNeed(body, en.items[id + " Name"] || ""); }
    keyMarks[t.id] = marks; }
  const start = html.indexOf('id="Guide"');if (start < 0) continue;let end = html.indexOf("<h2", start);const nav = html.indexOf("navbox", start);if (end < 0 || (nav >= 0 && nav < end)) end = nav;if (end < 0) end = html.length;
  const part = html.slice(start, end).replace(/<table.*?<\/table>|<ul class="gallery.*?<\/ul>/gs, "");const lines = [];
  for (const m of part.matchAll(/<(p|li)(?:\s[^>]*)?>(.*?)<\/\1>/gs)) { const x = clean(m[2]);if (x) lines.push((m[1] === "li" ? "• " : "") + x); }
  let text = lines.join("\n");if (text.length > 4000) text = text.slice(0, 4000).trimEnd() + "…";write(t.id, text);
}
fs.writeFileSync(path.join(out, "ids.json"), JSON.stringify(ids));
// {quest id: {key id: "required" | "optional"}} for the quest card's needed keys ("(optional)" beside a key).
fs.writeFileSync(path.join(out, "keys.json"), JSON.stringify(keyMarks));
const kc = Object.values(keyMarks).flatMap((m) => Object.values(m));console.log("keys: required", kc.filter((x) => x === "required").length, "optional", kc.filter((x) => x === "optional").length);
// Objective translations (source/guide-translate/objectives/<lang>.json, kept in git) are published with the guides.
const objDir = path.join(root, "source/guide-translate/objectives");fs.mkdirSync(path.join(out, "objectives"), { recursive: true });
for (const f of fs.existsSync(objDir) ? fs.readdirSync(objDir) : []) if (f.endsWith(".json")) fs.copyFileSync(path.join(objDir, f), path.join(out, "objectives", f));
console.log("quests", tasks.length, "with a guide", ids.length, "chars", chars);

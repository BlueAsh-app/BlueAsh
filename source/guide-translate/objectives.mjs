// Quest objectives that tarkov.dev has not translated (its tasks_<locale> keeps the English text or nothing), listed
// for translation: work/objectives-<lang>.json [{id, text, names:[[english, game name]]}] (names = the objective's
// items and maps in that language). Finished translations go to source/guide-translate/objectives/<lang>.json
// ({objective id: text}, kept in git; source/quest-guides.mjs publishes it); the app reads it for what tarkov.dev lacks.
// node source/guide-translate/objectives.mjs <lang ...>
import fs from "fs";import path from "path";import { fileURLToPath } from "url";
const here = path.dirname(fileURLToPath(import.meta.url));
const LOC = { ko: "ko", ja: "ja", "zh-CN": "zh", ru: "ru", de: "de", es: "es", fr: "fr", "pt-BR": "pt", it: "it", pl: "pl", tr: "tr", "zh-TW": "zh" };
const SCRIPT = { ko: /[가-힣]/, ja: /[぀-ヿ一-鿿]/, "zh-CN": /[一-鿿]/, "zh-TW": /[一-鿿]/, ru: /[Ѐ-ӿ]/ };
const get = async (u) => (await (await fetch(u)).json()).data;
const tasks = [];for (const mode of ["pve", "regular"]) for (const t of Object.values((await get("https://json.tarkov.dev/" + mode + "/tasks")).tasks)) if (!tasks.find((x) => x.id === t.id)) tasks.push(t);
const en = await get("https://json.tarkov.dev/regular/tasks_en"), itemsEn = await get("https://json.tarkov.dev/regular/items_en"), mapsEn = await get("https://json.tarkov.dev/regular/maps_en");
for (const lang of process.argv.slice(2)) {
  const l = LOC[lang];if (!l) { console.log("unknown " + lang);continue; }
  const loc = await get("https://json.tarkov.dev/regular/tasks_" + l), items = await get("https://json.tarkov.dev/regular/items_" + l), maps = await get("https://json.tarkov.dev/regular/maps_" + l);
  const done = (() => { try { return JSON.parse(fs.readFileSync(path.join(here, "objectives", lang + ".json"), "utf8")); } catch { return {}; } })();
  const seen = new Set(), out = [];
  for (const t of tasks) for (const o of t.objectives || []) {
    const key = o.description, english = en[key];if (!english || seen.has(o.id) || done[o.id]) continue;
    const local = loc[key];const has = local && local !== english && (!SCRIPT[lang] || SCRIPT[lang].test(local));if (has) continue;
    seen.add(o.id);const names = [];
    for (const it of [...(o.items || []), ...(o.item ? [o.item] : []), ...(o.markerItem ? [o.markerItem] : []), ...(o.containsAll || [])]) { const id = typeof it === "string" ? it : it?.id;const a = itemsEn[id + " Name"] || en[id + " Name"], b = items[id + " Name"] || loc[id + " Name"];if (a && b) names.push([a, b]); }
    for (const m of o.maps || []) { const a = mapsEn[m + " Name"], b = maps[m + " Name"];if (a && b) names.push([a, b]); }
    out.push({ id: o.id, text: english, names });
  }
  fs.mkdirSync(path.join(here, "work"), { recursive: true });fs.writeFileSync(path.join(here, "work", "objectives-" + lang + ".json"), JSON.stringify(out, null, 1));
  console.log(lang, out.length, "objectives without a translation");
}

// Work batches for translating the quest guides by hand or with an agent (Codex): source/guide-translate/CODEX.md.
// node source/guide-translate/export.mjs [lang ...]   (default: every language)
// Reads relay/public/quest-guide (source/quest-guides.mjs) and writes work/<lang>/batch-NN.json, 20 quests each:
// [{id, text, names:[[english, as shown in the game]], hints:[[english, game name to decline]]}]. Quests that already
// have a reviewed text on the server (by "review") are left out, so running it again only lists what is left.
import fs from "fs";import path from "path";import { fileURLToPath } from "url";
const here = path.dirname(fileURLToPath(import.meta.url)), root = path.resolve(here, "../..");
const src = path.join(root, "relay/public/quest-guide");
const ALL = ["ja", "zh-CN", "ru", "de", "es", "fr", "pt-BR", "it", "pl", "tr", "zh-TW", "ko"];
const langs = process.argv.slice(2).length ? process.argv.slice(2) : ALL;
const ids = JSON.parse(fs.readFileSync(path.join(src, "ids.json"), "utf8"));
const relay = "https://modepulse-relay.modepulse.workers.dev";
for (const lang of langs) {
  if (!ALL.includes(lang)) { console.log("unknown language " + lang);continue; }
  const dir = path.join(here, "work", lang);fs.mkdirSync(dir, { recursive: true });
  const todo = [];
  for (const { id } of ids) {
    const r = await fetch(`${relay}/quest/guide?id=${id}&lang=${encodeURIComponent(lang)}`).catch(() => null);
    if (r && r.status === 200 && (await r.json()).by === "review") continue;
    const s = JSON.parse(fs.readFileSync(path.join(src, "src", id + ".json"), "utf8"));
    todo.push({ id, text: s.text, names: (s.glossary || {})[lang] || [], hints: (s.hints || {})[lang] || [] });
  }
  for (const f of fs.readdirSync(dir)) if (/^batch-\d+\.json$/.test(f)) fs.rmSync(path.join(dir, f));
  for (let i = 0; i * 20 < todo.length; i++) fs.writeFileSync(path.join(dir, `batch-${String(i + 1).padStart(2, "0")}.json`), JSON.stringify(todo.slice(i * 20, i * 20 + 20), null, 1));
  console.log(lang, todo.length, "quests left,", Math.ceil(todo.length / 20), "batches");
}

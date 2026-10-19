// Checks finished translations and stores them on BlueAsh's server as reviewed texts (they replace the machine ones
// and the server's own batch skips them). node source/guide-translate/upload.mjs <lang> [batch-NN ...]
// Reads work/<lang>/batch-NN.out.json ([{id, text}]) against batch-NN.json; checks every quest is there once, the
// same number of lines, "• " on the same lines, the language's own script, names kept. Problems are listed and that
// quest is not sent. The owner key comes from relay/.dev.vars (STATS_KEY) and is never printed.
import fs from "fs";import path from "path";import { fileURLToPath } from "url";
const here = path.dirname(fileURLToPath(import.meta.url)), root = path.resolve(here, "../..");
const [lang, ...only] = process.argv.slice(2);if (!lang) { console.log("usage: upload.mjs <lang> [batch-NN ...]");process.exit(1); }
const key = (fs.readFileSync(path.join(root, "relay/.dev.vars"), "utf8").match(/^STATS_KEY\s*=\s*"?([^"\r\n]+)"?/m) || [])[1];
if (!key) { console.log("relay/.dev.vars has no STATS_KEY");process.exit(1); }
const SCRIPT = { ko: /[가-힣]/, ja: /[぀-ヿ]/, "zh-CN": /[一-鿿]/, "zh-TW": /[一-鿿]/, ru: /[Ѐ-ӿ]/ };
const dir = path.join(here, "work", lang);let sent = 0, bad = 0;
const batches = fs.readdirSync(dir).filter((f) => /^batch-\d+\.json$/.test(f) && (!only.length || only.includes(f.replace(".json", ""))));
for (const b of batches) {
  const outFile = path.join(dir, b.replace(".json", ".out.json"));if (!fs.existsSync(outFile)) { console.log(b, "no .out.json yet");continue; }
  const want = JSON.parse(fs.readFileSync(path.join(dir, b), "utf8"));let got;
  try { got = JSON.parse(fs.readFileSync(outFile, "utf8").replace(/^﻿/, "")); } catch (e) { console.log(b, "out file is not valid JSON:", e.message);bad += want.length;continue; }
  const byId = new Map();for (const g of Array.isArray(got) ? got : []) if (g && typeof g.id === "string") byId.set(g.id, g);
  for (const w of want) {
    const g = byId.get(w.id), problems = [];
    if (!g || typeof g.text !== "string" || !g.text.trim()) problems.push("missing");
    else {
      const a = w.text.split("\n"), t = g.text.trim().split("\n");
      if (a.length !== t.length) problems.push(`lines ${t.length} (English ${a.length})`);
      else a.forEach((l, i) => { if (l.startsWith("• ") !== t[i].startsWith("• ")) problems.push(`bullet on line ${i + 1}`); });
      if (SCRIPT[lang] && !SCRIPT[lang].test(g.text)) problems.push("not in the language's script");
      for (const [en, name] of w.names) if (en === name && w.text.includes(en) && !g.text.includes(name)) problems.push(`keep "${name}"`);
      if (/⟦|⟧/.test(g.text)) problems.push("brackets left");
    }
    if (problems.length) { bad++;console.log(`${b} ${w.id}: ${problems.join(", ")}`);continue; }
    const r = await fetch(`https://modepulse-relay.modepulse.workers.dev/quest/guide?key=${encodeURIComponent(key)}`, { method: "PUT", body: JSON.stringify({ id: w.id, lang, text: g.text.trim(), src: "manual" }) });
    if (r.status === 200) sent++;else { bad++;console.log(`${b} ${w.id}: server ${r.status}`); }
  }
}
console.log(`${lang}: ${sent} stored, ${bad} with problems`);

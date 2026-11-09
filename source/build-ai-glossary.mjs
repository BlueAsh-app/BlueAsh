// Game-name glossary for this PC's AI translator (companion LocalAi): every English name the quest guides use, with the
// name each language's version of the game shows (maps, traders, items, quests), plus the common words of
// quest-guides.mjs TERMS. Made from relay/public/quest-guide/src (made by quest-guides.mjs) into companion/ai-glossary.tsv
// (language, English, name). Run again after quest-guides.mjs:  node source/build-ai-glossary.mjs
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const src = path.join(root, "relay/public/quest-guide/src");
const terms = (() => { const s = fs.readFileSync(path.join(root, "source/quest-guides.mjs"), "utf8"); const a = s.indexOf("const TERMS = "), b = s.indexOf("\n// ", a);
  return Function("return " + s.slice(a + 14, b).trim().replace(/;$/, ""))(); })();
const by = {};
const add = (l, e, t) => { e = String(e).trim(); t = String(t).trim(); if (!e || !t || /[\t\n]/.test(e + t)) return; (by[l] ??= new Map()); if (!by[l].has(e)) by[l].set(e, t); };
for (const [l, m] of Object.entries(terms)) for (const [e, t] of Object.entries(m)) add(l, e, t);
for (const f of fs.readdirSync(src).sort()) { const j = JSON.parse(fs.readFileSync(path.join(src, f), "utf8"));
  for (const part of ["glossary", "hints"]) for (const [l, pairs] of Object.entries(j[part] || {})) for (const [e, t] of pairs) add(l, e, t); }
const lines = ["# language\tEnglish\tname in that language's game (made by source/build-ai-glossary.mjs)"];
for (const l of Object.keys(by).sort()) for (const [e, t] of [...by[l]].sort((x, y) => y[0].length - x[0].length || x[0].localeCompare(y[0]))) lines.push(l + "\t" + e + "\t" + t);
fs.writeFileSync(path.join(root, "companion/ai-glossary.tsv"), "﻿" + lines.join("\n") + "\n");
console.log(Object.entries(by).map(([l, m]) => l + " " + m.size).join(", "));

// Second pass over the patched OptiScaler menu sources: every string a player can see that prepare-optiscaler.mjs left
// unwrapped (tooltips, combo items, preset descriptions, printf readouts, the title-bar quips) is wrapped in
// blueash_locale::text(...), and the new strings are appended to source-strings.json for translation.
// Works on the whole file, because adjacent literals continue across lines ("a"\n "b") and the first pass wrapped such
// runs as one call. Left alone on purpose: log lines, config/ini names, comparisons, ImGui IDs ("##..."), identifiers in
// CAPS_WITH_UNDERSCORES, file names, and std::format format strings (compile-time formats).
//   node source/optiscaler-localization/wrap-all.mjs [--dry]
import fs from 'node:fs';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
const here=path.dirname(fileURLToPath(import.meta.url));
const root=path.resolve(here,'../bench/optiscaler-localization-work/OptiScaler');
const dry=process.argv.includes('--dry');
const files=['menu/menu_common.cpp','dlssnr/DlssNr_Menu.cpp'];
const skipLine=/LOG_|spdlog|GetPrivateProfile|WritePrivateProfile|#include|std::format\s*\(|_stricmp|strcmp|\.find\s*\(|starts_with|ends_with|PushID|GetID\s*\(|OpenPopup|BeginPopup|L"|u8"|R"|getenv|LoadLibrary|GetModuleHandle|GetProcAddress/;
const literal='"(?:\\\\.|[^"\\\\\\n])*"';
const run=new RegExp(`(?:${literal}\\s*)+`,'g'),wrappedCall=new RegExp(`blueash_locale::text\\(\\s*(?:${literal}\\s*)+\\)`,'g');
const visible=s=>/[A-Za-z]{2}/.test(s)&&!/^##/.test(s)&&!/^[A-Z0-9_]+$/.test(s)&&!/^[a-z0-9_.]+$/.test(s)&&!/\.(dll|exe|ini|json|log|txt|dat)$/i.test(s.trim());
const value=r=>[...r.matchAll(/"((?:\\.|[^"\\])*)"/g)].map(m=>JSON.parse('"'+m[1]+'"')).join('');
const strings=JSON.parse(fs.readFileSync(path.join(here,'source-strings.json'),'utf8'));const known=new Set(strings);let added=[],wrapped=0;
for(const file of files){const p=path.join(root,file);const text=fs.readFileSync(p,'utf8');
 // Masked copy: wrapped calls, comments and character literals blanked (same length, newlines kept) so offsets match.
 const blank=m=>m.replace(/[^\n]/g,' ');
 const masked=text.replace(wrappedCall,blank).replace(/'(?:\\.|[^'\\\n])'/g,blank);
 const lineStart=i=>text.lastIndexOf('\n',i-1)+1,lineOf=i=>text.slice(lineStart(i),(text.indexOf('\n',i)+1||text.length+1)-1);
 let out='',last=0;
 for(const m of masked.matchAll(run)){const at=m.index,r=text.slice(at,at+m[0].length).replace(/\s+$/,'');if(!r.startsWith('"'))continue;
  if(skipLine.test(lineOf(at))||/^\s*\/\//.test(lineOf(at)))continue;
  const before=masked.slice(Math.max(0,at-12),at).trimEnd(),after=masked.slice(at+r.length,at+r.length+4).trimStart();
  if(/(==|!=|case)$/.test(before)||/^(==|!=)/.test(after))continue; // a comparison keeps its literal
  let v;try{v=value(r);}catch{continue;}if(!visible(v))continue;
  out+=text.slice(last,at)+`blueash_locale::text(${r})`;last=at+r.length;wrapped++;if(!known.has(v)){known.add(v);added.push(v);}}
 if(!dry)fs.writeFileSync(p,out+text.slice(last));}
if(!dry)fs.writeFileSync(path.join(here,'source-strings.json'),JSON.stringify([...strings,...added],null,2)+'\n');
fs.writeFileSync(path.join(here,'new-strings.json'),JSON.stringify(added,null,2)+'\n');
console.log(`${dry?'would wrap':'wrapped'} ${wrapped} literals, ${added.length} new strings (total ${strings.length+added.length})`);

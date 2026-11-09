// Prepare a translated copy of the pinned OptiScaler sources; never touch the installed game.
import fs from 'node:fs';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
const here=path.dirname(fileURLToPath(import.meta.url));
const root=path.resolve(here,'../bench/optiscaler-localization-work/OptiScaler');
const langs=['ko','en','zh-CN','ja','ru','es','pt-BR','de','fr','it','pl','tr','zh-TW'];
const source=JSON.parse(fs.readFileSync(path.join(here,'source-strings.json'),'utf8'));
const rows=langs.map(l=>l==='en'?Object.fromEntries(source.map(s=>[s,s])):JSON.parse(fs.readFileSync(path.join(here,l+'.review.json'),'utf8')));
for(let i=0;i<langs.length;i++)for(const s of source)if(typeof rows[i][s]!=='string')throw Error(`missing ${langs[i]}: ${s}`);
// Hex-escaped UTF-8 bytes avoid reliance on the MSVC source-code page.
const literal=s=>'"'+[...Buffer.from(s,'utf8')].map(b=>'\\x'+b.toString(16).padStart(2,'0')).join('')+'"';
const table=source.map(s=>'{'+literal(s)+',{'+rows.map(r=>literal(r[s])).join(',')+'}},').join('\n');
const header=`#pragma once\n#include <windows.h>\n#include <string>\n#include <unordered_map>\nnamespace blueash_locale {\nstruct Row { const char* key; const char* values[13]; };\ninline const Row rows[]={\n${table}\n};\ninline int language(){static int lang=[](){wchar_t code[32]={};DWORD n=sizeof(code);if(RegGetValueW(HKEY_CURRENT_USER,L"Software\\\\ModeSwitch",L"Language",RRF_RT_REG_SZ,nullptr,code,&n)!=ERROR_SUCCESS)return 1;const wchar_t* names[]={L"ko",L"en",L"zh-CN",L"ja",L"ru",L"es",L"pt-BR",L"de",L"fr",L"it",L"pl",L"tr",L"zh-TW"};for(int i=0;i<13;i++)if(!_wcsicmp(code,names[i]))return i;return 1;}();return lang;}\ninline const char* text(const char* s){static const auto index=[](){std::unordered_map<std::string,const Row*> m;for(const auto& row:rows)m.emplace(row.key,&row);return m;}();auto i=index.find(s);return i==index.end()?s:i->second->values[language()];}\n}\n`;
fs.writeFileSync(path.join(root,'BlueAshText.h'),header);
const files=['menu/menu_common.cpp','dlssnr/DlssNr_Menu.cpp'];
const re=/(?:ImGui::(?:Checkbox|Button|SliderFloat|SliderInt|Combo|Selectable|TextUnformatted|TextDisabled|Text|BeginTabItem|CollapsingHeader|SeparatorText)|ScopedCollapsingHeader|HelpMarker|ShowHelpMarker)\s*\(\s*((?:"(?:\\.|[^"\\])*"\s*)+)/g;
for(const file of files){const p=path.join(root,file),before=fs.readFileSync(p,'utf8');if(before.includes('BlueAshText.h'))throw Error(`Already patched: ${file}`);let count=0;
 const after=before.replace(re,(match,expression)=>{let value;try{value=[...expression.matchAll(/"(?:\\.|[^"\\])*"/g)].map(x=>JSON.parse(x[0])).join('');}catch{return match;}if(!source.includes(value))return match;count++;return match.replace(expression,`blueash_locale::text(${expression.trim()})`);});
 fs.writeFileSync(p,after.replace('#include "pch.h"','#include "pch.h"\n#include <BlueAshText.h>'));
 console.log(`${file}: connected ${count} text expressions`);
}

// Writes BlueAshText.h (the translation table compiled into the OptiScaler menu) from source-strings.json and the
// <lang>.review.json files. A string not translated yet falls back to English, so a build never fails on a missing row;
// --strict fails instead (for release builds).
import fs from 'node:fs';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
const here=path.dirname(fileURLToPath(import.meta.url));
const root=path.resolve(here,'../bench/optiscaler-localization-work/OptiScaler');
const strict=process.argv.includes('--strict');
const langs=['ko','en','zh-CN','ja','ru','es','pt-BR','de','fr','it','pl','tr','zh-TW'];
const source=JSON.parse(fs.readFileSync(path.join(here,'source-strings.json'),'utf8'));
const rows=langs.map(l=>{if(l==='en')return {};const p=path.join(here,l+'.review.json');return fs.existsSync(p)?JSON.parse(fs.readFileSync(p,'utf8')):{};});
const missing={};for(let i=0;i<langs.length;i++){if(langs[i]==='en')continue;const n=source.filter(s=>typeof rows[i][s]!=='string').length;if(n)missing[langs[i]]=n;}
if(strict&&Object.keys(missing).length)throw Error('missing translations: '+JSON.stringify(missing));
// Hex-escaped UTF-8 bytes avoid reliance on the MSVC source-code page.
const literal=s=>'"'+[...Buffer.from(s,'utf8')].map(b=>'\\x'+b.toString(16).padStart(2,'0')).join('')+'"';
const table=source.map(s=>'{'+literal(s)+',{'+rows.map((r,i)=>literal(langs[i]==='en'||typeof r[s]!=='string'?s:r[s])).join(',')+'}},').join('\n');
const header=`#pragma once\n#include <windows.h>\n#include <string>\n#include <unordered_map>\nnamespace blueash_locale {\nstruct Row { const char* key; const char* values[13]; };\ninline const Row rows[]={\n${table}\n};\ninline int language(){static int lang=[](){wchar_t code[32]={};DWORD n=sizeof(code);if(RegGetValueW(HKEY_CURRENT_USER,L"Software\\\\ModeSwitch",L"Language",RRF_RT_REG_SZ,nullptr,code,&n)!=ERROR_SUCCESS)return 1;const wchar_t* names[]={L"ko",L"en",L"zh-CN",L"ja",L"ru",L"es",L"pt-BR",L"de",L"fr",L"it",L"pl",L"tr",L"zh-TW"};for(int i=0;i<13;i++)if(!_wcsicmp(code,names[i]))return i;return 1;}();return lang;}\ninline const char* text(const char* s){static const auto index=[](){std::unordered_map<std::string,const Row*> m;for(const auto& row:rows)m.emplace(row.key,&row);return m;}();auto i=index.find(s);return i==index.end()?s:i->second->values[language()];}\n}\n`;
fs.writeFileSync(path.join(root,'BlueAshText.h'),header);
console.log(`BlueAshText.h: ${source.length} strings`+(Object.keys(missing).length?`, English fallback: ${JSON.stringify(missing)}`:''));

import fs from 'node:fs';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
const root=path.dirname(fileURLToPath(import.meta.url));
const catalog=JSON.parse(fs.readFileSync(path.join(root,'i18n_native.json'),'utf8'));
const languages=['ko','en','zh-CN','ja','ru','es','pt-BR','de','fr','it','pl','tr','zh-TW'];
const errors=[];
const korean=/[\u1100-\u11ff\u3130-\u318f\uac00-\ud7a3]/;
for(const [key,entry] of Object.entries(catalog)){
 for(const code of languages)if(typeof entry[code]!=='string'||!entry[code])errors.push(`Missing ${code}: ${key}`);
 if(entry.ko!==key)errors.push(`Source mismatch: ${key}`);
 for(const code of languages.slice(1))if(korean.test(entry[code]))errors.push(`Korean fallback in ${code}: ${key}`);
 const formats=s=>s.match(/%(?:\d+\$)?[-+ #0]*(?:\d+)?(?:\.\d+)?(?:hh|ll|[hlLzjt])?[diuoxXfFeEgGaAcspn]/g)||[];
 for(const code of languages.slice(1))if(JSON.stringify(formats(entry.ko))!==JSON.stringify(formats(entry[code])))errors.push(`Printf placeholders changed in ${code}: ${key}`);
}
const quote=s=>'L'+JSON.stringify(s);
const header='#pragma once\n// Native UI catalog. Source of truth: i18n_native.json; order: ko, en, zh-CN, ja, ru, es, pt-BR, de, fr, it, pl, tr, zh-TW.\nnamespace i18n {\nstruct Entry {const wchar_t* values[13];};\ninline const Entry catalog[]={\n'+Object.values(catalog).map(e=>' {'+languages.map(k=>quote(e[k])).join(',')+'},').join('\n')+'\n};\n}\n';
const inventory=new Map();
const files=fs.readdirSync(root).filter(f=>(f==='ModeSwitch.cpp'||f.endsWith('.h'))&&!f.startsWith('i18n'));
const mapNames=new Set(['에란겔','미라마','태이고','데스턴','비켄디','론도','사녹','카라킨','파라모','헤이븐']);
for(const file of files){const source=fs.readFileSync(path.join(root,file),'utf8');const tokens=/\/\/[^\r\n]*|\/\*[\s\S]*?\*\/|L"(?:\\.|[^"\\])*"/g;let token;
 while((token=tokens.exec(source))){if(!token[0].startsWith('L"'))continue;let key;try{key=JSON.parse(token[0].slice(1).replace(/\\0/g,'\\u0000'));}catch{continue;}if(!korean.test(key))continue;
  let exclusion='';if(file==='pubg_board.h'&&(key==='시작'||key==='남은'))exclusion='Fixed Korean game OCR recognition token';
  else if((file==='ModeSwitch.cpp'||file==='lang_menu.h')&&key==='한국어')exclusion='Language autonym';
  else if(file==='profiles.h')exclusion='Profile display key translated at UI sinks';
  else if(file==='pubg_map.h'&&mapNames.has(key))exclusion='Map display key translated at overlay renderer';
  else if(/raid_(context|events|files|discovery)\.h|pubg_log\.h|tarkov_keys\.h/.test(file)&&!catalog[key])exclusion='Fixed game parser token';
  const localized=!!catalog[key];if(!localized&&!exclusion)errors.push(`Uncataloged UI literal in ${file}: ${key}`);
  if(!inventory.has(key))inventory.set(key,{key,files:[],localized,exclusion});const entry=inventory.get(key);if(!entry.files.includes(file))entry.files.push(file);if(exclusion&&!entry.exclusion)entry.exclusion=exclusion;
 }
}
const mapRoot=path.join(root,'..','maps','pubg');if(fs.existsSync(mapRoot))for(const file of fs.readdirSync(mapRoot).filter(f=>f.endsWith('.txt'))){for(const line of fs.readFileSync(path.join(mapRoot,file),'utf8').split(/\r?\n/)){if(line.startsWith('#'))continue;const name=line.split('\t')[3];if(name&&korean.test(name)&&!catalog[name])errors.push(`Uncataloged PUBG marker in ${file}: ${name}`);}}
const inventoryText=JSON.stringify([...inventory.values()],null,2)+'\n';
if(process.argv.includes('--write')){fs.writeFileSync(path.join(root,'i18n_native.h'),header);fs.writeFileSync(path.join(root,'i18n_inventory.json'),inventoryText);}
else if(fs.readFileSync(path.join(root,'i18n_native.h'),'utf8').replaceAll('\r\n','\n')!==header)errors.push('Generated native header differs from JSON; run node source/i18n_check.mjs --write');
if(errors.length){console.error(errors.join('\n'));process.exitCode=1;}else console.log(`PASS: ${Object.keys(catalog).length} complete native keys; ${inventory.size} source literals audited; printf placeholders and PUBG data covered`);

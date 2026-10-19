// Extra UI languages beyond the original five (ko, en, zh-CN, ja, ru).
// Their translations live in one table, source/i18n_extra.tsv (source text -> one column per extra language),
// and this script writes them into every catalog, each in its own format:
//   source/i18n_native.json (C++ UI), companion/Localization.tsv (helpers), companion/FandomText.tsv (map texts,
//   English source), price/PriceLanguage.cs (price worker), updater/UpdateLanguage.ps1, relay/language.js.
//   node source/i18n_extend.mjs --list    -> source/i18n_sources.tsv (every source text with its English, for translating)
//   node source/i18n_extend.mjs --write   -> merge i18n_extra.tsv into the catalogs (missing rows fall back to English)
import fs from 'node:fs';import path from 'node:path';import {fileURLToPath} from 'node:url';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..');
export const EXTRA=['es','pt-BR','de','fr','it','pl','tr','zh-TW'];
const file=(p)=>path.join(root,p);
const read=(p)=>fs.readFileSync(file(p),'utf8');
const BOM='\ufeff';
const write=(p,s,bom)=>fs.writeFileSync(file(p),(bom?BOM:'')+s.replace(/^\ufeff/,''),'utf8');
const esc=(s)=>s.replace(/\\/g,'\\\\').replace(/\t/g,'\\t').replace(/\r/g,'\\r').replace(/\n/g,'\\n');
const unesc=(s)=>s.replace(/\\(\\|t|r|n)/g,(m,c)=>c==='\\'?'\\':c==='t'?'\t':c==='r'?'\r':'\n');

// --- readers: [source, english] pairs from each catalog ---
function nativePairs(){const j=JSON.parse(read('source/i18n_native.json'));return Object.entries(j).map(([k,v])=>[k,v.en]);}
// Localization.tsv / FandomText.tsv cells keep "\n" written as a backslash pair; compare in that form.
function tsvRows(p){return read(p).replace(/^\ufeff/,'').split(/\r?\n/);}
function locPairs(){return tsvRows('companion/Localization.tsv').filter(l=>l&&!l.startsWith('#')).map(l=>l.split('\t')).filter(c=>c.length>=5).map(c=>[c[0],c[1]]);}
function fandomPairs(){return tsvRows('companion/FandomText.tsv').filter(l=>l&&!l.startsWith('#')).map(l=>l.split('\t')).filter(c=>c.length>=5).map(c=>[c[0],c[0]]);}
// C# string literal list inside add(...): "a","b",...
const csStr=/"((?:[^"\\]|\\.)*)"/g;
function csUnquote(s){return s.replace(/\\(u[0-9a-fA-F]{4}|.)/g,(m,c)=>c[0]==='u'&&c.length===5?String.fromCharCode(parseInt(c.slice(1),16)):c==='n'?'\n':c==='r'?'\r':c==='t'?'\t':c);}
function csQuote(s){return '"'+s.replace(/\\/g,'\\\\').replace(/"/g,'\\"').replace(/\r/g,'\\r').replace(/\n/g,'\\n').replace(/\t/g,'\\t')+'"';}
function priceCalls(){const s=read('price/PriceLanguage.cs');const out=[];const re=/^(\s*)add\((.*)\);\s*$/gm;let m;
 while((m=re.exec(s))){const args=[...m[2].matchAll(csStr)].map(a=>csUnquote(a[1]));out.push({line:m[0],indent:m[1],args});}return out;}
function pricePairs(){return priceCalls().map(c=>[c.args[0],c.args[3]]);} // order ko, zh-CN, ja, en, ru
// PowerShell single-quoted: 'it''s'
const psStr=/'((?:[^']|'')*)'/g;
function updateCalls(){const s=read('updater/UpdateLanguage.ps1');const out=[];const re=/^Add-UpdateWord (.*)$/gm;let m;
 while((m=re.exec(s))){const args=[...m[1].matchAll(psStr)].map(a=>a[1].replace(/''/g,"'"));out.push({line:m[0].replace(/\r$/,''),args});}return out;}
function updatePairs(){return updateCalls().map(c=>[c.args[0],c.args[3]]);} // ko, zh-CN, ja, en, ru
async function relayRows(){const mod=await import('file://'+file('relay/language.js').replace(/\\/g,'/')+'?t='+Date.now());return [...mod.CATALOG.values()];}

// --- the extra table ---
function extraTable(){const map=new Map();if(!fs.existsSync(file('source/i18n_extra.tsv')))return map;
 for(const l of tsvRows('source/i18n_extra.tsv')){if(!l||l.startsWith('#'))continue;const c=l.split('\t');const src=unesc(c[0]);const row={};EXTRA.forEach((code,i)=>{if(c[i+1])row[code]=unesc(c[i+1]);});map.set(src,row);}
 return map;}
const pick=(table,src,en,code)=>{const r=table.get(src);return r&&r[code]?r[code]:en;};

async function list(){
 const seen=new Map();const add=(src,en)=>{if(src&&!seen.has(src))seen.set(src,en);};
 for(const [s,e] of nativePairs())add(s,e);
 for(const [s,e] of locPairs())add(unesc(s),unesc(e));
 for(const [s,e] of fandomPairs())add(unesc(s),unesc(e));
 for(const [s,e] of pricePairs())add(s,e);
 for(const [s,e] of updatePairs())add(s,e);
 for(const r of await relayRows())add(r[0],r[1]);
 const done=extraTable();let n=0;const lines=['# id\tsource\tEnglish (rows already in i18n_extra.tsv are left out)'];
 for(const [s,e] of seen){if(done.has(s)&&EXTRA.every(c=>done.get(s)[c]))continue;lines.push(`${++n}\t${esc(s)}\t${esc(e)}`);}
 write('source/i18n_sources.tsv',lines.join('\n')+'\n',false);console.log(`${seen.size} source texts, ${n} still to translate -> source/i18n_sources.tsv`);
}

async function merge(){
 const t=extraTable();let missing=new Set();const tr=(src,en,code)=>{const r=t.get(src);if(!r||!r[code])missing.add(src);return pick(t,src,en,code);};
 // C++ native catalog
 const j=JSON.parse(read('source/i18n_native.json'));
 for(const [k,v] of Object.entries(j))for(const code of EXTRA)v[code]=tr(k,v.en,code);
 write('source/i18n_native.json',JSON.stringify(j,null,2)+'\n',false);
 // Localization.tsv: ko en zh ja ru + extras
 const loc=tsvRows('companion/Localization.tsv').map(l=>{if(!l)return l;if(l.startsWith('#')){return l.startsWith('# Korean source key')?l.split('\t').slice(0,5).concat(['Español','Português (Brasil)','Deutsch','Français','Italiano','Polski','Türkçe','繁體中文']).join('\t'):l;}
  const c=l.split('\t');if(c.length<5)return l;const base=c.slice(0,5);return base.concat(EXTRA.map(code=>esc(tr(unesc(c[0]),unesc(c[1]),code)))).join('\t');});
 write('companion/Localization.tsv',loc.join('\n'),true);
 // FandomText.tsv: en ko zh-CN ja ru + extras (English is the source)
 const fan=tsvRows('companion/FandomText.tsv').map(l=>{if(!l||l.startsWith('#'))return l;const c=l.split('\t');if(c.length<5)return l;return c.slice(0,5).concat(EXTRA.map(code=>esc(tr(unesc(c[0]),unesc(c[0]),code)))).join('\t');});
 write('companion/FandomText.tsv',fan.join('\n'),false);
 // PriceLanguage.cs
 let cs=read('price/PriceLanguage.cs');
 for(const c of priceCalls()){const base=c.args.slice(0,5);const extra=EXTRA.map(code=>tr(base[0],base[3],code));cs=cs.replace(c.line,c.indent+'add('+base.concat(extra).map(csQuote).join(',')+');');}
 write('price/PriceLanguage.cs',cs,false);
 // UpdateLanguage.ps1
 let ps=read('updater/UpdateLanguage.ps1');const q=(s)=>"'"+s.replace(/'/g,"''")+"'";
 for(const c of updateCalls()){const base=c.args.slice(0,5);ps=ps.replace(c.line,'Add-UpdateWord '+base.concat(EXTRA.map(code=>tr(base[0],base[3],code))).map(q).join(' '));}
 write('updater/UpdateLanguage.ps1',ps,true);
 // relay/language.js rows: ko en zh-CN ja ru + extras
 let js=read('relay/language.js');
 for(const r of await relayRows()){const base=r.slice(0,5);const oldLine=' '+JSON.stringify(r)+',';const newLine=' '+JSON.stringify(base.concat(EXTRA.map(code=>tr(base[0],base[1],code))))+',';
  if(js.includes(oldLine))js=js.replace(oldLine,newLine);else console.warn('relay row not found: '+base[0]);}
 write('relay/language.js',js,false);
 console.log(missing.size?`merged; ${missing.size} texts have no extra translation yet (English used)`:'merged; every text translated');
}
const arg=process.argv[2];if(arg==='--list')await list();else if(arg==='--write')await merge();else console.log('usage: node source/i18n_extend.mjs --list | --write');

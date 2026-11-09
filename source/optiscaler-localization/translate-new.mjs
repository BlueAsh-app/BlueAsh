// Translates the OptiScaler menu strings that have no <lang>.review.json row yet (new-strings.json from wrap-all.mjs, or any
// gap) with the large local model (Qwen3-30B-A3B in %LOCALAPPDATA%\ModeSwitch\ai\review), straight to reviewed quality.
// Every answer is checked: printf (%s %.2f …) and std::format ({}) placeholders, ImGui IDs (##…), \n line breaks,
// file names and product names must survive; a batch that fails is retried one string at a time. Resumable.
//   node source/optiscaler-localization/translate-new.mjs [--lang ko]
import fs from 'node:fs';
import path from 'node:path';
import os from 'node:os';
import {spawn} from 'node:child_process';
import {fileURLToPath} from 'node:url';
const here=path.dirname(fileURLToPath(import.meta.url));
const only=(()=>{const i=process.argv.indexOf('--lang');return i>0?process.argv[i+1]:'';})();
const ai=path.join(os.homedir(),'AppData/Local/ModeSwitch/ai');
const names={ko:'Korean','zh-CN':'Simplified Chinese',ja:'Japanese',ru:'Russian',es:'Spanish','pt-BR':'Brazilian Portuguese',de:'German',fr:'French',it:'Italian',pl:'Polish',tr:'Turkish','zh-TW':'Traditional Chinese (Taiwan)'};
const source=JSON.parse(fs.readFileSync(path.join(here,'source-strings.json'),'utf8'));
const terms=['LatencyFlex','OptiScaler','DLSS','DLSSG','DLSSD','XeSS','XeFG','XeLL','FSR','FFX','Vulkan','DirectX','DXVK','Reflex','AntiLag','nvngx','ReShade','RCAS','CAS','NIS','HDR','VRAM','GPU','CPU','FPS','FG','MFG','DLAA','TAA','OptiFG','Streamline','NVAPI','Nukem'];
const tokens=s=>[...(s.match(/%(?:\d+\$)?[-+#0 ]*\d*(?:\.\d+)?(?:ll|l|z|h)?[diouxXfFeEgGaAcsp%]|\{[^{}]*\}|##\S*/g)||[])];
const files=s=>s.match(/\b[\w.-]+\.(?:dll|exe|ini|json|log|txt|dat)\b/gi)||[];
const breaks=s=>(s.match(/\n/g)||[]).length;
function ok(src,tr){if(typeof tr!=='string'||!tr.trim())return false;if(JSON.stringify(tokens(src))!==JSON.stringify(tokens(tr)))return false;
 if(breaks(src)!==breaks(tr))return false;for(const f of files(src))if(!tr.includes(f))return false;for(const t of terms)if(new RegExp(`\\b${t}\\b`).test(src)&&!tr.includes(t))return false;
 if(/^\s/.test(src)!==/^\s/.test(tr)||/\s$/.test(src)!==/\s$/.test(tr))return false;return true;}
const base='http://127.0.0.1:51944';
async function health(){try{return (await fetch(base+'/health')).ok;}catch{return false;}}
let proc=null;
if(!await health()){proc=spawn(path.join(ai,'llama-b12065/llama-server.exe'),['-m',path.join(ai,'review/Qwen3-30B-A3B-Instruct-2507-Q4_K_M.gguf'),'-ngl','99','--n-cpu-moe','12','-c','8192','-np','1','-fa','on','--host','127.0.0.1','--port','51944','--no-webui'],{cwd:path.join(ai,'llama-b12065'),windowsHide:true,stdio:'ignore'});
 for(let i=0;i<600&&!await health();i++)await new Promise(r=>setTimeout(r,500));}
async function ask(lang,batch){const system=`You translate the in-game settings menu of OptiScaler (an upscaling / frame-generation mod) from English into ${names[lang]} for gamers. Translate the meaning naturally and concisely as UI text. Keep product and technical names in English (DLSS, XeSS, FSR, Reflex, OptiScaler, DXVK, GPU, VRAM, file names). Keep every placeholder exactly and in the same order: printf (%s, %d, %.2f, %6.1f, %%), {} and ##IDs. Keep the same number of line breaks (\\n) and any leading or trailing spaces. Short joke taglines: translate them as light, natural jokes in ${names[lang]}. Answer with only a JSON array of the translated strings, in the same order.`;
 const r=await fetch(base+'/v1/chat/completions',{method:'POST',headers:{'content-type':'application/json'},body:JSON.stringify({messages:[{role:'system',content:system},{role:'user',content:JSON.stringify(batch)}],temperature:0.15,max_tokens:4096})});
 if(!r.ok)return null;const t=(await r.json()).choices?.[0]?.message?.content||'';const a=t.indexOf('['),b=t.lastIndexOf(']');try{const out=JSON.parse(t.slice(a,b+1));return Array.isArray(out)&&out.length===batch.length?out:null;}catch{return null;}}
try{
 for(const lang of Object.keys(names)){if(only&&lang!==only)continue;const dest=path.join(here,lang+'.review.json');const rows=fs.existsSync(dest)?JSON.parse(fs.readFileSync(dest,'utf8')):{};
  const todo=source.filter(s=>typeof rows[s]!=='string');let fails=0;const t0=Date.now();
  for(let i=0;i<todo.length;){const batch=[];let n=0;while(i<todo.length&&batch.length<8&&(!batch.length||n+todo[i].length<1800)){n+=todo[i].length;batch.push(todo[i++]);}
   const got=await ask(lang,batch)||batch.map(()=>null);
   for(let k=0;k<batch.length;k++){let v=got[k];if(!ok(batch[k],v)){for(let tries=0;tries<2&&!ok(batch[k],v);tries++){const one=await ask(lang,[batch[k]]);v=one?one[0]:null;}}
    if(ok(batch[k],v))rows[batch[k]]=v;else fails++;}
   fs.writeFileSync(dest,JSON.stringify(rows,null,2)+'\n');}
  console.log(`${lang}: ${todo.length-fails}/${todo.length} translated, ${fails} left in English, ${Math.round((Date.now()-t0)/1000)} s`);}
}finally{if(proc)proc.kill();}

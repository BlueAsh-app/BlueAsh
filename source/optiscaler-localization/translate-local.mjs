import fs from 'node:fs';
import path from 'node:path';
import os from 'node:os';
import {spawn} from 'node:child_process';
import {fileURLToPath} from 'node:url';
const here=path.dirname(fileURLToPath(import.meta.url));
const project=path.resolve(here,'../bench/optiscaler-localization-work/OptiScaler');
const files=['menu/menu_common.cpp','dlssnr/DlssNr_Menu.cpp'];
const strings=new Set();
for(const f of files){
 const source=fs.readFileSync(path.join(project,f),'utf8');
 const re=/(?:ImGui::(?:Checkbox|Button|SliderFloat|SliderInt|Combo|Selectable|TextUnformatted|TextDisabled|Text|BeginTabItem|CollapsingHeader|SeparatorText)|ScopedCollapsingHeader|HelpMarker|ShowHelpMarker)\s*\(\s*((?:"(?:\\.|[^"\\])*"\s*)+)/g;
 for(const m of source.matchAll(re)){
  const pieces=[...m[1].matchAll(/"(?:\\.|[^"\\])*"/g)];let value='';
  try{value=pieces.map(x=>JSON.parse(x[0])).join('');}catch{continue;}
  if(/[a-z]{2}/i.test(value)&&!value.startsWith('##')&&value.length<=1500)strings.add(value);
 }
}
const input=[...strings];fs.writeFileSync(path.join(here,'source-strings.json'),JSON.stringify(input,null,2)+'\n');
const languages={ko:'Korean','zh-CN':'Simplified Chinese',ja:'Japanese',ru:'Russian',es:'Spanish','pt-BR':'Brazilian Portuguese',de:'German',fr:'French',it:'Italian',pl:'Polish',tr:'Turkish','zh-TW':'Traditional Chinese'};
const ai=path.join(os.homedir(),'AppData/Local/ModeSwitch/ai'),exe=path.join(ai,'llama-b12065/llama-server.exe'),model=path.join(ai,'Qwen3-4B-Instruct-2507-Q4_K_M.gguf');
const base='http://127.0.0.1:51942';
async function health(){try{return (await fetch(base+'/health')).ok;}catch{return false;}}
if(await health())throw Error('Port 51942 already in use; refusing to send data to an unknown server');
const proc=spawn(exe,['-m',model,'-ngl','99','-c','8192','-np','1','--host','127.0.0.1','--port','51942','--no-webui'],{cwd:path.dirname(exe),windowsHide:true,stdio:'ignore'});
try{
 let ready=false;for(let i=0;i<180;i++){if(await health()){ready=true;break;}if(proc.exitCode!==null)break;await new Promise(r=>setTimeout(r,1000));}if(!ready)throw Error('Local AI failed to start');
 console.log(`Local GPU model ready; ${input.length} source strings`);
 for(const [lang,name] of Object.entries(languages)){
  const dest=path.join(here,`${lang}.json`);const cache=fs.existsSync(dest)?JSON.parse(fs.readFileSync(dest,'utf8')):{};
  const todo=input.filter(s=>!cache[s]);
  const batchSize=process.env.BLUEASH_TRANSLATION_RETRY==='1'?1:8;
  for(let i=0;i<todo.length;i+=batchSize){const batch=todo.slice(i,i+batchSize);const prompt=`Translate these OptiScaler graphics settings from English into ${name}. Return only a JSON array with exactly ${batch.length} strings in the same order. Preserve format tokens such as %s, %d, %.2f, escaped line breaks, technical terms and UI identifiers after ##. Do not add explanations.`;
   const r=await fetch(base+'/v1/chat/completions',{method:'POST',headers:{'content-type':'application/json'},body:JSON.stringify({messages:[{role:'system',content:prompt},{role:'user',content:JSON.stringify(batch)}],temperature:0.1,max_tokens:3072})});
   if(!r.ok)throw Error(`AI HTTP ${r.status}`);const t=(await r.json()).choices?.[0]?.message?.content||'';const a=t.indexOf('['),b=t.lastIndexOf(']');let out;try{out=JSON.parse(t.slice(a,b+1));}catch{out=[];}
   if(!Array.isArray(out)||out.length!==batch.length){console.log(`${lang}: invalid batch at ${i}, skipped`);continue;}
   for(let k=0;k<batch.length;k++){const orig=batch[k],tr=out[k];if(typeof tr!=='string'||!tr.trim())continue;const tokens=s=>s.match(/%(?:\d+\$)?[-+#0]*\d*(?:\.\d+)?(?:ll|l|z|h)?[diouxXfFeEgGaAcsp%]|##[^\s]*/g)||[];if(JSON.stringify(tokens(orig))!==JSON.stringify(tokens(tr)))continue;cache[orig]=tr;}
   fs.writeFileSync(dest,JSON.stringify(cache,null,2)+'\n');console.log(`${lang}: ${Object.keys(cache).length}/${input.length}`);
  }
 }
}finally{proc.kill();}

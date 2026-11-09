import fs from 'node:fs';
import path from 'node:path';
import os from 'node:os';
import {spawn} from 'node:child_process';
import {fileURLToPath} from 'node:url';
const here=path.dirname(fileURLToPath(import.meta.url));
const ai=path.join(os.homedir(),'AppData/Local/ModeSwitch/ai');
const exe=path.join(ai,'llama-b12065/llama-server.exe');
const model=path.join(ai,'review/Qwen3-30B-A3B-Instruct-2507-Q4_K_M.gguf');
const names={ko:'Korean','zh-CN':'Simplified Chinese',ja:'Japanese',ru:'Russian',es:'Spanish','pt-BR':'Brazilian Portuguese',de:'German',fr:'French',it:'Italian',pl:'Polish',tr:'Turkish','zh-TW':'Traditional Chinese'};
const base='http://127.0.0.1:51943';
async function health(){try{return (await fetch(base+'/health')).ok;}catch{return false;}}
if(await health())throw Error('Review port already in use');
const log=fs.openSync(path.join(here,'large-model-run.log'),'a');
const proc=spawn(exe,['-m',model,'-ngl','35','-c','8192','-np','1','--host','127.0.0.1','--port','51943','--no-webui'],{cwd:path.dirname(exe),windowsHide:true,stdio:['ignore',log,log]});
const tokens=s=>s.match(/%(?:\d+\$)?[-+#0]*\d*(?:\.\d+)?(?:ll|l|z|h)?[diouxXfFeEgGaAcsp%]|##[^\s]*/g)||[];
try{
 let ready=false;for(let i=0;i<240;i++){if(await health()){ready=true;break;}if(proc.exitCode!==null)break;await new Promise(r=>setTimeout(r,1000));}if(!ready)throw Error('Large model failed to start');
 console.log('Qwen3-30B-A3B review model ready');
 for(const [lang,name] of Object.entries(names)){
  const input=JSON.parse(fs.readFileSync(path.join(here,lang+'.json'),'utf8'));const dest=path.join(here,lang+'.review.json');const result=fs.existsSync(dest)?JSON.parse(fs.readFileSync(dest,'utf8')):{};const todo=Object.entries(input).filter(([s])=>!(s in result));
  for(let i=0;i<todo.length;i+=5){let batch=todo.slice(i,i+5);const system=`Review English-to-${name} translations for OptiScaler graphics settings. Correct mistranslation, awkward wording and foreign script. Preserve brand and graphics terms such as DLSS, Reflex, upscaler, GPU, frame generation. Preserve printf placeholders (%s, %llu, %.2f) exactly and in order, and identifiers after ##. Return ONLY a JSON array of corrected translations in the same order, without explanations.`;
   const r=await fetch(base+'/v1/chat/completions',{method:'POST',headers:{'content-type':'application/json'},body:JSON.stringify({messages:[{role:'system',content:system},{role:'user',content:JSON.stringify(batch.map(([source,translation])=>({source,translation})))}],temperature:0.1,max_tokens:3072})});if(!r.ok)throw Error(`AI HTTP ${r.status}`);
   const t=(await r.json()).choices?.[0]?.message?.content||'';const a=t.indexOf('['),b=t.lastIndexOf(']');let out;try{out=JSON.parse(t.slice(a,b+1));}catch{out=[];}
   if(Array.isArray(out)&&out.length===batch.length)for(let k=0;k<batch.length;k++){const [src]=batch[k],item=out[k],v=typeof item==='string'?item:item?.translation;if(typeof item==='object'&&item?.source&&item.source!==src)continue;if(typeof v==='string'&&v.trim()&&JSON.stringify(tokens(src))===JSON.stringify(tokens(v)))result[src]=v;}
   fs.writeFileSync(dest,JSON.stringify(result,null,2)+'\n');console.log(`${lang}: ${Object.keys(result).length}/${Object.keys(input).length}`);
  }
 }
}finally{proc.kill();fs.closeSync(log);}

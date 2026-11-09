import fs from 'node:fs';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
const dir=path.dirname(fileURLToPath(import.meta.url));
const source=JSON.parse(fs.readFileSync(path.join(dir,'source-strings.json'),'utf8'));
const langs=['ko','zh-CN','ja','ru','es','pt-BR','de','fr','it','pl','tr','zh-TW'];
const printf=s=>s.match(/%(?:\d+\$)?[-+#0]*\d*(?:\.\d+)?(?:ll|l|z|h)?[diouxXfFeEgGaAcsp%]|##[^\s]*/g)||[];
const files=s=>s.match(/\b[a-zA-Z0-9_][a-zA-Z0-9_.-]*\.(?:dll|exe|ini|json|log|txt)\b/gi)||[];
const terms=['LatencyFlex','OptiScaler','DLSS','DLSSG','DLSSD','XeSS','FSR','FFX','Vulkan','DirectX','Reflex','nvngx','ReShade'];
let report=[];
for(const lang of langs){const p=path.join(dir,lang+'.review.json'),rows=fs.existsSync(p)?JSON.parse(fs.readFileSync(p,'utf8')):{};
 for(const s of source){const v=rows[s];if(typeof v!=='string'||!v.trim()){report.push({lang,problem:'missing',source:s});continue;}
  if(JSON.stringify(printf(s))!==JSON.stringify(printf(v)))report.push({lang,problem:'format',source:s,translation:v});
  for(const f of files(s))if(!v.includes(f))report.push({lang,problem:'filename',file:f,source:s,translation:v});
  for(const term of terms)if(s.includes(term)&&!v.includes(term))report.push({lang,problem:'technical-name',term,source:s,translation:v});
  if(lang==='ko'&&/[\u3400-\u9fff\u3040-\u30ff]/u.test(v))report.push({lang,problem:'unexpected-script',source:s,translation:v});
  if(lang==='ru'&&/[\u3400-\u9fff\u3040-\u30ff\uac00-\ud7a3]/u.test(v))report.push({lang,problem:'unexpected-script',source:s,translation:v});
 }
}
const counts={};for(const issue of report){let key=issue.lang+' '+issue.problem;counts[key]=(counts[key]||0)+1;}
fs.writeFileSync(path.join(dir,'review-audit.json'),JSON.stringify(report,null,2)+'\n');console.log(JSON.stringify(counts,null,2));console.log('Issues:',report.length);

// Builds relay/public/ocr: the PaddleOCR PP-OCRv5 text-line recognition models BlueAsh downloads on first use
// (one per game text language) and the ONNX Runtime that runs them. Nothing here is bundled into BlueAsh.exe.
//   node source/ocr-models.mjs     (downloads into %TEMP%\blueash-ocr-src when missing, then fills relay/public/ocr)
// Sources (official, pinned by SHA-256):
//   models  https://paddle-model-ecology.bj.bcebos.com/paddlex/official_inference_model/paddle3.0.0/<name>_onnx_infer.tar  (Apache-2.0)
//   runtime https://api.nuget.org/v3-flatcontainer/microsoft.ml.onnxruntime/1.22.0/microsoft.ml.onnxruntime.1.22.0.nupkg (MIT)
//   Visual C++ runtime DLLs (app-local, so users need no redistributable install): this PC's System32 copies.
// relay/public/ocr is git-ignored (about 75 MB): run this before deploying the relay from a fresh checkout,
// otherwise the deploy would remove the files from the server (release.ps1 checks for manifest.json).
import fs from 'node:fs';import path from 'node:path';import os from 'node:os';import crypto from 'node:crypto';import {execFileSync} from 'node:child_process';import {fileURLToPath} from 'node:url';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..');
const src=path.join(os.tmpdir(),'blueash-ocr-src');fs.mkdirSync(src,{recursive:true});
const out=path.join(root,'relay','public','ocr');fs.mkdirSync(out,{recursive:true});
const MODELS={
 'korean_PP-OCRv5_mobile_rec':'568ed8b43a260adc9f484d92105e425ea8cddf8ce16940c177bc12864cfb0eb0',
 'PP-OCRv5_mobile_rec':'f7e792bc836f36e7ef895ad47c426d75b0b75b1650caa6d63fe9418441ffba8c',
 'en_PP-OCRv5_mobile_rec':'4424e851309b291b00aab8191cd4314cefbd2d1b2381ff8994019d262fa95e28',
 'latin_PP-OCRv5_mobile_rec':'0fd8634124d871d25492311da077517ba3b4277ea67a0dec9e46ce537978c7cb',
 'eslav_PP-OCRv5_mobile_rec':'49e3d48598e6aed05eaa7c9b0a759ab4261d9021dff2d71849d4f1c95b79d30b',
};
const ORT={version:'1.22.0',sha256:null};
const sha=(f)=>crypto.createHash('sha256').update(fs.readFileSync(f)).digest('hex');
async function fetchTo(url,file,want){
 if(!fs.existsSync(file)){const r=await fetch(url);if(!r.ok)throw new Error(url+' '+r.status);fs.writeFileSync(file,Buffer.from(await r.arrayBuffer()));}
 if(want&&sha(file)!==want)throw new Error('SHA-256 mismatch: '+file);return file;}
// YAML list item scalars as PaddleX writes them: plain, 'single' ('' = '), "double" (JSON escapes).
function scalar(v){if(v.startsWith("'"))return v.slice(1,-1).replace(/''/g,"'");if(v.startsWith('"'))return JSON.parse(v);return v;}
function dictOf(yml){const lines=fs.readFileSync(yml,'utf8').split(/\r?\n/);const at=lines.findIndex(l=>/^\s*character_dict:\s*$/.test(l));if(at<0)throw new Error('no character_dict '+yml);
 const d=[];for(let i=at+1;i<lines.length;i++){const m=lines[i].match(/^  - (.*)$/);if(!m)break;d.push(scalar(m[1]));}return d;}
const manifest={runtime:{},models:{}};
const put=(rel,from)=>{const to=path.join(out,rel);fs.mkdirSync(path.dirname(to),{recursive:true});fs.copyFileSync(from,to);return {size:fs.statSync(to).size,sha256:sha(to)};};
for(const [name,want] of Object.entries(MODELS)){
 const tar=await fetchTo(`https://paddle-model-ecology.bj.bcebos.com/paddlex/official_inference_model/paddle3.0.0/${name}_onnx_infer.tar`,path.join(src,name+'.tar'),want);
 const dir=path.join(src,name);fs.mkdirSync(dir,{recursive:true});execFileSync(path.join(process.env.SystemRoot,'System32','tar.exe'),['-xf',tar,'-C',dir]);
 const inner=path.join(dir,name+'_onnx_infer');const dict=dictOf(path.join(inner,'inference.yml'));
 // One character per line, \n and \\ escaped (the dictionary has no line breaks, but stay safe).
 const dictFile=path.join(dir,'dict.txt');fs.writeFileSync(dictFile,dict.map(c=>c.replace(/\\/g,'\\\\').replace(/\n/g,'\\n')).join('\n')+'\n','utf8');
 manifest.models[name]={files:{'inference.onnx':put(`${name}/inference.onnx`,path.join(inner,'inference.onnx')),'dict.txt':put(`${name}/dict.txt`,dictFile)},characters:dict.length};
 console.log(name,dict.length,'characters');
}
const nupkg=await fetchTo(`https://api.nuget.org/v3-flatcontainer/microsoft.ml.onnxruntime/${ORT.version}/microsoft.ml.onnxruntime.${ORT.version}.nupkg`,path.join(src,`ort-${ORT.version}.nupkg`),null);
const ortDir=path.join(src,'ort');fs.mkdirSync(ortDir,{recursive:true});execFileSync(path.join(process.env.SystemRoot,'System32','tar.exe'),['-xf',nupkg,'-C',ortDir,'runtimes/win-x64/native/onnxruntime.dll','LICENSE','ThirdPartyNotices.txt']);
manifest.runtime['onnxruntime.dll']=put('runtime/onnxruntime.dll',path.join(ortDir,'runtimes/win-x64/native/onnxruntime.dll'));
for(const dll of ['msvcp140.dll','msvcp140_1.dll','vcruntime140.dll','vcruntime140_1.dll'])manifest.runtime[dll]=put('runtime/'+dll,path.join(process.env.SystemRoot,'System32',dll));
put('ONNXRUNTIME-LICENSE.txt',path.join(ortDir,'LICENSE'));put('ONNXRUNTIME-ThirdPartyNotices.txt',path.join(ortDir,'ThirdPartyNotices.txt'));
manifest.ortVersion=ORT.version;manifest.version=1;
fs.writeFileSync(path.join(out,'manifest.json'),JSON.stringify(manifest,null,1)+'\n');
let total=0;for(const f of Object.values(manifest.runtime))total+=f.size;for(const m of Object.values(manifest.models))for(const f of Object.values(m.files))total+=f.size;
console.log('relay/public/ocr ready,',(total/1048576).toFixed(1),'MB');

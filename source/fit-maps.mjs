// Checks and refits the game -> map image placement of every Tarkov map (quest, key and screenshot positions).
// Reference points: the Fandom interactive map's markers (containers, locked doors, stationary guns, exits, spawns)
// paired with the same kind of thing in tarkov.dev's map data (game x,y,z) by mutual nearest neighbour.
//   node source/fit-maps.mjs           report only (current maps/markers.json placement)
//   node source/fit-maps.mjs --write   also write maps/fit.json (affine + local correction per map, used over markers.json)
// Pages and data are cached in %TEMP%\blueash-fit (delete it to fetch again). Floors drawn side by side (The Lab) stay in
// maps/levels.json and are only checked here.
import fs from 'node:fs';import path from 'node:path';import {fileURLToPath} from 'node:url';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..');
const cache=path.join(process.env.TEMP||'.','blueash-fit');fs.mkdirSync(cache,{recursive:true});
const TITLES={sandbox:'Ground Zero',lighthouse:'Lighthouse',laboratory:'The Lab',rezervbase:'Reserve',labyrinth:'The Labyrinth',woods:'Woods',shoreline:'Shoreline',tarkovstreets:'Streets of Tarkov',icebreaker:'Icebreaker',interchange:'Interchange',bigmap:'Customs',terminal:'Terminal',factory4_day:'Factory'};
async function get(url,file){const f=path.join(cache,file);if(fs.existsSync(f))return fs.readFileSync(f,'utf8');const r=await fetch(url,{headers:{'user-agent':'BlueAsh map fit (https://modepulse-relay.modepulse.workers.dev)'}});if(!r.ok)throw new Error(url+' '+r.status);const t=await r.text();fs.writeFileSync(f,t);await new Promise(s=>setTimeout(s,500));return t;}
// tarkov.dev container name -> Fandom category
const KIND={'PC block':'container_pc','Drawer':'container_drawer','Technical supply crate':'container_crate','Ration supply crate':'container_crate','Medical supply crate':'container_crate','Dead Scav':'container_dead','Scav body':'container_dead','PMC body':'container_dead','Civilian body':'container_dead','Duffle bag':'container_duffle','Weapon box':'container_weapon','Wooden crate':'container_greencrate','Medbag':'container_medical','Wooden ammo box':'container_ammo','Toolbox':'container_tool','Grenade box':'container_grenade','Buried barrel cache':'container_stash','Ground cache':'container_stash','Jacket':'container_jacket','Safe':'container_safe','Bank safe':'container_safe','Medcase':'container_medcase','Cash register':'container_cash','Bank cash register':'container_cash'};
const data=JSON.parse(await get('https://json.tarkov.dev/regular/maps','tarkov-maps.json')).data,names=JSON.parse(await get('https://json.tarkov.dev/regular/maps_en','tarkov-maps_en.json')).data;
const shipped=JSON.parse(fs.readFileSync(path.join(root,'data/maps/markers.json'),'utf8').replace(/^﻿/,'')).maps;
const levels=JSON.parse(fs.readFileSync(path.join(root,'maps/levels.json'),'utf8'));
const affine=(t,x,z)=>[t[0]*x+t[1]*z+t[2],t[3]*x+t[4]*z+t[5]];
function correct(t,ctrl,sigma,x,z){const p=affine(t,x,z);if(!ctrl||ctrl.length<4||!(sigma>0))return p;let su=0,sv=0,sw=0;for(let i=0;i+3<ctrl.length;i+=4){const dx=ctrl[i]-x,dz=ctrl[i+1]-z,w=Math.exp(-(dx*dx+dz*dz)/(sigma*sigma));su+=w*ctrl[i+2];sv+=w*ctrl[i+3];sw+=w;}if(sw<1)sw=1;return [p[0]+su/sw,p[1]+sv/sw];}
function lsAffine(pairs){ // u = a x + b z + c, v = d x + e z + f
 const A=[[0,0,0],[0,0,0],[0,0,0]],bu=[0,0,0],bv=[0,0,0];for(const p of pairs){const r=[p.x,p.z,1];for(let i=0;i<3;i++){bu[i]+=r[i]*p.u;bv[i]+=r[i]*p.v;for(let j=0;j<3;j++)A[i][j]+=r[i]*r[j];}}
 const solve=(M,b)=>{const m=M.map((r,i)=>[...r,b[i]]);for(let i=0;i<3;i++){let k=i;for(let j=i+1;j<3;j++)if(Math.abs(m[j][i])>Math.abs(m[k][i]))k=j;[m[i],m[k]]=[m[k],m[i]];for(let j=0;j<3;j++)if(j!==i){const f=m[j][i]/m[i][i];for(let c=i;c<4;c++)m[j][c]-=f*m[i][c];}}return m.map((r,i)=>r[3]/r[i]);};
 const u=solve(A,bu),v=solve(A,bv);return [u[0],u[1],u[2],v[0],v[1],v[2]];}
const median=a=>{const s=[...a].sort((x,y)=>x-y);return s.length?s[s.length>>1]:NaN;},pct=(a,q)=>{const s=[...a].sort((x,y)=>x-y);return s.length?s[Math.min(s.length-1,Math.floor(s.length*q))]:NaN;};
// Maps drawn floor by floor side by side: floors by game height (upper bounds, lowest floor first). Each floor's
// drawing is found by voting: same-kind marker - point differences under the shared scale/rotation pile up at its offset.
const FLOORS={laboratory:[-1.5,3]};
// Floors drawn side by side on a page without containers (Factory): exits paired by name, one scale/rotation and an
// offset per floor; a floor without exits sits halfway between two that have them (the drawings are evenly spaced).
const NAMED_FLOORS={factory4_day:{bands:[-0.5,3,6.5],between:{2:[1,3]}}};
// Interchange's mall floors (insets beside the outdoor map, 'area'-limited entries in levels.json) were fitted once from
// named locked doors and exits and are kept as they are; only the outdoor part is checked here.
// Parts of a page drawn at another scale (insets): their markers are left out of the pairing.
const REGION={interchange:m=>m.u<0.52};
const levelOut={_comment:levels._comment};for(const [k,v] of Object.entries(levels))if(k!=='_comment')levelOut[k]=v;
const out={_comment:'Game x,z -> map image (normalised), refitted by source/fit-maps.mjs from Fandom markers paired with tarkov.dev positions (containers, locked doors, guns, exits, spawns). Used over markers.json\'s transform; maps/levels.json still wins for floors drawn side by side.'};
const report=[];
for(const [id,title] of Object.entries(TITLES)){
 const raw=JSON.parse(await get('https://escapefromtarkov.fandom.com/api.php?action=query&prop=revisions&rvprop=content&rvslots=main&format=json&titles='+encodeURIComponent('Map:'+title),'fandom-'+id+'.json'));
 const page=JSON.parse(Object.values(raw.query.pages)[0].revisions[0].slots.main['*']);const yx=page.coordinateOrder==='yx',bottom=page.origin==='bottom-left',hi=page.mapBounds[1],W=+hi[yx?1:0],H=+hi[yx?0:1];
 const fm=page.markers.map(m=>{const x=+m.position[yx?1:0],y=+m.position[yx?0:1];return {k:m.categoryId,u:x/W,v:bottom?1-y/H:y/H,n:m.popup?.title||''};}).filter(REGION[id]||(()=>true));
 const tm=Object.values(data.maps).find(m=>{let n=m.nameId.toLowerCase();if(n==='sandbox_high')n='sandbox';return n===id;});if(!tm){report.push(id+': no tarkov.dev map');continue;}
 const tp=[];for(const c of tm.lootContainers||[]){const k=KIND[names[c.lootContainer+' Name']];if(k)tp.push({k,...c.position});}
 for(const l of tm.locks||[])tp.push({k:'locked',...l.position});for(const g of tm.stationaryWeapons||[])tp.push({k:'stationarygun',...g.position});
 for(const e of [...(tm.extracts||[]),...(tm.transits||[])])tp.push({k:'exit',...e.position});for(const s of tm.spawns||[])if((s.categories||[]).includes('player')||(s.sides||[]).includes('pmc'))tp.push({k:'spawn_pmc',...s.position});
 const fk=m=>m.k.startsWith('exfil')?'exit':m.k;const sh=shipped[id];if(!sh||!sh.transform){report.push(id+': no shipped transform');continue;}
 const lv=levels[id];const cur=(p)=>{if(lv){for(const f of lv){if(f.below!==undefined&&p.y>=f.below)continue;return affine(f.t,p.x,p.z);}}return correct(sh.transform,sh.ctrl,sh.sigma,p.x,p.z);};
 // mutual nearest pairs of the same kind under a placement, within maxPx
 const pairUp=(place,maxPx,pts=tp)=>{const pairs=[];const byK={};for(const m of fm){(byK[fk(m)]??=[]).push(m);}
  const proj=pts.map(p=>({p,q:place(p)}));for(const [k,list] of Object.entries(byK)){const cand=proj.filter(o=>o.p.k===k);if(!cand.length)continue;
   for(const m of list){let best=null,bd=1e9;for(const o of cand){const d=Math.hypot((o.q[0]-m.u)*W,(o.q[1]-m.v)*H);if(d<bd){bd=d;best=o;}}if(!best||bd>maxPx)continue;
    let back=null,bb=1e9;for(const m2 of list){const d=Math.hypot((best.q[0]-m2.u)*W,(best.q[1]-m2.v)*H);if(d<bb){bb=d;back=m2;}}if(back!==m)continue;pairs.push({x:best.p.x,y:best.p.y,z:best.p.z,u:m.u,v:m.v,k,d:bd});}}
  return pairs;};
 const R=Math.max(W,H)*0.025; // pairing radius: 2.5 % of the image
 const before=pairUp(cur,R);const e0=before.map(p=>p.d);
 let line=`${id.padEnd(14)} image ${W}x${H}  pairs ${String(before.length).padStart(4)}  now: median ${median(e0).toFixed(0).padStart(3)} px  p80 ${pct(e0,0.8).toFixed(0).padStart(3)} px`;
 if(NAMED_FLOORS[id]){const cfg=NAMED_FLOORS[id],bands=cfg.bands,floorOf=y=>{let i=0;while(i<bands.length&&y>=bands[i])i++;return i;};
  const pts=[];for(const e of [...(tm.extracts||[]),...(tm.transits||[])]){const en=(names[e.name]||e.name||'').trim().toLowerCase();if(!en)continue;
   for(const m of fm){if(!m.k.startsWith('exfil')||m.n.trim().toLowerCase()!==en)continue;pts.push({x:e.position.x,z:e.position.z,u:m.u,v:m.v,f:floorOf(e.bottom??e.position.y),n:m.n});}}
  // dedupe: one pair per name and floor (the page repeats some exits)
  const seen=new Set(),use=pts.filter(q=>{const k=q.n+'|'+q.f+'|'+q.u.toFixed(3);if(seen.has(k))return false;seen.add(k);return true;});
  const nf=bands.length+1,used=[...new Set(use.map(q=>q.f))].sort();const n=4+2*used.length;const col=f=>4+2*used.indexOf(f);
  const A=Array.from({length:n},()=>Array(n).fill(0)),B=Array(n).fill(0);
  for(const q of use){for(const [r,val] of [[[q.x,q.z,0,0],q.u],[[0,0,q.x,q.z],q.v]]){const row=Array(n).fill(0);row[0]=r[0];row[1]=r[1];row[2]=r[2];row[3]=r[3];row[col(q.f)+(val===q.u?0:1)]=1;for(let a=0;a<n;a++){B[a]+=row[a]*val;for(let b=0;b<n;b++)A[a][b]+=row[a]*row[b];}}}
  for(let i=0;i<n;i++){let k=i;for(let j=i+1;j<n;j++)if(Math.abs(A[j][i])>Math.abs(A[k][i]))k=j;[A[i],A[k]]=[A[k],A[i]];[B[i],B[k]]=[B[k],B[i]];for(let j=0;j<n;j++)if(j!==i){const f=A[j][i]/A[i][i];for(let c=i;c<n;c++)A[j][c]-=f*A[i][c];B[j]-=f*B[i];}}
  const x=B.map((b,i)=>b/A[i][i]);const off={};for(const f of used)off[f]=[x[col(f)],x[col(f)+1]];
  for(const [f,[a,b]] of Object.entries(cfg.between))if(!off[f]&&off[a]&&off[b])off[f]=[(off[a][0]+off[b][0])/2,(off[a][1]+off[b][1])/2];
  const floors=[];for(let f=0;f<nf;f++){const o=off[f];floors.push(o?[x[0],x[1],o[0],x[2],x[3],o[1]]:null);}
  const res=use.map(q=>{const t=floors[q.f];const r=affine(t,q.x,q.z);return Math.hypot((r[0]-q.u)*W,(r[1]-q.v)*H);});
  const shipErr=use.map(q=>{const r=cur({x:q.x,y:0,z:q.z});return Math.hypot((r[0]-q.u)*W,(r[1]-q.v)*H);});
  line+='\n    named exits: '+use.length+' on floors '+used.join(',')+' — per-floor median '+median(res).toFixed(0)+' px (max '+Math.max(...res).toFixed(0)+'), shipped median '+median(shipErr).toFixed(0)+' px, max '+Math.max(...shipErr).toFixed(0)+' px';
  // check with the PMC/boss spawns: each nearest marker after placing it on its floor
  const sp=tp.filter(q=>q.k==='spawn_pmc');const nearest=sp.map(q=>{const t=floors[floorOf(q.y)];if(!t)return 1e9;const r=affine(t,q.x,q.z);let b=1e9;for(const m of fm)if(m.k.startsWith('spawn'))b=Math.min(b,Math.hypot((r[0]-m.u)*W,(r[1]-m.v)*H));return b;});
  line+='\n    spawns: nearest spawn marker median '+median(nearest).toFixed(0)+' px over '+sp.length;
  // the shipped single transform fits one floor and puts the others on the wrong drawing, so judge by the worst exit
  if(floors.every(Boolean)&&Math.max(...res)<0.02*W){levelOut[id]=floors.map((t,i)=>({...(i<bands.length?{below:bands[i]}:{}),t:t.map(v=>+v.toPrecision(10))}));line+=' -> use per-floor';}
  report.push(line);continue;}
 if(FLOORS[id]){const bands=FLOORS[id],floorOf=y=>{let i=0;while(i<bands.length&&y>=bands[i])i++;return i;};const L=lv?lv[0].t:sh.transform,floors=[],hold=[],held=[];
  for(let f=0;f<=bands.length;f++){const pts=tp.filter(p=>floorOf(p.y)===f);if(!pts.length){floors.push(lv?lv[f]?.t:null);continue;}
   // vote for the floor drawing's offset under the shared scale/rotation
   const bin=0.003,votes=new Map();for(const p of pts){const q0=[L[0]*p.x+L[1]*p.z,L[3]*p.x+L[4]*p.z];for(const m of fm){if(fk(m)!==p.k)continue;const key=Math.round((m.u-q0[0])/bin)+','+Math.round((m.v-q0[1])/bin);votes.set(key,(votes.get(key)||0)+1);}}
   let best=null,bs=-1;for(const key of votes.keys()){const [a,b]=key.split(',').map(Number);let sum=0;for(let i=-1;i<=1;i++)for(let j=-1;j<=1;j++)sum+=votes.get((a+i)+','+(b+j))||0;if(sum>bs){bs=sum;best=[a,b];}}
   let t=[L[0],L[1],best[0]*bin,L[3],L[4],best[1]*bin];let pairs=pairUp(q=>affine(t,q.x,q.z),R,pts);
   for(let it=0;it<3&&pairs.length>=4;it++){const du=median(pairs.map(q=>q.u-affine(t,q.x,q.z)[0])),dv=median(pairs.map(q=>q.v-affine(t,q.x,q.z)[1]));t=[t[0],t[1],t[2]+du,t[3],t[4],t[5]+dv];pairs=pairUp(q=>affine(t,q.x,q.z),R,pts);}
   if(pairs.length>=20){let tt=lsAffine(pairs);let pp=pairUp(q=>affine(tt,q.x,q.z),R,pts);if(pp.length>=pairs.length*0.9){t=tt;pairs=pp;}}
   pairs.forEach((q,i)=>(i%5===0?hold:held).push({...q,f}));floors.push(t);
   line+=`
    floor ${f} (y<${bands[f]??'∞'}): ${pts.length} points, ${pairs.length} pairs, votes ${bs}`;}
  const place=q=>affine(floors[floorOf(q.y)],q.x,q.z),err=(set,pl)=>median(set.map(q=>{const r=pl(q);return Math.hypot((r[0]-q.u)*W,(r[1]-q.v)*H);}));
  const now=err(hold,cur),neu=err(hold,place),better=neu<now;line+=`
    held-out: shipped ${now.toFixed(0)} px, per-floor ${neu.toFixed(0)} px -> ${better?'use per-floor':'keep'}`;
  if(better)levelOut[id]=floors.map((t,i)=>({...(i<bands.length?{below:bands[i]}:{}),t:t.map(v=>+v.toPrecision(10))}));report.push(line);continue;}
 if(before.length<12){report.push(line+'  (too few pairs, kept)');continue;}
 // refit: affine on pairs (outliers out), re-pair with it, then a local correction with sigma chosen by held-out pairs
 let t=lsAffine(before);let pairs=before;
 for(let it=0;it<3;it++){pairs=pairUp(p=>affine(t,p.x,p.z),R);const res=pairs.map(p=>{const q=affine(t,p.x,p.z);return Math.hypot((q[0]-p.u)*W,(q[1]-p.v)*H);});const cut=Math.max(3*median(res),R*0.3);const keep=pairs.filter((p,i)=>res[i]<=cut);if(keep.length<8)break;t=lsAffine(keep);pairs=keep;}
 const ctrlOf=(set)=>set.flatMap(p=>{const q=affine(t,p.x,p.z);return [p.x,p.z,p.u-q[0],p.v-q[1]];});
 const ext=Math.max(...pairs.map(p=>p.x))-Math.min(...pairs.map(p=>p.x))+Math.max(...pairs.map(p=>p.z))-Math.min(...pairs.map(p=>p.z));
 const hold=pairs.filter((_,i)=>i%5===0),train=pairs.filter((_,i)=>i%5!==0);let best={sigma:0,err:median(hold.map(p=>{const q=affine(t,p.x,p.z);return Math.hypot((q[0]-p.u)*W,(q[1]-p.v)*H);}))};const plain=best.err;
 for(const f of [0.01,0.02,0.035,0.05,0.08]){const s=ext/2*f,c=ctrlOf(train);const err=median(hold.map(p=>{const q=correct(t,c,s,p.x,p.z);return Math.hypot((q[0]-p.u)*W,(q[1]-p.v)*H);}));if(err<best.err)best={sigma:s,err};}
 // the shipped placement on the same held-out pairs, for a fair comparison
 const shippedHold=median(hold.map(p=>{const q=cur(p);return Math.hypot((q[0]-p.u)*W,(q[1]-p.v)*H);}));
 const ctrl=best.sigma>0?ctrlOf(pairs):[];const after=pairs.map(p=>{const q=correct(t,ctrl,best.sigma,p.x,p.z);return Math.hypot((q[0]-p.u)*W,(q[1]-p.v)*H);});
 const better=best.err<shippedHold;
 line+=`  | held-out: shipped ${shippedHold.toFixed(0)} px, affine ${plain.toFixed(0)} px, refit ${best.err.toFixed(0)} px${best.sigma?` (local, sigma ${best.sigma.toFixed(0)} m)`:''}  -> ${better?'use refit':'keep shipped'}  [${pairs.length} pairs, median ${median(after).toFixed(0)} px]`;
 if(better)out[id]={t:t.map(v=>+v.toPrecision(10)),ctrl:ctrl.map(v=>+v.toPrecision(8)),sigma:+best.sigma.toFixed(3),pairs:pairs.length,heldOutPx:+best.err.toFixed(1),shippedHeldOutPx:+shippedHold.toFixed(1),image:[W,H]};
 report.push(line);}
console.log(report.join('\n'));
if(process.argv.includes('--write')){fs.writeFileSync(path.join(root,'maps/levels.json'),JSON.stringify(levelOut,null,1));fs.writeFileSync(path.join(root,'maps/fit.json'),JSON.stringify(out)+'\n');console.log('maps/fit.json written:',Object.keys(out).filter(k=>k!=='_comment').join(', '));}

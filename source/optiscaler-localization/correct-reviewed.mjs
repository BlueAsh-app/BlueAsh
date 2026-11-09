import fs from 'node:fs';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
const dir=path.dirname(fileURLToPath(import.meta.url));
const values={ko:'화면 고정',ja:'フレームを固定',ru:'Зафиксировать кадр',es:'Congelar fotograma','pt-BR':'Congelar quadro',de:'Bild anhalten',fr:'Figer l’image',it:'Blocca il fotogramma',tr:'Kareyi dondur','zh-TW':'凍結畫面'};
for(const [lang,value] of Object.entries(values)){
 const p=path.join(dir,lang+'.review.json'),data=JSON.parse(fs.readFileSync(p,'utf8'));
 if(!Object.hasOwn(data,'Hold frame'))throw Error(`Missing ${lang} label`);
 data['Hold frame']=value;fs.writeFileSync(p,JSON.stringify(data,null,2)+'\n');
}
console.log(`Corrected ${Object.keys(values).length} hold-frame labels`);
const changes={
 ko:{'Force LatencyFlex':'LatencyFlex 강제 사용',"By default, FSR Anti-Lag 2.0/XeLL is used when available.\nThis setting lets you force LatencyFlex instead":'기본적으로 FSR Anti-Lag 2.0/XeLL을 사용할 수 있으면 사용합니다.\n이 설정으로 대신 LatencyFlex를 강제로 사용할 수 있습니다.'},
 es:{'Force LatencyFlex':'Forzar LatencyFlex',"By default, FSR Anti-Lag 2.0/XeLL is used when available.\nThis setting lets you force LatencyFlex instead":'Por defecto, se utiliza FSR Anti-Lag 2.0/XeLL cuando está disponible.\nEsta opción permite forzar LatencyFlex en su lugar.'},
 ja:{"Use Game's Reflex Markers":'ゲームのReflexマーカーを使用'},
 fr:{"Use Game's Reflex Markers":'Utiliser les marqueurs Reflex du jeu'},
 ru:{'Reflex timings, whole frame: %.1fms':'Задержка Reflex, весь кадр: %.1f мс'}
};
for(const [lang,rows] of Object.entries(changes)){
 const p=path.join(dir,lang+'.review.json'),data=JSON.parse(fs.readFileSync(p,'utf8'));
 for(const [key,value] of Object.entries(rows)){if(!Object.hasOwn(data,key))throw Error(`Missing ${lang}: ${key}`);data[key]=value;}
 if(lang==='ru')for(const key of Object.keys(data))if(key.startsWith('What the frame is divided by before the model sees it.'))data[key]=data[key].replace('Выход усилителя','Выход апскейлера').replace('линейный HDR-буфер игры','линейный HDR-буфер DLSS игры');
 fs.writeFileSync(p,JSON.stringify(data,null,2)+'\n');
}
console.log('Corrected product names and technical terminology');

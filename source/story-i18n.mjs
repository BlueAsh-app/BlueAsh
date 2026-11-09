// Merges hand-made story translations into companion/story.<lang>.tsv and the story UI rows of companion/Localization.tsv.
//   node source/story-i18n.mjs <lang> <file>
// <file>: one line per English source, in this order: every story line of companion/story.ko.tsv, then the story UI
// strings (STORY_UI below, Korean keys of Localization.tsv). Checked: line count,
// numbers and {n} placeholders equal to the English, so a shifted line is caught.
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
const root = path.join(path.dirname(fileURLToPath(import.meta.url)), '..');
const [lang, file] = process.argv.slice(2);
const COLS = ['ko', 'en', 'zh-CN', 'ja', 'ru', 'es', 'pt-BR', 'de', 'fr', 'it', 'pl', 'tr', 'zh-TW'];
const col = COLS.indexOf(lang); if (col < 2) throw new Error('language: ' + lang);
const STORY_UI = ['지도에서 위치 보기', '다음 목표', '진행 중', '미해금', '완료', '분기', '위치 미확인', '스토리 목표 {0}개', '스토리', '(스포일러 보호됨)', '층 표시 없음', '편집기 층 {0}', '위치 미확인 스토리 목표 {0}개',
  '● 클릭하면 목표를 골라 공략과 진행도를 봅니다.', '● 클릭하면 공략과 진행도를 봅니다.', '공략', '진행도', '분기·보상', '이 자리의 목표', '단계 {0} / {1}', '선택 목표', '완료로 표시', '완료 취소', '위치', '이 지도에는 표시할 위치가 없습니다.',
  '위치 데이터가 없습니다. 대략적인 지역만 안내합니다.', '층', '준비물·열쇠', '따로 확인된 준비물 데이터가 없습니다. 목표 문장의 아이템·열쇠를 확인하세요.', '완료 조건', '목표 문장대로 하면 완료됩니다. 게임 로그로는 확인할 수 없어 직접 체크합니다.', '챕터 시작 방법',
  '이전 단계', '다음 단계', '지도로 이동', '보기', '챕터 진행', '모든 챕터', '필요:', '이 챕터 데이터는 일부만 확인됐습니다. 하위 퀘스트 {0}/{1}개 확인, 문장 없는 목표 {2}개.', '스포일러', '숨김', '힌트', '전체 공개',
  '스포일러 보호 중입니다. 보려면 위에서 공개 수준을 고르세요.', '이 챕터에는 함께 끝낼 수 없는 선택이 {0}곳 있습니다. 한쪽을 끝내면 다른 쪽은 끝낼 수 없습니다.', '엔딩으로 갈라지는 길 {0}개가 있습니다.', '선택 {0}', '선택 A', '선택 B',
  '이 선택에 포함된 목표', '목표 {0}개', '목표 수', '맵', '생존·탈출 조건 있음', '제거 조건 있음', '아이템 전달·획득 있음', '열쇠·키카드 언급', '보스·위험 세력', '포기하게 되는 것', '다른 쪽 하위 퀘스트를 끝낼 수 없게 됩니다.', '되돌릴 수 있는지',
  '되돌릴 수 없습니다 (게임 데이터상 함께 완료 불가).', '보상', '미확인', '우호도·엔딩 영향', '난이도 요소 비교 (어느 쪽이 쉽다고 단정하지 않습니다)', '선택한 쪽', '엔딩', '목표가 확인된 길', '목표 데이터 없음', '출처 메모', '출처', '확인 날짜',
  '위키에서 챕터 보기', '진행도는 이 PC에만 저장되며 모드별로 따로 기록됩니다.', "보상·우호도·엔딩 영향은 확인된 데이터가 없으면 '미확인'으로 표시합니다.", 'PvP·PvE 공통 데이터로 공개돼 있습니다. 모드별 차이는 확인되지 않았습니다.', '챕터 보상', '시작 전', '잠김',
  '이 지도에서 이 목표의 위치를 찾지 못했습니다.', '일반 퀘스트', '스토리 퀘스트', '완료 숨기기', '미완료 챕터만', '분기 관련만', '챕터', '스포일러 보호', '필터'];

const read = (f) => fs.readFileSync(f, 'utf8').replace(/^﻿/, '');
const english = read(path.join(root, 'companion', 'story.ko.tsv')).split(/\r?\n/).filter((l) => l && !l.startsWith('#')).map((l) => l.split('\t')[0]);
const locPath = path.join(root, 'companion', 'Localization.tsv');
let loc = read(locPath); const nl = loc.includes('\r\n') ? '\r\n' : '\n';
const rows = loc.split(/\r?\n/); const byKey = new Map(rows.map((r, i) => [r.split('\t')[0], i]));
const keys = [...STORY_UI];
const sources = [...english, ...keys.map((k) => rows[byKey.get(k)].split('\t')[1])];
const lines = read(file).split(/\r?\n/).filter((l) => l.trim().length);
if (lines.length !== sources.length) throw new Error(`${lang}: ${lines.length} lines, expected ${sources.length}`);
const marks = (s) => (s.match(/\d+(\.\d+)?|\{\d\}/g) || []).sort().join(' ');
const bad = [];
lines.forEach((t, i) => { if (marks(t) !== marks(sources[i])) bad.push(`${i + 1}: ${sources[i]}  =>  ${t}`); });
if (bad.length) throw new Error(`${lang}: numbers differ\n` + bad.join('\n'));
fs.writeFileSync(path.join(root, 'companion', `story.${lang}.tsv`), '﻿# English story text (tarkov-data-overlay) <TAB> ' + lang + ', translated for BlueAsh\n' + english.map((e, i) => e + '\t' + lines[i]).join('\n') + '\n');
keys.forEach((k, j) => { const r = rows[byKey.get(k)].split('\t'); const v = lines[english.length + j]; r[col] = v; rows[byKey.get(k)] = r.join('\t'); });
fs.writeFileSync(locPath, String.fromCharCode(0xfeff) + rows.join(nl));
console.log(`${lang}: ${english.length} story lines, ${keys.length} UI rows`);

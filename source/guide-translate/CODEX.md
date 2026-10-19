# 퀘스트 공략 글 번역 작업 (Codex용)

BlueAsh 지도의 퀘스트 카드에 나오는 "위키 안내"(Escape from Tarkov 위키의 영어 공략 글)를 각 언어로 번역해
BlueAsh 서버에 올리는 작업이다. 서버의 무료 AI도 같은 일을 매분 조금씩 하고 있지만 몇 주 걸리고 품질이 낮다.
여기서 올린 번역은 "검수본"(by: review)으로 저장되어 자동 번역보다 우선하고, 서버는 그 퀘스트를 다시 번역하지 않는다.

## 순서

1. 남은 작업 묶음 만들기 (이미 검수본이 있는 퀘스트는 빠짐):
   ```
   node source/guide-translate/export.mjs ko ja zh-CN ru de es fr pt-BR it pl tr zh-TW
   ```
   → `source/guide-translate/work/<언어>/batch-01.json …` (묶음당 퀘스트 20개)
2. 묶음 하나씩: `batch-NN.json`을 읽고 같은 폴더에 `batch-NN.out.json`을 쓴다 (아래 형식·규칙).
3. 한 언어가 끝나면(또는 묶음 몇 개마다) 검사 후 올리기:
   ```
   node source/guide-translate/upload.mjs ja
   ```
   문제가 나온 퀘스트는 올라가지 않는다. 출력된 문제를 고쳐서 다시 실행한다(이미 올라간 것은 같은 내용으로 덮어써도 됨).
4. 사용량 한도로 중간에 멈췄다면 다음에 1번부터 다시: 올라간 퀘스트는 빠지고 남은 것만 묶음이 된다.

언어 우선순위: **ko**, ja, zh-CN, ru, de, es, fr, pt-BR, it, pl, tr, zh-TW. 한국어는 서버 자동 번역 품질이 낮아 Codex가 다시 한다(이미 검수본이 있는 퀘스트는 export가 뺀다).

## 입력 형식 (batch-NN.json)

```json
[{"id": "퀘스트 id", "text": "영어 원문 (줄바꿈 \n)", "names": [["English", "게임에 나오는 이름"]], "hints": [["English", "게임 이름 (활용형으로 바꿔도 됨)"]]}]
```

## 출력 형식 (batch-NN.out.json)

```json
[{"id": "퀘스트 id", "text": "번역문"}]
```
UTF-8, 입력의 모든 id를 한 번씩, 순서는 상관없음. JSON 안의 줄바꿈은 `\n`.

## 번역 규칙

- 대상: Escape from Tarkov 플레이어. 게이머가 쓰는 자연스러운 그 언어로, 직역하지 말 것. 존댓말/반말 등 문체는 그 언어 게임 공략의 일반적인 문체.
- **줄 수를 원문과 똑같이.** 원문 한 줄 = 번역 한 줄. 줄을 나누거나 합치지 않는다.
- 줄 맨 앞 `• `는 원문에 있는 줄에만 그대로 붙인다.
- `names`: 그 언어 게임 화면에 나오는 정확한 이름. 원문에 그 영어가 나오면 반드시 이 이름으로 쓴다. 이름이 영어 그대로인 항목(`["Machinery key","Machinery key"]`)은 번역하지 말고 영어 그대로 둔다.
- `hints`(러시아어·폴란드어·튀르키예어): 게임 이름이지만 문장에 맞게 격변화해도 된다.
- 번체 중국어(zh-TW)는 간체를 쓰지 않는다. 맵 이름: 海關, 森林, 海岸線, 立交橋, 儲備站, 燈塔, 塔科夫街區, 中心區, 工廠, 實驗室.
- 탈출구 이름(예: "ZB-1011", "Smugglers' Boat"), 무기·아이템 코드, 숫자, 좌표, 방 번호는 원문 그대로.
- 원문의 큰따옴표 인용("...")은 그 언어의 인용 표기로 바꿔도 되지만 안의 이름 규칙은 위와 같다.
- `⟦ ⟧` 같은 표시는 쓰지 않는다. 설명·주석·원문을 덧붙이지 않는다. 번역문만.
- 확실하지 않은 게임 용어는 그 언어판 타르코프에서 쓰는 말로(예: secure container, Scav, raid, extraction, flea market, stash, hideout).

## 주의

- `relay/.dev.vars`의 키(STATS_KEY)를 출력하거나 파일·커밋에 넣지 말 것. upload.mjs가 직접 읽는다.
- `work/` 폴더는 git에 넣지 않는다(.gitignore). 서버 배포(wrangler deploy)는 필요 없다. upload.mjs가 바로 저장한다.
- 끝나면 AGENTS.md 작업 로그에 언어별로 올린 개수를 한 줄 적는다.

## 퀘스트 목표 문장 (공략 글이 끝난 언어부터)

tarkov.dev에 번역이 없는 퀘스트 목표(약 300~560개, 짧은 문장)도 같은 방식으로 번역한다.

1. `node source/guide-translate/objectives.mjs <언어>` → `work/objectives-<언어>.json` ([{id, text, names}])
2. 번역 결과를 `source/guide-translate/objectives/<언어>.json`에 `{"목표 id": "번역문", ...}`로 저장 (git에 넣는다).
   기존 파일이 있으면 합친다(이미 있는 id는 그대로).
3. 형식은 그 언어 게임의 목표 문장처럼 짧은 명령형/명사형. 한국어 예: `세관(Customs)의 의료 연구실에 [주름진 호스] 숨기기`
   (맵 이름 뒤 괄호 영어, 아이템은 [ ], `names`의 게임 이름 사용).
4. `node source/quest-guides.mjs` 후 `relay` 폴더에서 `npx wrangler deploy` (목표 파일은 정적 파일로 배포됨).

## 참고

- 묶음에는 공략 글(퀘스트 id)과 퀘스트 설명(id 끝에 `-b`, 상인이 하는 말)이 섞여 있다. `-b`는 상인의 말투로(한국어는 게임처럼 하오체/반말 등 상인 말투 유지) 번역한다.

# RLOOP 規格與開發計畫

兩方機器人對戰競技場｜PvP / PvC / CvC｜C++ 核心 + DQN 訓練 + UE5 呈現

本文件包含：①遊戲規格 ②待確認事項 ③開發路線 ④GitHub 協作流程 ⑤Issue 清單。標示「建議」的內容是原始需求未明確指定、由本文件補上的預設值，可隨時調整。

## 1. 遊戲規格

### 1.1 模式與技術分層

- **PvP**：玩家對玩家
- **PvC**：玩家對 RL agent（開發初期先以 rule-based 教官代打，agent 訓練完成後替換）
- **CvC**：RL agent 對 RL agent
- 技術分層：
  - `core`（C++，不依賴 UE5）：模擬、物理、規則、observation、教官、replay 序列化
  - `bindings`（pybind11）：把環境 expose 給 Python
  - `training`（Python，全專案唯一使用 Python 的部分）：DQN、對手池、評測
  - `ue5`：渲染、玩家輸入、PvC 模型推論、replay 播放，直接呼叫 `core`
- 建議：`core` 可 headless 執行，訓練與 UE5 共用同一份規則與 observation 程式碼，避免兩套實作不一致

### 1.2 競技場

- 有界 2D 平面，邊界為牆
- 固定掩體（不可移動）與可移動掩體（只能平移、不可旋轉）各數個
- 場景由 config 描述（尺寸、掩體數量與位置、出生點），curriculum 只需調整 config
- 建議：訓練時雙方鏡像出生並加小幅隨機擾動；PvP 測試使用固定出生點

### 1.3 機器人

**屬性（依類型寫死於 config）**

- 最大生命值
- 單下攻擊傷害
- 攻擊冷卻
- 攻擊距離
- 移動速度
- 脫戰回血量
- 建議補充：碰撞半徑（角色屬性）；脫戰等待時間、推動減速係數（全域參數）

**戰鬥狀態（執行期）**

- 當前生命值
- 當前攻擊冷卻
- 內部另需「脫戰計時」

**角色定義**（「—」表示原始需求未指定；實際數值寫在 `robot_types` config，之後依對戰結果平衡）

| 角色 | 生命 | 單下傷害 | 冷卻 | DPS | 射程 | 移速 | 脫戰回血 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| tank | 高 | — | — | 高 | 中 | — | — |
| scout | — | 高 | 長 | 低 | 低 | 高 | 高 |
| ranged | 低 | — | — | 中 | 高 | — | — |

- tank：持續交戰幾乎無敵，鼓勵貼臉、積極進攻；需靠地形與走位逼對方進入近距離交戰
- scout：靠頻繁進場換血 → 脫戰回血取勝
- ranged：靠維持距離耗死對方

**Agent 組合**

- 相同角色不對戰，共 3 種對戰組合；每個組合雙方各訓練一個 agent，共 6 個模型，以（自己角色, 對手角色）命名：
  - `tank_vs_scout`、`scout_vs_tank`
  - `tank_vs_ranged`、`ranged_vs_tank`
  - `scout_vs_ranged`、`ranged_vs_scout`

### 1.4 動作空間（18）

- 移動 9 種：不動 + 八方位（N、NE、E、SE、S、SW、W、NW）
- 攻擊 2 種：否 / 是
- 建議編碼：`action = move_id * 2 + attack`
- 移動與攻擊可同時進行，攻擊後無僵直
- 斜向移動需正規化，確保各方向速度一致
- 冷卻中或超距時仍可發出攻擊指令，只是無效（不做 action masking，由 agent 自己學）

### 1.5 攻擊與傷害

- 雷射武器：自動瞄準（無朝向問題）、hitscan（無飛行時間）
- 攻擊指令發出時依序判定：
  1. 冷卻未完成 → 無效，回傳 `on_cooldown`
  2. 對手不在攻擊距離內 → 無傷害，回傳 `out_of_range`；renderer 仍朝對手方向畫出雷射
  3. 雙方連線穿過固定掩體 → 傷害 0，雷射在掩體前被擋下
  4. 連線穿過 n 個可移動掩體 → 傷害 = 單下傷害 × 0.25ⁿ（每層擋 75%，可疊加）；雷射畫面正常呈現
  5. 其餘 → 完整傷害
- 固定掩體優先：只要連線上有固定掩體，不論有幾個可移動掩體，傷害皆為 0
- 疊加範例：兩層可移動掩體 → 剩餘 0.25 × 0.25 = 0.0625（原文寫 0.625，應為筆誤，請確認）
- 每次攻擊回傳結果碼與實際傷害值，供 RL 使用：`hit`、`reduced`、`blocked`、`out_of_range`、`on_cooldown`

### 1.6 物理與移動

- 2D 物理引擎，固定時間步長（建議物理 60 Hz；RL 每 4 tick 決策一次，皆為 config）
- 可移動掩體鎖定旋轉，只能平移
- 機器人推力方向與其速度方向相同；推動過程與掩體之間不產生滑動
- 只能推、不能拉（離開接觸即無作用力）
- 推動減速：機器人對掩體有正向力時，移速乘上固定比例（全域參數）；不考慮力的方向與大小
- 建議：物理引擎選 Box2D（純 C++、輕量、可 headless），不綁定 UE5 的 Chaos，訓練端與 UE5 才能共用核心
- 建議：機器人之間有碰撞（圓形剛體）

### 1.7 脫戰回血

- 持續 T 秒沒有受到傷害、也沒有主動攻擊 → 開始持續回血（依「脫戰回血量」，到滿血為止）
- 受傷或主動攻擊 → 計時歸零並立即停止回血

### 1.8 勝負與回合

- 任一方生命 ≤ 0 即落敗；同 tick 雙亡為平手
- 設定回合時間上限（建議）；超時規則見第 2 節

### 1.9 RL 觀測空間

- 場地：兩張以自己為中心的 25×25 probe（二值）——可移動掩體圖、固定掩體圖
- 對手相對座標（dx, dy）與距離
- 對手各項數值現值（以當前生命、當前冷卻為主；靜態屬性因對手角色固定，等同常數）
- 視線資訊：
  - `static_blocked`：連線是否被固定掩體擋住（0/1）
  - `movable_cover_count`：連線穿過的可移動掩體數量（即忽略可移動掩體的視線）
  - 兩者合起來可推得預期傷害倍率（0 或 0.25ⁿ）
- 自己的戰鬥狀態：當前生命、當前攻擊冷卻
- 建議：數值正規化到 \[0,1\] 或 \[-1,1\]；probe 格距為 config 參數；場外格子視為固定掩體
- 建議網路：probe 走 CNN 分支、其餘向量走 MLP 分支，再合併
- 攻擊結果碼與傷害值放在 `info`，不進 observation，供 reward 使用
- 訓練困難時再加高階戰場指標（見 E9）

### 1.10 Reward（v0 草案）

- 終局：勝 +1 / 敗 −1 / 平 0
- 造成傷害為正、受到傷害為負（以最大生命正規化）
- 攻擊失敗（`out_of_range`）小幅懲罰
- 每步微小時間懲罰，避免龜縮
- 權重放 config，依訓練結果調整

### 1.11 訓練流程

1. 先做 PvC，並實作 rule-based 教官，確認教官具備基本對戰能力
2. 以教官為對手，訓練每個組合的 v1 agent（共 6 個）
3. CvC：同組合的兩個 agent 互打
4. 以對手池（教官 + 歷史版本）對戰，避免遺忘已學會的策略
5. Curriculum learning：逐步增加戰場複雜度
6. 若難以訓練，再於 observation 加入高階戰場指標

- 演算法：DQN（建議先以基本版跑通，再視需要加 Double / Dueling / PER）

### 1.12 Agent 對戰比較（後端功能）

- Match runner：輸入（A, B, 場景, 場數, seeds），輸出對戰統計
- 指標：勝率、平均存活時間、剩餘生命、傷害輸出、命中率與空擊率、掩體吸收傷害
- 交叉對戰矩陣（round-robin）與 Elo
- Model registry：版本、訓練設定、observation 版本

### 1.13 Replay 與模型交付

- 對戰結果保存為 replay（config + seed + 每 tick 的 action 序列 + 事件），UE5 讀取後重播
- 模型匯出為 ONNX（建議），UE5 以 C++ 端推論（如 UE NNE 或 ONNX Runtime，需技術驗證）
- observation 由同一份 C++ core 產生，確保訓練與遊戲內一致

## 2. 待確認事項

以下是原始需求中模糊或有衝突的地方，每項附建議預設，確認後再定案。

1. **疊加算式**：兩層可移動掩體應為 0.25 × 0.25 = 0.0625，請確認。
2. **空擊與冷卻中的攻擊**：是否消耗冷卻、是否重置脫戰計時？建議：不消耗、不重置，只回傳失敗碼；若希望空擊有代價，可改為消耗冷卻。
3. **「主動攻擊」的定義**：是否包含被固定掩體完全擋下的攻擊？建議：包含（只要成功發射就算）。
4. **距離與遮擋基準**：建議以中心點對中心點計算攻擊距離，連線與掩體矩形相交即視為被擋。
5. **機器人互撞**：是否互相阻擋？建議：會（圓形剛體）。
6. **超時規則**：建議以剩餘生命比例判勝負，差距過小判平手。
7. **脫戰等待時間、推動減速係數**：建議為全域參數；回血以每 tick 連續累加。
8. **probe 視野**：25 格 × 格距就是視野範圍，建議至少涵蓋近戰角色的交戰距離，並可在 config 調整。
9. **可移動掩體的質量與阻尼**：需實測調整手感；場景生成器須避免掩體卡死出生點或通道。

## 3. 開發路線

依你規劃的順序：後端 → PvP 測試 → RL 串接 → PvC 與教官 → 教官訓練 v1 → CvC → 歷史版本對打 → curriculum。

| 里程碑 | 項目 | 完成標準 | 前置 |
| --- | --- | --- | --- |
| M0 | E0 專案基礎建設 | repo、CI、協作規範就緒 | — |
| M1 | E1 遊戲核心後端 | 可 headless 完整模擬一場對戰 | M0 |
| M2 | E2 PvP 測試模組 | 兩人可手動對戰並驗證全部規則 | M1 |
| M3 | E3 RL 環境串接 | Python 可 reset / step，observation 與 reward 正確 | M1 |
| M4 | E4 PvC 與 rule-based 教官 | 玩家可對戰三種角色的教官 | M2、M3 |
| M5 | E5 DQN 訓練 v1 | 6 個 agent 可穩定贏過基本對手 | M3、M4 |
| M6 | E6 CvC 與對戰比較系統 | 任意兩個 agent 或教官可批次對戰並出報表 | M5 |
| M7 | E7 對手池與防遺忘 | 新版本不輸給舊版本與教官 | M6 |
| M8 | E8 Curriculum Learning | agent 能通過完整複雜度的場景 | M7 |
| M9 | E9 Observation 強化 | 條件式：訓練遇到瓶頸時才啟動 | M5 起視需要 |
| M10 | E10 UE5 整合 | UE5 內可 PvP / PvC / CvC 與 replay | E1 API 穩定後可平行，最後驗收 |

- 兩人分工建議：M1 完成後，E2（viewer 與輸入）與 E3（環境與 pybind）可並行
- 優先級：P0 = 主線必做；P1 = 重要但可稍後；P2 = 條件式或優化

## 4. GitHub 協作流程

### 分支

- `main`：穩定版本；只接受從 `develop` 來的 release PR，並打 tag（如 `v0.1-pvp`）
- `develop`：日常整合分支，所有 feature 合併到這裡
- `feature/<issue編號>-<簡短名稱>`：從 `develop` 切出，一個 issue 一條分支，合併後刪除（例：`feature/12-laser-damage`）

### 流程

1. 重要功能先建 issue（掛 Epic、labels、milestone）
2. 從 `develop` 切出 `feature/*`
3. 小步提交：一個 commit 只做一件事，盡量可編譯、測試通過
4. 推送並開 PR 到 `develop`，描述寫 `Closes #編號`，附測試結果或截圖
5. 對方 review：至少 1 人 approve、CI 通過、討論皆 resolved 後才合併
6. 里程碑完成後，開 `develop` → `main` 的 release PR 並打 tag

### Commit 訊息

- 格式：`<type>: <做了什麼> (#issue編號)`
- `feat:` 新功能｜`fix:` 修 bug｜`test:` 測試｜`docs:` 文件
- 範例：
  - `feat: add laser hit resolution against static cover (#12)`
  - `test: add stacked movable cover damage cases (#12)`
  - `fix: reset out-of-combat timer when attack is fired (#15)`
  - `docs: add manual test checklist for PvP (#20)`
- 之後若需要，可再擴充 `refactor:`、`chore:`

### Labels 與 Milestone

- type：`feat` `fix` `test` `docs`（對應 commit 前綴）
- area：`infra` `core` `physics` `combat` `pvp` `env` `teacher` `rl` `eval` `curriculum` `ue5`
- priority：`P0` 主線必做、`P1` 重要、`P2` 條件式或優化
- `epic`：Epic issue 專用
- Milestone：M0 \~ M10，對應第 3 節

### 注意事項（建議）

- `Closes #N` 只有在 PR 合併到「預設分支」時才會自動關閉 issue。若預設分支是 `main`，合併到 `develop` 不會關閉 → 建議把 repo 預設分支設為 `develop`，或手動關閉
- 為了保留小步提交的完整紀錄，PR 以 merge commit（或 rebase merge）合併，避免 squash
- `main`、`develop` 禁止直接 push，需 PR + 1 位 reviewer + CI 通過
- 模型權重、replay 檔、UE5 二進位資產走 Git LFS 或 repo 外儲存；repo 只放 registry metadata
- Epic issue 內用 task list（`- [ ] #編號`）追蹤子 issue；建立子 issue 後，把清單中的 `E1-1` 換成實際編號

### Issue 範本

```
## 目標

## 待辦
- [ ] 

## 驗收標準

## 相關（Epic / 相依 issue）
```

## 5. Issue 清單

每個 `E*-*` 是一個 GitHub issue，標題格式為 `type: 標題`（如 `feat: 雷射攻擊與傷害結算`）；每個 `E*` 是 Epic issue（label `epic`，標題 `[Epic] 名稱`），內文寫目標與子 issue 的 task list。勾選項就是 issue 內的待辦。共 11 個 Epic、46 個子 issue。

### E0 專案基礎建設｜M0｜`infra`

目標：repo、規範與 CI 就緒，之後每個 PR 都有自動檢查。

**E0-1｜docs: 協作規範與 repo 設定** · `infra` `P0`

- [ ] 建立 `main` / `develop`，設定 branch protection 與預設分支
- [ ] `CONTRIBUTING.md`（分支、commit、PR 規則）、PR 與 Issue 範本
- [ ] 建立 labels 與 milestones（M0–M10）
- [ ] 本規格存入 `docs/spec.md`

**E0-2｜feat: 專案骨架與 CI** · `infra` `P0`

- [ ] 目錄結構：`core/`、`bindings/`、`training/`、`ue5/`、`docs/`
- [ ] CMake 與依賴（物理引擎、pybind11、C++ 測試框架）
- [ ] CI：PR 時自動 build 並跑測試；Python 端 lint

### E1 遊戲核心後端（C++）｜M1｜`core`

目標：不依賴 UE5，可 headless 完整模擬一場對戰。每個 issue 都附單元測試（`test:` commit）。

**E1-1｜feat: 模擬核心骨架** · `core` `P0`

- [ ] 固定步長的 `World`，提供 `reset(seed, config)` 與 `step(actions)`
- [ ] 確定性：同 seed、同 action 序列 → 同結果
- [ ] test: 確定性回歸測試

**E1-2｜feat: 角色與全域參數 config** · `core` `P0`

- [ ] tank / scout / ranged 屬性（JSON 或 YAML 載入）
- [ ] 全域參數：脫戰等待時間、推動減速係數、可移動掩體抵擋比例 0.75、碰撞半徑
- [ ] test: 載入與驗證（缺漏、非法值）

**E1-3｜feat: 競技場與掩體** · `core` `P0`

- [ ] 有界場地與牆
- [ ] 固定與可移動掩體資料結構，arena config 載入
- [ ] 出生點設定（固定 / 鏡像隨機）

**E1-4｜feat: 2D 物理整合** · `physics` `P0`

- [ ] 機器人剛體、掩體剛體（鎖定旋轉）、牆
- [ ] 推力方向 = 速度方向；推動無滑動；只推不拉
- [ ] 機器人互撞
- [ ] test: 推動、卡牆、卡角落案例

**E1-5｜feat: 動作解碼與移動** · `core` `P0`

- [ ] 18 個 action 解碼（`move_id * 2 + attack`）
- [ ] 八方位速度正規化
- [ ] 推動減速（對掩體有正向力 → 移速 × 係數）
- [ ] test: 各方向速度、推動時速度比

**E1-6｜feat: 雷射攻擊與傷害結算** · `combat` `P0`

- [ ] 冷卻與攻擊距離判定
- [ ] 連線與掩體相交：固定掩體全擋、可移動掩體 × 0.25ⁿ
- [ ] 結果碼與實際傷害；雷射線段（含被擋截斷點、空擊朝向對手）
- [ ] test: 單層、兩層（0.0625）、固定 + 可移動、空擊、冷卻中

**E1-7｜feat: 脫戰回血與勝負判定** · `core` `P0`

- [ ] 脫戰計時與持續回血；受傷、攻擊時重置
- [ ] 勝負、平手、超時規則；episode 結束原因
- [ ] test: 回血時機與上限、同 tick 雙亡

**E1-8｜feat: 事件紀錄與 replay 序列化** · `core` `P1`

- [ ] 每 tick 事件（攻擊、命中、被擋、死亡）
- [ ] replay 格式（config + seed + action 序列）與重播
- [ ] test: 重播結果與原對戰逐 tick 一致

### E2 PvP 測試模組｜M2｜`pvp`

目標：兩人可手動對戰，逐項驗證核心規則。

**E2-1｜feat: 控制器介面與鍵盤輸入** · `pvp` `P0`

- [ ] `IController`：輸出 18 個 action 之一；之後教官與 agent 共用
- [ ] 雙人鍵盤映射（移動 + 攻擊）

**E2-2｜feat: debug viewer** · `pvp` `P0`

- [ ] 繪製場地、掩體、機器人、血條與冷卻
- [ ] 雷射：被固定掩體截斷、空擊朝向對手
- [ ] debug 疊圖：攻擊距離圈、視線、probe 範圍
- [ ] 建議：先用輕量 viewer（如 SDL2 / raylib），之後由 UE5 取代

**E2-3｜feat: PvP 模式與手動驗證** · `pvp` `P0`

- [ ] 局外選擇機器人類型 → 對戰 → 勝負畫面
- [ ] docs: 手動測試清單（掩體疊加、推動、脫戰回血、空擊…），逐項驗證並修正

### E3 RL 環境串接｜M3｜`env`

目標：Python 端可 reset / step，並取得正確的 observation 與 reward。

**E3-1｜feat: observation builder** · `env` `P0`

- [ ] 兩張 25×25 probe（格距可設，場外視為固定掩體）
- [ ] 對手相對座標、距離、對手數值現值
- [ ] `static_blocked`、`movable_cover_count`
- [ ] 自身生命與冷卻；正規化
- [ ] test: 已知場景的 probe 與特徵值

**E3-2｜feat: pybind11 綁定** · `env` `P0`

- [ ] expose `reset` / `step` / observation / info
- [ ] numpy 零拷貝或低成本傳遞
- [ ] 回傳 `obs_version`

**E3-3｜feat: Gym 風格 wrapper 與向量化環境** · `env` `P0`

- [ ] 單 agent 視角；對手由 `IController` 注入（教官 / agent / 隨機）
- [ ] 決策頻率（frame skip）
- [ ] 多環境並行

**E3-4｜feat: reward 與 episode 設定（v0）** · `env` `P0`

- [ ] 終局、傷害差、空擊懲罰、時間懲罰
- [ ] 權重 config 化

**E3-5｜test: 環境驗證與效能基準** · `env` `P1`

- [ ] 隨機策略連跑 N 局無 crash
- [ ] 同 seed 結果一致
- [ ] steps/sec 基準
- [ ] probe 與 viewer 對照目視確認

### E4 PvC 與 rule-based 教官｜M4｜`teacher`

目標：玩家可對戰教官，且教官足以作為 RL 訓練對手。

**E4-1｜feat: 教官框架** · `teacher` `P0`

- [ ] 以 C++ 實作 `IController` 教官基底（PvC 與訓練共用，免跨語言）
- [ ] 難度參數：反應延遲、失誤率、隨機性（供 curriculum 使用）

**E4-2｜feat: 三種角色的教官策略** · `teacher` `P0`

- [ ] tank：追擊貼臉、利用地形繞行
- [ ] scout：進場 → 撤退 → 脫戰回血循環
- [ ] ranged：保持距離、風箏、利用掩體

**E4-3｜feat: PvC 模式** · `pvp` `P0`

- [ ] 玩家對教官，沿用 E2 的 viewer
- [ ] 玩測並記錄手感與問題

**E4-4｜test: 教官 baseline 評測** · `teacher` `P1`

- [ ] 教官互打勝率表（3 組合 × 雙方）
- [ ] 教官對隨機策略勝率達標
- [ ] 結果寫入 docs，作為後續 agent 的基準線

### E5 DQN 訓練 v1｜M5｜`rl`

目標：6 個 agent 的第一版，對教官達到設定勝率。

**E5-1｜feat: DQN 基礎實作** · `rl` `P0`

- [ ] CNN（2×25×25）+ MLP 融合網路
- [ ] replay buffer、target network、ε-greedy、Huber loss
- [ ] 選項：Double / Dueling / PER（設定開關）

**E5-2｜feat: 訓練腳本與實驗管理** · `rl` `P0`

- [ ] YAML config、固定隨機種子
- [ ] TensorBoard logging
- [ ] checkpoint 與 metadata（obs\_version、config、git commit）

**E5-3｜feat: 對教官訓練 6 個 v1 agent** · `rl` `P0`

- [ ] 每個（自己, 對手）組合各訓練一個
- [ ] 週期性對教官評估勝率
- [ ] 保存 v1 並登記

**E5-4｜docs: 訓練診斷與問題紀錄** · `rl` `P1`

- [ ] 學習曲線、Q 值、動作分布（是否亂按攻擊）、空擊率、掩體利用
- [ ] 訓練失敗排查筆記（供 E9 判斷是否加指標）

### E6 CvC 與 Agent 對戰比較系統｜M6｜`eval`

目標：任意兩個 agent（或教官）可批次對戰，並產出可比較的報表。

**E6-1｜feat: CvC 模式** · `eval` `P0`

- [ ] agent 以 `IController` 接入環境，同組合的兩個 agent 互打（如 `tank_vs_scout` 對 `scout_vs_tank`）
- [ ] 支援不同版本、不同 checkpoint 互打
- [ ] test: CvC 單場可跑完並產生結果

**E6-2｜feat: Match runner** · `eval` `P0`

- [ ] 輸入（A, B, 場景, 場數, seeds）；雙方交換出生側各打一半，消除位置偏差
- [ ] 輸出 JSON / CSV：勝率、平均存活時間、剩餘生命、傷害輸出、命中率與空擊率、掩體吸收傷害
- [ ] CLI 介面

**E6-3｜feat: Model registry** · `eval` `P0`

- [ ] 版本命名：`<自己>_vs_<對手>/v<N>`
- [ ] metadata：訓練 config、obs\_version、git commit、評測結果
- [ ] 載入 API（依名稱與版本取得 controller）

**E6-4｜feat: 交叉對戰矩陣與 Elo** · `eval` `P1`

- [ ] round-robin：各版本與教官的勝率矩陣
- [ ] Elo 排名
- [ ] 報表輸出（表格 / heatmap）

**E6-5｜feat: 對戰 replay 輸出** · `eval` `P1`

- [ ] Match runner 可選擇保存 replay（沿用 E1-8 格式）
- [ ] 可挑選代表性對戰（最長、最短、逆轉）保存

### E7 對手池與防遺忘｜M7｜`rl`

目標：新版本不會忘記對舊版本與教官有效的策略。

**E7-1｜feat: 對手池** · `rl` `P0`

- [ ] 收錄教官與歷史 snapshot
- [ ] 抽樣策略（最新、歷史、教官的比例可設）
- [ ] 池大小與淘汰規則

**E7-2｜feat: CvC 交替訓練迴圈** · `rl` `P0`

- [ ] 輪流更新其中一方、另一方凍結，定期互換
- [ ] 定期 snapshot 入池
- [ ] 避免雙方同時學習造成訓練不穩定（non-stationary）

**E7-3｜test: 遺忘偵測回歸測試** · `rl` `P1`

- [ ] 新版本對舊版本與教官的勝率不得低於門檻
- [ ] 定期排程產出報告；低於門檻時標記該版本

### E8 Curriculum Learning｜M8｜`curriculum`

目標：逐步增加戰場複雜度，agent 能通過完整複雜度的場景。

**E8-1｜feat: 場景生成器** · `curriculum` `P0`

- [ ] 參數化：場地大小、固定 / 可移動掩體數量、隨機 seed
- [ ] 合法性檢查：出生點與通道不被封死
- [ ] test: 同 seed 重現同場景

**E8-2｜feat: 課程階段定義** · `curriculum` `P0`

- [ ] Stage 0 空場 → 1 少量固定掩體 → 2 加入可移動掩體 → 3 增加數量與隨機配置 → 4 完整複雜度
- [ ] 每階段一份 config
- [ ] 教官難度參數可隨階段調整

**E8-3｜feat: 課程排程器** · `curriculum` `P0`

- [ ] 晉級條件：對教官 / 對手池勝率連續 N 次評估達門檻
- [ ] 晉級後仍混入舊階段場景，避免遺忘
- [ ] 失敗時可降級或延長該階段

**E8-4｜test: 分階段評測與報告** · `curriculum` `P1`

- [ ] 同一 agent 在所有階段場景的勝率表
- [ ] 比較有 / 無 curriculum 的收斂速度

### E9 Observation 強化（條件式）｜M9｜`rl`

目標：訓練遇到瓶頸時，以最小成本加入高階戰場指標。

**E9-1｜feat: 高階指標實作** · `env` `P2`

- [ ] 候選：frame stack 或上一動作（補足速度資訊）、對手相對速度、最近掩體方位與距離、雙方射程差、是否位於對手射程內、預期傷害倍率、掩體後安全點
- [ ] 每個指標可開關

**E9-2｜feat: observation schema 版本化** · `env` `P2`

- [ ] `obs_version` 隨指標組合遞增
- [ ] 舊 checkpoint 依其 obs\_version 載入，registry 記錄對應版本

**E9-3｜docs: Ablation 實驗** · `rl` `P2`

- [ ] 加 / 不加各指標，比較勝率與收斂速度
- [ ] 結論寫入 docs，決定最終 observation

### E10 UE5 整合｜M10｜`ue5`

目標：UE5 內可進行 PvP / PvC / CvC，並重播保存的對戰。

**E10-1｜feat: core 編入 UE5** · `ue5` `P0`

- [ ] 以 plugin 或 third-party library 方式引入 `core`
- [ ] 以時間累積器讓固定步長模擬與 UE tick 解耦
- [ ] test: UE 內結果與 headless 結果一致（同 seed、同 action）

**E10-2｜feat: Renderer** · `ue5` `P0`

- [ ] 機器人、固定與可移動掩體、場地
- [ ] 血條與冷卻 UI
- [ ] 雷射：被固定掩體擋下時截斷、空擊朝向對手、可移動掩體照常呈現

**E10-3｜feat: 玩家輸入與模式選單** · `ue5` `P0`

- [ ] Enhanced Input 對應 18 個 action
- [ ] 局外選擇機器人類型與模式（PvP / PvC / CvC）

**E10-4｜feat: 模型匯出與 PvC 推論** · `ue5` `P0`

- [ ] Python 端匯出 ONNX
- [ ] UE5 以 C++ 載入並推論（NNE 或 ONNX Runtime，先做技術驗證）
- [ ] test: 同一組 observation 下，PyTorch 與 C++ 推論輸出一致

**E10-5｜feat: Replay 播放器** · `ue5` `P1`

- [ ] 讀取保存的對戰結果並重播
- [ ] 播放 / 暫停 / 倍速 / 鏡頭切換

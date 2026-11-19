# RLOOP — 遊戲與技術規格 v1.1（PR 草案）

> **權威規格候選版**：收錄截至 2026-11-19 已確認的機制；合併 PR 後才正式取代 [v1.0](../RLOOP_spec_v1.0.md)。
> 原版中的「建議」與未核准的數值仍然是候選設計。協作流程請以 [CONTRIBUTING.md](../CONTRIBUTING.md) 為準。

雙方機器人對戰競技場｜PvP / PvC / CvC｜C++ Core + Python DQN + UE5。
架構原則：C++ Core 為唯一遊戲邏輯來源、可獨立 headless 運行；Python 經 pybind11 訓練；UE5 只接入 Core、處理輸入、推論與呈現，不重寫碰撞或戰鬥規則。

## 1. 遊戲規格

### 1.1 模式與技術分層

- **PvP**：玩家對玩家
- **PvC**：玩家對 RL agent（開發初期先以 rule-based 教官代打，agent 訓練完成後替換）
- **CvC**：RL agent 對 RL agent
- 技術分層：
  - `core`（C++，不依賴 UE5）：固定步長模擬、kinematic 碰撞與推箱、戰鬥規則、observation、教官、replay 序列化
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

### 1.5 攻擊與傷害（已確認）

- 雷射自動瞄準、hitscan、無飛行時間。
- 每 tick 同時取得雙方 action，**先處理位移與碰撞**，再從位移後的相同世界狀態判定雙方攻擊，**最後同時結算傷害及死亡**。雙方不能因 player ID 先後處理而獲得優勢。
- 冷卻中按攻擊：`on_cooldown`，沒有發射、不消耗新冷卻、不重置脫戰計時。
- 冷卻已完成時發出攻擊指令：**視為成功發射，立即消耗冷卻並重置脫戰計時**，無論最後有無造成傷害。超出射程也算發射。
- 發射後依序判定：
  1. 對手不在射程內：`out_of_range`、傷害 0；仍可向對手方向呈現雷射效果。
  2. 連線穿過任一固定掩體：`blocked`、傷害 0、雷射被截斷。
  3. 連線穿過 n 個不同可移動掩體：`reduced`，傷害 = 原傷害 × `0.25^n`。
  4. 未受掩體影響：`hit`、完整傷害。
- 固定掩體永遠優先。兩層可移動掩體的倍率是 `0.0625`，不是 `0.625`。
- Core 回傳 `result_code`、`actual_damage`、可供 renderer 使用的射線事件；攻擊幾何邊界定義需有獨立測試。

### 1.6 Kinematic 移動、碰撞與連鎖推箱（已確認）

**不需要真實動力學模擬，也不強制使用 Box2D。** 需求中的「箱子質量 0、完全無彈性、轉動慣量無限大」應視為遊戲行為：箱子只能平移、不能旋轉或反彈、不保留慣性；沒有推動時立即停止。

- 機器人接觸箱子**且朝箱子方向移動**時才觸發推動；只接觸不推不減速。
- 推動中機器人移速乘 `push_speed_multiplier`（預設 `0.5`，configurable）；**不論同時推多少箱子，只減速一次**。
- 箱子允許連鎖推動；相互接觸而共同推動的箱子形成暫時群組，每位有效推動者在群組內只計一次。
- 同時推動時計算各推動者**減速後的推動速度向量**之和 `s = Σv_i`。箱子群組速度按**整體向量長度** clamp：若 `|s| > v_max`，則 `v_box = s × (v_max / |s|)`；否則 `v_box = s`。若 `s = 0` 則靜止。不得逐軸 clamp，亦不得因多個推動者提高 `v_max`。
- 已確認範例：`(3,4) + (4,3) = (7,7)`，當原本速度長度上限為 5，最終為 `(5/√2, 5/√2)`。
- `v_max` 是原本的箱子速度長度限制，須由 config／明確契約定義；目前**不替角色與場景填入未確認的數值**。
- 方向相反的推動向量按相加抵消；結果不得取決於玩家／箱子的遍歷順序。
- 牆壁、固定掩體及箱子不可互相穿透；箱子群組撞到障礙時限制合法位移。角落、箱組碰撞與雙人對推列入 regression tests。
- 八方向斜向移動須正規化。固定模擬時間步長 `dt`，不使用加速度或動量來模擬推動。

**仍待實作期確認**：箱組碰牆時是否允許沿牆滑動、機器人相互碰撞細節、幾何接觸容差，以及不同角色原始移速和 `v_max` 的對應；不得在實作中默默將候選設定當成既定需求。

### 1.7 脫戰回血（已確認）

- 持續 `out_of_combat_delay` 秒沒有受傷、沒有**成功發射**攻擊，開始以角色設定的 **HP/s** 回血，最高不超過最大 HP。
- 受到傷害，或成功發射（包含超距、被擋、減傷或命中），重置計時並停止回血。
- 冷卻中按攻擊但沒有發射：**不重置**計時。
- 等待秒數、回血率放 config，與固定 dt 無關。

### 1.8 勝負與超時（已確認）

- HP ≤ 0 即落敗；**同 tick 雙方都死亡才算同歸於盡的平手**。
- 時間到時比較**剩餘 HP 百分比**，較高者獲勝。**百分比完全相等才平手**，無任何勝率／HP 差異容忍範圍。
- 不同最大 HP 可透過 `HP_A × MaxHP_B` 和 `HP_B × MaxHP_A` 比較。實作需明訂固定精度／rounding 方式，避免浮點誤差造成假平手。
- 回合時限、角色 HP 具體數值仍由 config 定義，現階段不平衡調參。

### 1.9 RL 觀測空間（候選，待訓練驗證）

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
- Observation schema 從首版就版本化；評估加入自己／對手速度、frame stack，不將所有動態資訊一律推遲到後期。

### 1.10 Reward（v0 候選、非已確認）

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

### 1.12 Agent 對戰比較（基礎版需於 DQN 前可用）

- Match runner：輸入（A, B, 場景, 場數, seeds），輸出對戰統計
- 指標：勝率、平均存活時間、剩餘生命、傷害輸出、命中率與空擊率、掩體吸收傷害
- 交叉對戰矩陣（round-robin）與 Elo
- Model registry：版本、訓練設定、observation 版本
- 基礎 Match runner 應在第一組 DQN 訓練前可用；以固定 seed、交換出生側比較，並區分角色本身的平衡與 agent 學習能力。

### 1.13 Replay 與模型交付

- Replay 包含 schema/core 版本、config/其 hash、seed、每 tick action、事件及可驗證結果。相同 seed/config/actions 在文件指定平台與數值契約下應可逐 tick 重現。
- 模型匯出 ONNX 與 UE5 C++ 推論（NNE 或 ONNX Runtime）是候選方案，應在早期進行技術可行性驗證。
- Observation 由同一份 C++ Core 產生；`ActionV1`、`ObservationV1`、`ReplayV1` 和 model metadata 均須版本化，模型載入時檢查相容性。

## 2. 已確認的共識與尚未定案事項

| 決策 | 結果 |
| --- | --- |
| Git workflow | 單一 `main` + feature branches + PR；Conventional Commits + Type/Area/Priority |
| 開發策略 | 垂直切片、Issues 分批建立 |
| G1 | 位移後同時判定攻擊，並同時結算雙方傷害 |
| G2 | 成功發射即消耗冷卻並重置脫戰計時；冷卻中的按鍵無效 |
| G3 | 超時以剩餘 HP 百分比判勝，完全相等才平手；同 tick 雙亡平手 |
| G4 | 無動量／反彈／旋轉的 kinematic 箱子、推動時機器人速度乘 0.5（可設定）、不疊加 |
| G4-A | 可連鎖推；多個機器人速度向量相加，再對原速度長度上限 clamp |
| 角色平衡 | 以跨地圖與交換出生側後的 matchup 勝率接近 50:50 為目標；個別地圖可不同 |

**保留待定、暫不討論數值**：角色 HP／傷害／冷卻／射程／移速／回血，地圖大小與掩體佈局，回合時長，脫戰等待時間，箱子速度上限的具體值，碰撞邊界與沿牆滑動，Observation probe/速度特徵，reward 權重，模型推論方案。原 spec 標記「建議」的預設仍只是候選，不是已核准機制。

## 3. 里程碑與垂直切片 Roadmap

| Phase | 交付成果 |
| --- | --- |
| P0 | 文件規範、CMake、CI、測試骨架 |
| P1 | Headless Core、固定 dt、可重現的空場對局 |
| P2 | Kinematic 箱子／連鎖推動、攻擊與回血、PvP debug viewer |
| P3 | 版本化 Observation、pybind11、基本 Match runner |
| P4 | Rule-based teacher 與**一組角色對戰** DQN prototype、固定基準測試 |
| P5 | 擴展到六個 agents、CvC、model registry、對手池與 curriculum |
| P6 | UE5 完整模式、模型推論、Replay UI |

UE5 Core 串接與 ONNX inference 以**早期 feasibility spike** 驗證，不等到 P6。每組角色在代表性地圖／seeds 上測試，並交換出生側；整體追求 50:50，允許地圖偏好。本階段不強制設定每張地圖的勝率門檻。

## 4. GitHub conventions

請以 [CONTRIBUTING.md](../CONTRIBUTING.md) 為準。**不使用 `develop`**；`main` 是唯一長期分支。所有程式與規格改動透過 feature branch → PR → review/CI → squash merge。Commit 使用 Conventional Commits；Issue 使用 Type／Area／Priority labels。Release tag 採 `v0.1.0` 等版本號。

## 5. 第一批 Issues：8 張

這是已核准的第一批工作範圍，**實際 Issue 編號以 GitHub 回傳為準**；後續 backlog 分批產生，不直接把舊版 46 個條目全部建立。

1. `docs: establish project conventions and specifications` — 本次規格及協作文件，應在 PR review/merge 後才算完成。
2. `build: initialize C++ project and CI` — headless Core 與自動化 build/test 骨架。
3. `feat: implement deterministic world simulation` — `reset(seed, config)`、`step(actions)` 與固定 dt。
4. `feat: implement robot configuration and actions` — robot configs、18 actions、正規化與驗證。
5. `feat: implement kinematic arena and chained box pushing` — 箱組連鎖、推動減速、速度向量 clamp、碰撞測試。
6. `feat: implement combat and match resolution` — 發射冷卻、掩體減傷、回血與同步死亡／超時。
7. `feat: implement player controls and debug viewer` — 雙人操作、場景與戰鬥狀態可視化。
8. `test: verify gameplay rules and deterministic replay` — 固定 seed/replay 與跨模組的機制驗收。

本批刻意不含正式 DQN、最終角色數值平衡或 UE5 完整整合。

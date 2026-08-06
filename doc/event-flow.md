# Engine イベント処理フロー

Engine を中心としたキーボード/マウスイベントの処理フローと、設定再読み込みフローを示す。

---

## 1. キーボード/マウス入力フロー

```mermaid
---
config:
  layout: elk
---
flowchart TD
    A([ユーザー入力<br>キー押下 / マウスボタン])
    AS([scripter からの<br>ExecKeySeq])

    subgraph hook_dll["hook.cpp (DLL)"]
        B["lowLevelKeyboardProc():771<br>WH_KEYBOARD_LL コールバック"]
        B2["lowLevelMouseProc():749<br>WH_MOUSE_LL コールバック"]
    end

    subgraph engine_detour["engine.cpp — デトゥア (フックコールバック)"]
        C["Engine::keyboardDetour(KBDLLHOOKSTRUCT*):766<br>injected イベントをスキップ<br>KEYBOARD_INPUT_DATA を生成"]
        C2["Engine::mouseDetour(WPARAM, MSLLHOOKSTRUCT*):806<br>マウスイベントを擬似 MakeCode(1〜9) に変換<br>E1 フラグでキーボードと区別"]
    end

    subgraph scripter_path["scripter → engine (ExecKeySeq パス)"]
        SC["CmdProcessor::operator()(CmdArgsExecKeySeq):106<br>AdHocMaterializer::materialize()"]
        SS["Engine::scheduleAdHocKeySeq():1384<br>AdHocKeySeq をキューに投入"]
    end

    subgraph queue["engine.cpp — 入力キュー"]
        D[/"m_inputQueue (deque&lt;InputEvent&gt;)<br>variant&lt;KEYBOARD_INPUT_DATA, AdHocKeySeq&gt;<br>m_queueMutex で保護<br>SetEvent(m_readEvent)"/]
    end

    subgraph handler["engine.cpp — Engine::keyboardHandler():981<br>専用スレッド (keyboardHandlerThread)"]
        E["キューから InputEvent をデキュー<br>:981〜"]
        F["checkFocusWindow():19<br>フォーカスウィンドウ確認・キーマップ更新"]
        G{設定有効?<br>m_setting / m_isEnabled}
        GV{AdHocKeySeq?}
        H["キー検索<br>m_currentKeymap->searchKey()<br>修飾キー状態取得: getCurrentModifiers()"]
        I["fixModifierKey()<br>AssignMode 決定<br>normal / true / oneShot / oneShotRepeatable"]
        IA["reconstructCurrentFromContext():1406<br>Current.m_adhocKeySeq をセット<br>Type_Down をセット"]
        J["beginGeneratingKeyboardEvents():537<br>イベント生成エントリ<br>(再帰ガードリセット / prefix 処理 / before_key_down)"]
    end

    subgraph generate["engine.cpp — イベント生成チェーン"]
        K["generateKeyboardEvents():503<br>m_adhocKeySeq あり → Part_all で直接実行<br>なし → キーマップ検索"]
        L["generateEvents():314"]
        M["generateActionEvents():404"]
        N["generateKeySeqEvents():481"]
        O["generateKeyEvent():256"]
    end

    P["injectInput():611<br>SendInput() で Windows へ再注入"]
    Q([Windows が通常キー入力として処理])

    A -->|WH_KEYBOARD_LL| B
    A -->|WH_MOUSE_LL| B2
    AS -->|cmd パイプ| SC --> SS
    B -->|g.m_keyboardDetour コールバック| C
    B2 -->|g.m_mouseDetour コールバック| C2
    C --> D
    C2 --> D
    SS --> D
    D -->|m_readEvent シグナル| E
    E --> F
    F --> G
    G -->|無効 / 無効化中| P
    G -->|有効| GV
    GV -->|AdHocKeySeq| IA --> J
    GV -->|KEYBOARD_INPUT_DATA| H
    H -->|キーマップにヒット| I
    H -->|ヒットなし| P
    I --> J
    J --> K --> L --> M --> N --> O --> P
    P --> Q
```

### 擬似マウス MakeCode 対応表

| MakeCode | 意味 |
|----------|------|
| 1 | 左クリック |
| 2 | 右クリック |
| 3 | 中クリック |
| 4 | ホイール上 |
| 5 | ホイール下 |
| 6 | XButton1 |
| 7 | XButton2 |
| 8 | 水平ホイール左 |
| 9 | 水平ホイール右 |

### ExecKeySeq と通常キーの処理比較

| 処理 | 通常キー | ExecKeySeq |
|------|----------|-----------|
| キューの型 | `KEYBOARD_INPUT_DATA` | `AdHocKeySeq` |
| キーマップ検索 | `searchKey()` で実施 | `m_adhocKeySeq` オーバーライドでスキップ |
| 再帰ガードリセット | `beginGeneratingKeyboardEvents` で実施 | 同上 (共有) |
| `before_key_down` 発火 | `beginGeneratingKeyboardEvents` で実施 | 同上 (共有) |
| prefix キー状態管理 | `beginGeneratingKeyboardEvents` で実施 | 同上 (共有) |
| `m_emacsEditKillLine` リセット | `beginGeneratingKeyboardEvents` で実施 | 同上 (共有) |
| キーマップ base リセット | `beginGeneratingKeyboardEvents` で実施 | 同上 (共有) |
| modifier / oneShot 処理 | `keyboardHandler` 通常パスで実施 | 非適用 |
| `generateKeySeqEvents` の Part | Down / Up (物理押下に応じて) | `Part_all` |

---

## 2. フォーカス追跡フロー

WH_GETMESSAGE / WH_CALLWNDPROC フックは **グローバルフック** (threadId=0) であり、
フックを登録したプロセスのビット数に対応した DLL がフック対象プロセスに注入される。

- **nyamy.exe (64-bit)** が `installMessageHook()` を呼ぶ → **nyamy64.dll** が 64-bit プロセスに注入
- **nyamyd32.exe (32-bit)** が `installMessageHook(0)` を呼ぶ → **nyamy32.dll** が 32-bit プロセスに注入

どちらの DLL も同じ hook.cpp を共有しており、フォーカス変化を WM_COPYDATA で nyamy.exe へ送る。

```mermaid
---
config:
  layout: elk
---
flowchart TD
    subgraph proc_nyamy["nyamy.exe (64-bit)"]
        INIT["mayu.cpp:1064<br>installMessageHook(m_hwndTaskTray)<br>→ nyamy64.dll を 64-bit プロセスへ注入"]
        SPAWN["mayu.cpp:1142〜1156<br>CreateMutex(MUTEX_YAMYD_BLOCKER)<br>CreateProcess(nyamyd32.exe)"]
    end

    subgraph proc_nyamyd["nyamyd32.exe (32-bit)"]
        NYAMYD["yamyd.cpp:wWinMain():19<br>OpenMutex(MUTEX_YAMYD_BLOCKER)<br>installMessageHook(0)<br>→ nyamy32.dll を 32-bit プロセスへ注入<br>WaitForSingleObject (mutex が解放されるまで待機)"]
    end

    subgraph hook_dll64["nyamy64.dll — hook.cpp (64-bit プロセス内)"]
        A64["getMessageProc():556<br>WH_GETMESSAGE — フォーカス変化検出<br>ロックキー / IME 状態変化も検出"]
        A264["callWndProc():637<br>WH_CALLWNDPROC — ウィンドウアクティブ化検出"]
        B64["notifySetFocus():389<br>notifyName() → SendMessageTimeout()<br>WM_COPYDATA で m_hwndTaskTray へ送信"]
        B264["notifyLockState():535<br>WM_COPYDATA で m_hwndTaskTray へ送信"]
    end

    subgraph hook_dll32["nyamy32.dll — hook.cpp (32-bit プロセス内)"]
        A32["getMessageProc():556<br>(同上・32-bit プロセス内で動作)"]
        A232["callWndProc():637"]
        B32["notifySetFocus():389<br>WM_COPYDATA で m_hwndTaskTray へ送信"]
        B232["notifyLockState():535"]
    end

    subgraph mayu_recv["mayu.cpp — タスクトレイウィンドウプロシージャ"]
        WCD["WM_COPYDATA ハンドラ:301<br>→ notifyHandler():150"]
        NH_SF["case Type_setFocus:152<br>Engine::setFocus()"]
        NH_LS["case Type_lockState:202<br>修飾キー状態を更新"]
    end

    subgraph engine_focus["engine.cpp — Engine"]
        C["Engine::setFocus():1518<br>FocusOfThreads マップを更新<br>スレッドID → FocusOfThread"]
        D["settings->m_keymaps.searchWindow()<br>クラス名正規表現 × タイトル正規表現<br>適用キーマップリストを決定"]
        E[/"m_currentFocusOfThread<br>m_currentKeymap を更新"/]
    end

    SPAWN -->|"起動"| proc_nyamyd
    INIT --> hook_dll64
    NYAMYD --> hook_dll32

    A64 --> B64
    A264 --> B64
    A32 --> B32
    A232 --> B32

    B64 -->|"WM_COPYDATA (Type_setFocus)"| WCD
    B32 -->|"WM_COPYDATA (Type_setFocus)"| WCD
    B264 -->|"WM_COPYDATA (Type_lockState)"| WCD
    B232 -->|"WM_COPYDATA (Type_lockState)"| WCD

    WCD --> NH_SF
    WCD --> NH_LS
    NH_SF --> C --> D --> E
```

### nyamyd32 のライフサイクル

| フェーズ | 処理 |
|--------|------|
| 起動 | `OpenMutex(MUTEX_YAMYD_BLOCKER)` が成功した場合のみ動作 (nyamy.exe が保持) |
| 動作中 | `installMessageHook(0)` → nyamy32.dll を 32-bit プロセスへグローバル注入 |
| 終了 | nyamy.exe が `ReleaseMutex()` → `WaitForSingleObject` が解除 → `uninstallMessageHook()` して終了 |

---

## 3. 設定再読み込みフロー

設定リロード時は scripter プロセスを再起動する。
`start(syms)` は `std::async` で `launchScripter(syms)` を起動し、即座に返る。
初回起動時も同じ `start(syms)` を呼ぶ（mayu.cpp:770）。

```mermaid
---
config:
  layout: elk
---
sequenceDiagram
    participant mayu as mayu.cpp<br/>(メインスレッド)
    participant sm as scripter_manager.cpp<br/>ScripterManager
    participant async as scripter_manager.cpp<br/>launchScripter() [async]
    participant dt as scripter_manager.cpp<br/>dataThread / runReader()
    participant sc as nyamy_scripter.cpp<br/>scripter_engine()
    participant eng as engine.cpp<br/>keyboardHandler() [engine スレッド]

    mayu->>sm: start(syms):95
    sm->>async: std::async → launchScripter(syms):108

    Note over async: 既存 scripter が起動中なら
    async->>sc: sendQuit() → CtrlId::Quit + ctrl パイプ close
    sc->>sc: CtrlId::Quit 受信 → ループ脱出
    async->>async: WaitForMultipleObjects<br/>(process + dataThread + msgThread)
    async->>async: closeHandles() / m_quitSent=false

    async->>sc: CreateProcess(nyamy-scripter.exe)<br/>パイプ確立 + dataThread / msgThread 起動
    async->>sc: CtrlStreamWriter::writeStart(syms)<br/>→ ctrl パイプ書き込み

    sc->>sc: ctrlReader.readNext(id):122<br/>CtrlId::Start を受信
    sc->>sc: ctrlReader.readStart():129
    sc->>sc: doCompile(syms, dataWriter):51<br/>ConfigFiles → MayuParser::parseFile()<br/>→ MayuCompiler::compile()<br/>→ CmdStreamWriter (各 Def* コマンド)
    sc->>dt: CmdStreamWriter::writeCommit()<br/>→ cmd パイプへ CmdId::Commit(0xFF) 書き込み

    dt->>dt: CmdProcessor::process():85<br/>m_builder 初期化 (Global キーマップ設定)<br/>各コマンドを std::visit で SettingBuilder に積む
    dt->>dt: CmdId::Commit 受信:268<br/>SettingBuilder::build() → Setting<br/>m_builder.reset() → ビルダー解放<br/>onCommit コールバック実行
    dt->>dt: ScripterManager::setPendingSetting():441<br/>static な単一スロットへ shared_ptr&lt;Setting&gt; を move<br/>(旧 Setting が残っていればここで解放)
    dt->>mayu: PostMessage(WM_ScripterSettingReady, 0, 0)<br/>ペイロードなし。通知が失われても<br/>スロット上書き/終了時クリアで解放される

    mayu->>mayu: WM_APP_scripterSettingReady ハンドラ:437<br/>ScripterManager::takePendingSetting()<br/>スロットが空なら何もしない
    mayu->>eng: Engine::scheduleSetting():1514<br/>入力キューへ push_back<br/>(UI スレッドはここで解放される)
    eng->>eng: keyboardHandler() でイベント境界に取り出し:1125<br/>Engine::applySetting()
```

UI スレッドで直接 `m_setting` を差し替えず、engine スレッドの入力キュー
(`InputEvent` variant) に載せてイベント境界で適用する。これにより

- キューに溜まった旧設定下のキーイベントより後に切り替わる
- `&Sync` / `&Wait` の同期中を待つ必要がなくなる (取り出し地点では
  `m_isSynchronizing` が必ず false)
- UI スレッドがメッセージループへ即座に戻るため、`&Sync` を完了させる
  メールスロット完了 APC が飢餓しない

### Engine::applySetting() の内部処理 (engine.cpp :1460)

```mermaid
---
config:
  layout: elk
---
flowchart TD
    A["Engine::applySetting(shared_ptr&lt;Setting&gt;):1460"]
    B["旧設定のキー押下状態を保存<br>m_key->m_isPressed など"]
    C["m_setting をアトミック更新<br>memory_order_release"]
    D["g_hookData->m_correctKanaLockHandling を更新"]
    E["全 FocusOfThread のキーマップを再検索<br>settings->m_keymaps.searchWindow()"]
    F["m_currentFocusOfThread を globalFocus にリセット<br>m_currentKeymap を globalFocus.front() に設定"]
    G([完了])

    A --> B --> C --> D --> E --> F --> G
```

---

## 4. スレッド構成

```mermaid
---
config:
  layout: elk
---
flowchart LR
    subgraph proc_nyamy["nyamy.exe プロセス (64-bit)"]
        T_main["メインスレッド<br>mayu.cpp<br>Win32 メッセージループ"]
        T_kb["InputHandler (キーボード)<br>engine.cpp :1198<br>WH_KEYBOARD_LL フックを保持"]
        T_mouse["InputHandler (マウス)<br>engine.cpp :1199<br>WH_MOUSE_LL フックを保持"]
        T_handler["keyboardHandler スレッド<br>engine.cpp :981<br>キュー消費・キーマップ変換・イベント生成"]
        T_data["dataThread<br>scripter_manager.cpp :286<br>CmdStream 読み取り・Setting 構築"]
        T_msg["msgThread<br>scripter_manager.cpp :318<br>scripter ログ (UTF-16) 表示"]
    end

    subgraph proc_nyamyd["nyamyd32.exe プロセス (32-bit)"]
        T_nyamyd["wWinMain()<br>yamyd.cpp :19<br>nyamy32.dll を 32-bit プロセスへ注入<br>MUTEX_YAMYD_BLOCKER を監視"]
    end

    subgraph proc_sc["nyamy-scripter.exe プロセス"]
        T_sc["scripter_engine()<br>nyamy_scripter.cpp :85<br>.mayu コンパイル・CmdStream 送出"]
    end

    App32[("32-bit アプリ<br>(nyamy32.dll 注入済)")]
    App64[("64-bit アプリ<br>(nyamy64.dll 注入済)")]
    Windows[(Windows)]

    T_kb -->|"WH_KEYBOARD_LL<br>→ keyboardDetour()"| T_handler
    T_mouse -->|"WH_MOUSE_LL<br>→ mouseDetour()"| T_handler
    T_handler -->|"SendInput()"| Windows
    T_data -->|"PostMessage(WM_APP_scripterSettingReady)<br>→ scheduleSetting() で入力キューへ"| T_main
    T_sc -->|"cmd パイプ (CmdStream)"| T_data
    T_sc -->|"msg パイプ (stdout+stderr)"| T_msg
    T_main -->|"ctrl パイプ (CtrlStream: Start / Quit)"| T_sc
    T_main -->|"CreateMutex + CreateProcess"| T_nyamyd
    T_nyamyd -->|"WH_GETMESSAGE / WH_CALLWNDPROC<br>DLL 注入"| App32
    T_main -->|"WH_GETMESSAGE / WH_CALLWNDPROC<br>DLL 注入"| App64
    App32 -->|"WM_COPYDATA (フォーカス / ロックキー通知)<br>notifySetFocus():389 / notifyLockState():535"| T_main
    App64 -->|"WM_COPYDATA (フォーカス / ロックキー通知)<br>notifySetFocus():389 / notifyLockState():535"| T_main
    T_sc -->|"scheduleAdHocKeySeq()<br>(ExecKeySeq / ExecUserFunc 結果)"| T_handler
```

---

## 5. 主要関数・メソッド一覧

### hook.cpp

| 関数 | 行 | 役割 |
|------|---:|------|
| `lowLevelMouseProc()` | 749 | WH_MOUSE_LL コールバック |
| `lowLevelKeyboardProc()` | 771 | WH_KEYBOARD_LL コールバック |
| `installKeyboardHook()` | 826 | WH_KEYBOARD_LL フックを登録/解除 |
| `installMouseHook()` | 847 | WH_MOUSE_LL フックを登録/解除 |
| `installMessageHook()` | 793 | WH_GETMESSAGE + WH_CALLWNDPROC を登録 (nyamy.exe / nyamyd32.exe 共用) |
| `uninstallMessageHook()` | 812 | 上記フックを解除 |
| `notifySetFocus()` | 389 | フォーカス変化を WM_COPYDATA で nyamy.exe へ通知 |
| `notifyLockState()` | 535 | ロックキー変化を WM_COPYDATA で nyamy.exe へ通知 |

### yamyd.cpp

| 関数 | 行 | 役割 |
|------|---:|------|
| `wWinMain()` | 19 | MUTEX_YAMYD_BLOCKER を取得後 `installMessageHook(0)` で nyamy32.dll をグローバル注入。nyamy.exe が mutex を解放したら `uninstallMessageHook()` して終了 |

### engine.cpp

| 関数/メソッド | 行 | 役割 |
|-------------|---:|------|
| `Engine::checkFocusWindow()` | 19 | フォーカスウィンドウ確認・キーマップ更新 |
| `Engine::generateKeyEvent()` | 256 | キーイベント最終生成 |
| `Engine::generateEvents()` | 314 | アクションシーケンス処理 |
| `Engine::generateActionEvents()` | 404 | アクション実行 |
| `Engine::generateKeySeqEvents()` | 481 | キーシーケンス処理 |
| `Engine::generateKeyboardEvents()` | 503 | キーボードイベント生成コア。`m_adhocKeySeq` セット時はキーマップ検索をスキップし Part_all で実行 |
| `Engine::beginGeneratingKeyboardEvents()` | 537 | イベント生成エントリ (再帰ガードリセット / prefix 処理 / before_key_down・after_key_up) |
| `Engine::keyboardDetour(KBDLLHOOKSTRUCT*)` | 766 | フックコールバック → キューイング |
| `Engine::mouseDetour(WPARAM, MSLLHOOKSTRUCT*)` | 806 | マウスコールバック → 擬似キー変換 |
| `Engine::keyboardHandler()` | 981 | キュー処理メインループ (専用スレッド) |
| `Engine::applySetting()` | 1460 | 新しい Setting を適用 (engine スレッド専用) |
| `Engine::scheduleSetting()` | 1514 | Setting をキューに投入 (UI スレッドから) |
| `Engine::scheduleAdHocKeySeq()` | 1526 | AdHocKeySeq をキューに投入 |
| `Engine::reconstructCurrentFromContext()` | 1406 | TriggerInfo から Current を復元 (ExecKeySeq 用) |
| `Engine::setFocus()` | 1518 | フォーカス情報をエンジンへ反映 |

### scripter_manager.cpp

| 関数/メソッド | 行 | 役割 |
|-------------|---:|------|
| `ScripterManager::sendQuit()` | 57 | CtrlId::Quit 送信 + ctrl パイプ close (冪等) |
| `ScripterManager::collectHandles()` | 76 | プロセス + スレッドハンドルを配列に収集 |
| `ScripterManager::closeHandles()` | 85 | 全ハンドルを閉じて NULL/INVALID に初期化 |
| `ScripterManager::start(syms)` | 95 | std::async で launchScripter を起動 (再起動も兼ねる)。前回が完了していなければスキップ |
| `ScripterManager::launchScripter(syms)` | 108 | 起動済み scripter を停止後、新プロセスを起動し writeStart(syms) を送信 |
| `ScripterManager::setExecKeySeqCallback()` | 266 | ExecKeySeq 受信時のコールバックを登録 |
| `ScripterManager::execUserFunc()` | 272 | ExecUserFunc を scripter へ送信 |
| `ScripterManager::dataThread()` | 286 | CmdStream 読み取りスレッドエントリ |
| `ScripterManager::runReader()` | 293 | CmdProcessor でパイプを消費 |
| `ScripterManager::setPendingSetting()` | 42 | 完成した Setting を static な単一スロットへ move (旧 Setting はここで解放) |
| `ScripterManager::takePendingSetting()` | 50 | スロットから Setting を取り出す。空なら空ポインタ |
| `ScripterManager::clearPendingSetting()` | 56 | スロットを解放 (`~ScripterManager()` から呼ばれる) |
| `ScripterManager::msgThread()` | 318 | scripter ログ読み取りスレッドエントリ |

### nyamy_scripter.cpp (scripter/)

| 関数 | 行 | 役割 |
|------|---:|------|
| `doCompile()` | 51 | .mayu コンパイル + CmdStream 書き出し |
| `scripter_engine()` | 85 | DLL エクスポート・CtrlStream ループ (Start / Quit / ExecUserFunc を処理) |

### cmd_processor.cpp

| 関数/メソッド | 行 | 役割 |
|-------------|---:|------|
| `CmdProcessor::onCommit()` | 51 | Commit コールバック登録 |
| `CmdProcessor::process()` | 85 | m_builder 初期化 + CmdStream 読み取り・Setting 構築ループ |
| `CmdProcessor::operator()(CmdArgsExecKeySeq)` | 106 | ExecKeySeq をマテリアライズして ExecKeySeq コールバックへ |
| `CmdProcessor::operator()(CmdArgsCommit)` | 268 | build() → m_builder.reset() → onCommit コールバック |

### mayu.cpp

| 箇所 | 行 | 役割 |
|------|---:|------|
| `WM_APP_scripterSettingReady` 定義 | 92 | メッセージ ID 定数 (`ScripterManager::WM_ScripterSettingReady` から導出) |
| `WM_APP_scripterSettingReady` ハンドラ | 437 | `ScripterManager::takePendingSetting()` で Setting 受け取り → `Engine::scheduleSetting()` で engine スレッドへ委譲 |
| `WM_COPYDATA` ハンドラ | 301 | hook DLL からのフォーカス/ロックキー通知を受信 → `notifyHandler()` |
| `notifyHandler()` | 150 | Notify 種別に応じて `Engine::setFocus()` などを呼び出し |
| `m_scripter->start(syms)` | 771 | 初回起動時・リロード時に scripter を (再)起動 |
| nyamyd32 起動 | 1143〜1157 | `CreateMutex(MUTEX_YAMYD_BLOCKER)` + `CreateProcess("nyamyd32")` |

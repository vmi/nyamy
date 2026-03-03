# バイナリプロトコル仕様

## 概要

yamy ↔ yamy-scripter 間の通信は **4 本のハンドル** で行う。

| チャネル | 方向 | 渡し方 | 内容 |
|--------|------|--------|------|
| ctrl パイプ | yamy → scripter | コマンドライン `--ctrl=N` (継承ハンドル番号) | CtrlStream (バイナリ) |
| cmd パイプ | scripter → yamy | コマンドライン `--cmd=N` (継承ハンドル番号) | CmdStream (バイナリ) |
| msg パイプ | scripter → yamy | STARTUPINFO (stdout+stderr をマージ) | ログ出力 (テキスト) |
| NUL | — | STARTUPINFO stdin | 即 EOF |

**stdin/stdout/stderr はバイナリプロトコルに使用しない。**
これにより、scripter 実装側が `printf` / `std::cout` / `puts` 等を使っても
CmdStream を汚染しない。

共通規則 (バイナリストリーム):
- すべての整数はリトルエンディアン
- 文字列は `U16 長さ (文字数)` + `UTF-16LE 文字列` (null 終端なし)

---

## プロセス起動とハンドル継承

yamy 側 (`ScripterManager::start()`) の処理:

```cpp
// パイプ生成 (sa.bInheritHandle = TRUE で子プロセスに継承可能)
CreatePipe(&hCtrlRead,   &m_hCtrlWrite, &sa, 0);  // ctrl
CreatePipe(&m_hDataRead, &hDataWrite,   &sa, 0);  // cmd (data)
CreatePipe(&m_hMsgRead,  &hMsgWrite,    &sa, 0);  // msg (log)
hNul = CreateFile(L"NUL", GENERIC_READ, ..., &sa, ...);  // stdin 代替

// yamy 側ハンドルは子プロセスに継承させない
SetHandleInformation(m_hCtrlWrite, HANDLE_FLAG_INHERIT, 0);
SetHandleInformation(m_hDataRead,  HANDLE_FLAG_INHERIT, 0);
SetHandleInformation(m_hMsgRead,   HANDLE_FLAG_INHERIT, 0);
SetHandleInformation(hNul,         HANDLE_FLAG_INHERIT, 0);

// コマンドラインにハンドル番号を付加
swprintf_s(cmdLine, L"\"%s\" --ctrl=%llu --cmd=%llu",
           scripterPath.c_str(),
           (unsigned long long)(uintptr_t)hCtrlRead,
           (unsigned long long)(uintptr_t)hDataWrite);

// stdin=NUL, stdout+stderr=msgパイプ
si.hStdInput  = hNul;
si.hStdOutput = hMsgWrite;
si.hStdError  = hMsgWrite;  // 同じパイプにマージ

CreateProcess(NULL, cmdLine, ..., TRUE /*bInheritHandles*/, ..., &si, &pi);
```

scripter 側 (`scripter_engine()`) の処理:

```cpp
// argv から --ctrl=N, --cmd=N を解析してハンドルを復元
HANDLE hCtrlRead  = ...; // reinterpret_cast<HANDLE>(wcstoull(argv[i]+7, ...))
HANDLE hDataWrite = ...; // reinterpret_cast<HANDLE>(wcstoull(argv[i]+6, ...))

// stderr を UTF-16 に設定 (wcerr 経由のログを yamy が wchar_t 単位で読む)
_setmode(_fileno(stderr), _O_U16TEXT);

// 継承ハンドルを streambuf でラップ
PipeReadStreambuf  ctrlBuf(hCtrlRead);
PipeWriteStreambuf dataBuf(hDataWrite);
std::istream ctrlStream(&ctrlBuf);
std::ostream dataStream(&dataBuf);

CtrlStreamReader ctrlReader(ctrlStream);
CmdStreamWriter  dataWriter(dataStream);
// ... CtrlStream ループ
```

---

## msg パイプ (ログ)

scripter の stdout と stderr を 1 本のパイプにマージして yamy がログとして受信する。

- scripter 側: stderr に `_O_U16TEXT` を設定 → `std::wcerr` 出力が UTF-16 LE になる
- yamy 側: `PipeReadWStreambuf` (wchar_t = 2 バイト単位読み取り) で受信
- stdout に書き込まれたバイト列もこのパイプに流れるが、scripter が `std::cout` 等をログ目的で使う場合は UTF-16 でないと文字化けする

---

## CtrlStream (yamy → scripter)

### 現在の実装

| コマンド | ID | 状態 |
|---------|-----|------|
| Reload | 0x01 | 使用中 (シンボルセット送信) |
| Quit | 0xFF | 使用中 |

### 将来の変更 (未実装)

| コマンド | 旧 | 新 |
|---------|----|----|
| Reload | 0x01 (シンボルセット送信) | **廃止** (シンボルは argv で渡す) |
| CallFunc | なし | **0x01 (新規)** |
| Quit | 0xFF | 0xFF (変更なし) |

### コマンド ID (現状)

```cpp
enum class CtrlId : uint8_t {
    Reload = 0x01,  ///< Recompile with the given symbols
    Quit   = 0xFF,  ///< Terminate scripter
};
```

### Reload (0x01)

```
[0x01]
[n_syms : U16]
[sym0   : String]
...
[symN   : String]
```

### Quit (0xFF)

```
[0xFF]
```

### 将来の変更 (未実装)

| コマンド | 旧 | 新 |
|---------|----|----|
| Reload | 0x01 (シンボルセット送信) | **廃止** (シンボルは argv で渡す) |
| CallFunc | なし | **0x01 (新規)** |
| Quit | 0xFF | 0xFF (変更なし) |

#### CallFunc (0x01) — 未実装

```
[0x01]
[func_name : String]
[n_args    : U16]
[arg0      : TypedArg]
...
[argN      : TypedArg]
[context   : YamyFuncCallContext]
```

#### YamyFuncCallContext (未実装、60 bytes)

```c
typedef struct {
    uint64_t keyHwnd;
    uint16_t vkey;
    uint16_t scanCode;
    uint8_t  isKeyUp;
    uint8_t  imeOpen;
    uint8_t  e0Flag;
    uint8_t  _pad;
    uint64_t modifierState;
    uint32_t keyWindowDpi;
    uint32_t keyDisplayIndex;
    uint64_t mouseHwnd;
    int32_t  mouseX;
    int32_t  mouseY;
    uint32_t mouseWindowDpi;
    uint32_t mouseDisplayIndex;
} YamyFuncCallContext;
```

---

## CmdStream (scripter → yamy)

### 現在の実装

| コマンド | ID | 状態 |
|---------|-----|------|
| DefKeySeq〜Commit | 0x01〜0xFF | 使用中 (変更なし) |
| ExecFunc | 0x30 | 未実装 (将来追加) |

**現状のフロー**: scripter は compile 中に CmdStream コマンドを即座に stdout へ書き出す
(キューイングなし)。エラー発生時は Commit を書かずに終了する。

### 将来の変更 (未実装): キューイング方式

scripter は Def 系コマンドを内部でキューイングし、**Commit (0xFF) が呼ばれた
時点でエラーなく到達した場合のみ** まとめて yamy に送出する。
エラー発生時はキューを破棄し、何も送出しない。

Commit 送出後もパイプを維持し、ExecFunc コマンドを継続して送出できる。

### コマンド ID

```cpp
enum class CmdId : uint8_t {
    RegKeySeq  = 0x01,
    DefKey     = 0x10,
    DefMod     = 0x11,
    DefSync    = 0x12,
    DefAlias   = 0x13,
    DefSubst   = 0x14,
    DefOption  = 0x15,
    DefSymbol  = 0x16,
    BeginKeymap = 0x20,
    AssignKey  = 0x21,
    AssignEvent = 0x23,
    AssignMod  = 0x24,
    Commit     = 0xFF,
    // 未実装:
    ExecFunc   = 0x30,
};
```

### ExecFunc (0x30) — 未実装

Commit 後にユーザー定義関数コールバック内から送出。yamy 側の組み込み関数を
即時実行する。

```
[0x30]
[func_name : String]
[n_args    : U16]
[arg0      : TypedArg]
...
[argN      : TypedArg]
```

---

## 型付き引数 (TypedArg) — 未実装

→ [typed-args.md](typed-args.md) 参照

---

## プロセスライフサイクル

### 現在の起動シーケンス

```
yamy                              yamy-scripter
  |                                   |
  |--- CreateProcess(                 |
  |      "yamy-scripter.exe          |
  |       --ctrl=N --cmd=M"          |
  |      stdin=NUL                   |
  |      stdout=stderr=msgPipe) ---->|
  |                                   | 1. argv から --ctrl, --cmd を解析
  |                                   | 2. stderr を UTF-16 に設定
  |--- CtrlStream: Reload(syms) ----->| 3. .mayu コンパイル
  |<--- CmdStream (all cmds) ---------|
  |<--- Commit (0xFF) ---------------|
  |--- CtrlStream: Reload(syms) ----->| 4. 再コンパイル (設定変更時)
  |<--- CmdStream (all cmds) ---------|
  |<--- Commit (0xFF) ---------------|
  |--- CtrlStream: Quit (0xFF) ------>|
  |                                   | 5. 終了
```

### 将来の起動シーケンス (プロセス再起動方式、未実装)

```
yamy                              yamy-scripter
  |                                   |
  |--- CreateProcess(                 |
  |      "yamy-scripter.exe          |
  |       --ctrl=N --cmd=M           |
  |       -DSYM1 -DSYM2")  -------->|
  |                                   | 1. argv から --ctrl, --cmd, -D シンボルを解析
  |                                   | 2. .mayu コンパイル or スクリプト実行
  |                                   | 3. Commit → CmdStream 送出
  |<--- CmdStream (batch) -----------|
  |<--- Commit (0xFF) ---------------|
  |                                   | 4. CtrlStream 待ちループへ移行
  |--- CallFunc (0x01) ------------->|
  |<--- ExecFunc (0x30) -------------|
  |--- Quit (0xFF) ----------------->|
  |                                   | 5. 終了
```

### 将来の設定再読み込み (未実装、プロセス再起動方式)

```
yamy
  |--- CtrlStream: Quit を送信 OR ctrl パイプをクローズ
  |--- WaitForSingleObject(process, 5000ms)
  |--- CloseHandle(process)
  |--- CreateProcess(新シンボル付きで再起動)
```

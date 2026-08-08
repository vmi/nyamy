# nyamy 側の変更仕様

## 実装済みの変更

### パイプ構成の変更

```
旧: stdin=CtrlStream, stdout=CmdStream, stderr=ログ
新: stdin=NUL, stdout+stderr=msgパイプ(マージ), 環境変数 NYS_CTRL でCtrlStream, NYS_CMD でCmdStream
```

scripter の stdin は NUL デバイス (即 EOF)。CtrlStream / CmdStream は
継承ハンドルの番号を環境変数 `NYS_CTRL` / `NYS_CMD` で渡す。
これにより scripter 実装が `printf` / `std::cout` 等を使っても CmdStream を汚染しない。

### `pipe_streambuf.h` — 新規追加

`PipeWriteStreambuf` / `PipeReadStreambuf` / `PipeReadWStreambuf` を
`pipe_streambuf.h` に集約。`scripter_manager.cpp` と `nyamy_scripter.cpp` の
重複実装を除去した。

### `ScripterManager` インターフェース (現状)

```cpp
class ScripterManager
{
public:
    ScripterManager(SyncObject *i_soLog, std::wostream *i_log, HWND i_hwndNotify);
    ~ScripterManager();

    bool start(const Symbols &syms);     // プロセス起動・パイプ確立・Start(syms) 送信
    void sendQuit();                     // Quit 送信 + ctrl パイプ close (idempotent)
    void stopReaders();                  // reader の読み取りを畳む (idempotent)
    DWORD collectHandles(HANDLE*, DWORD); // WaitForMultipleObjects 用
    void closeHandles();

    // reader を止め、プロセスの停止を待つ。i_graceMillisec 待っても残っていれば
    // TerminateProcess し、さらに kScripterKillWaitMillisec 待つ。
    void forceStop(DWORD i_graceMillisec);
    // 非同期の start()/再起動タスクの完了を待つ (待たないとハンドルが足下で変わる)
    void waitForPendingStart();

    using ExecKeySeqCallback = std::function<void(AdHocKeySeq)>;
    void setExecKeySeqCallback(ExecKeySeqCallback cb);  // ExecKeySeq 受信コールバック設定

    void execUserFunc(const wstringi &name,             // CtrlStream: ExecUserFunc 送信
                      const std::vector<FuncArg> &args,
                      const TriggerInfo &ctx);

    static const UINT WM_ScripterSettingReady;

private:
    HANDLE m_hCtrlWrite;    ///< nyamy -> scripter (CtrlStream, non-stdio)
    HANDLE m_hDataRead;     ///< scripter -> nyamy (CmdStream, non-stdio)
    HANDLE m_hMsgRead;      ///< scripter stdout+stderr -> nyamy (log, merged)
    HANDLE m_hScripterProcess;
    HANDLE m_hDataThread;
    HANDLE m_hMsgThread;    ///< (旧 m_hStderrThread)
    HANDLE m_hReaderStop;   ///< 上記 2 本の読み取りを畳む停止イベント
    // ...
};
```

`start(syms)` はプロセスを起動し、CtrlStream の Start コマンド (0x01) でシンボルセットを送信する。
scripter 側は受信後に .mayu をコンパイルし、CmdStream (cmd パイプ) を返す。

`execUserFunc(name, args, ctx)` は CtrlStream の ExecUserFunc コマンド (0x02) を送信する。
scripter 側は受信後に `on_exec_user_func` コールバックを呼び出す。
scripter が `nys_exec_keyseq()` を呼ぶと CmdStream の ExecKeySeq (0x02) が返り、
`setExecKeySeqCallback` で登録したコールバックが呼ばれる。

`m_hMsgRead` は scripter の stdout+stderr をマージした msg パイプ。
`msgThread` / `runMsgReader()` が `PipeReadWStreambuf` (wchar_t 単位) で読み取り、
1 行ずつ nyamy のログウィンドウに表示する。

### `start(syms)` — コマンドライン構築とシンボル送信

`NYS_CTRL` / `NYS_CMD` を先頭に持つ環境ブロックを構築し、子プロセスに継承させる。
コマンドライン自体にはハンドル番号を含まない。

```cpp
wchar_t ctrlVal[32], cmdVal[32];
swprintf_s(ctrlVal, L"%llu", (unsigned long long)(uintptr_t)hCtrlRead);
swprintf_s(cmdVal,  L"%llu", (unsigned long long)(uintptr_t)hDataWrite);
// NYS_CTRL + NYS_CMD を先頭に持つ環境ブロックを作成 (既存 env をマージ)

wchar_t cmdLine[1024];
swprintf_s(cmdLine, L"\"%s\"", scripterPath.c_str());
// nyamy.ini の cmdLine 設定があれば追加

si.hStdInput  = hNul;      // NUL (即 EOF)
si.hStdOutput = hMsgWrite; // ログパイプ
si.hStdError  = hMsgWrite; // 同じパイプにマージ

CreateProcess(NULL, cmdLine, NULL, NULL, TRUE /*bInheritHandles*/,
              CREATE_UNICODE_ENVIRONMENT, envBlock, ...);
```

---

## 変更対象ファイル (未実装部分)

以下は設計済みだが未実装の変更。

- `scripter_manager.cpp/h` — reload = 再起動 (プロセス再起動方式)
- `nyamy.ini` — [nyamy-scripter] セクション追加

---

## ScripterManager の将来の変更 (未実装)

### インターフェース変更 (プロセス再起動方式)

```cpp
class ScripterManager
{
public:
    bool start(const Symbols& syms);  // 現在と同じシグネチャ (引数は argv に変わる)

    // reload = 旧プロセス終了 + 新プロセス起動
    void reload(const Symbols& syms);  // sendQuit() + start(syms)

    // 以下は実装済み (現在のインターフェースを参照)
    // execUserFunc / setExecKeySeqCallback

    static const UINT WM_ScripterSettingReady;
};
```

### start() の将来形 (プロセス再起動方式)

```cpp
bool ScripterManager::start(const Symbols& syms)
{
    // 1. nyamy.ini から command= を読み取り
    wstringi cmdLine = readScripterCommand(exeDir);
    if (cmdLine.empty())
        cmdLine = L"nyamy-scripter.exe";

    // 2. ${ENV_VAR} 展開
    cmdLine = expandEnvVars(cmdLine);

    // 3. 相対パスを nyamy.exe ディレクトリ基準で解決
    resolveRelativePath(cmdLine, exeDir);

    // 4. シンボルを引数として追加
    for (auto& s : syms)
        cmdLine += L" -D" + s;

    // 5. CreateProcess
    BOOL result = CreateProcess(NULL, cmdLine.data(), ...);
}
```

### execUserFunc() — 実装済み

`execUserFunc()` は `CtrlStreamWriter::writeExecUserFunc()` を呼び出す形で実装済み。
詳細は「CtrlStream の現在の実装」セクションを参照。

---

## CtrlStream の現在の実装 (実装済み)

`ctrl_stream.h`、`ctrl_stream_writer.cpp/h`、`scripter/ctrl_stream_reader.cpp/h` はすべて実装済み。

```cpp
// ctrl_stream.h (現在)
enum class CtrlId : uint8_t {
    Start        = 0x01,  ///< (Re)compile with symbols; sent on every scripter startup
    ExecUserFunc = 0x02,  ///< Engine -> scripter: invoke user-defined function
    Quit         = 0xFF,
};
```

```cpp
// ctrl_stream_writer.h (nyamy 側)
class CtrlStreamWriter {
public:
    void writeStart(const Symbols &syms);
    void writeExecUserFunc(const wstringi &funcName,
                           const std::vector<FuncArg> &args,
                           const TriggerInfo &ctx);
    void writeQuit();
};
```

```cpp
// scripter/ctrl_stream_reader.h (scripter 側)
class CtrlStreamReader {
public:
    bool readNext(CtrlId &ctrlId);
    Symbols readStart();

    struct ExecUserFuncData {
        wstringi             name;
        std::vector<FuncArg> args;
        TriggerInfo          context;
    };
    ExecUserFuncData readExecUserFunc();
};
```

## CtrlStream の将来の変更 (未実装)

プロセス再起動方式では `Start` が不要になる (`ctrl_stream.h` から削除)。
`writeStart` / `readStart` も削除される。

---

## nyamy.ini の将来の変更 (未実装)

3 箇所 (ルート / Release / Debug) に以下のセクションを追加。

```ini
[nyamy-scripter]
; scripter 起動コマンド。省略時: nyamy-scripter.exe を使用
; 相対パス: nyamy.exe のあるディレクトリからの相対
; ${VAR}: 環境変数展開
; command=nyamy-scripter.exe
; command=${LOCALAPPDATA}\MyApp\custom-scripter.exe --option
; command=python ${APPDATA}\nyamy\myscripter.py
```

---

## 未決事項

- `Engine::dispatchScripterFunc()` の実装詳細 (スレッド安全性、エラー処理)
- `TriggerInfo` を Engine が KBDLLHOOKSTRUCT 等から構築するコード
- ExecUserFunc を発行するトリガー: .mayu 言語の `&ExecUserFunc("name")` をどう表現するか

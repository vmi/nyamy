# yamy 側の変更仕様

## 実装済みの変更

### パイプ構成の変更

```
旧: stdin=CtrlStream, stdout=CmdStream, stderr=ログ
新: stdin=NUL, stdout+stderr=msgパイプ(マージ), --ctrl=N でCtrlStream, --cmd=M でCmdStream
```

scripter の stdin は NUL デバイス (即 EOF)。CtrlStream / CmdStream は
継承ハンドルをコマンドライン引数 `--ctrl=N` / `--cmd=M` で渡す。
これにより scripter 実装が `printf` / `std::cout` 等を使っても CmdStream を汚染しない。

### `pipe_streambuf.h` — 新規追加

`PipeWriteStreambuf` / `PipeReadStreambuf` / `PipeReadWStreambuf` を
`pipe_streambuf.h` に集約。`scripter_manager.cpp` と `yamy_scripter.cpp` の
重複実装を除去した。

### `ScripterManager` インターフェース (現状)

```cpp
class ScripterManager
{
public:
    ScripterManager(SyncObject *i_soLog, std::wostream *i_log, HWND i_hwndNotify);
    ~ScripterManager();

    bool start();                        // プロセス起動・パイプ確立
    void sendQuit();                     // Quit 送信 (non-blocking, idempotent)
    DWORD collectHandles(HANDLE*, DWORD); // WaitForMultipleObjects 用
    void closeHandles();

    void reload(const Symbols &syms);    // CtrlStream: Reload(syms) 送信

    std::unique_ptr<Setting> takeNewSetting();
    static const UINT WM_ScripterSettingReady;

private:
    HANDLE m_hCtrlWrite;    ///< yamy -> scripter (CtrlStream, non-stdio)
    HANDLE m_hDataRead;     ///< scripter -> yamy (CmdStream, non-stdio)
    HANDLE m_hMsgRead;      ///< scripter stdout+stderr -> yamy (log, merged)
    HANDLE m_hScripterProcess;
    HANDLE m_hDataThread;
    HANDLE m_hMsgThread;    ///< (旧 m_hStderrThread)
    // ...
};
```

`reload(syms)` は CtrlStream の Reload コマンド (0x01) でシンボルセットを送信する。
scripter 側は受信後に .mayu を再コンパイルし、CmdStream (cmd パイプ) を返す。

`m_hMsgRead` は scripter の stdout+stderr をマージした msg パイプ。
`msgThread` / `runMsgReader()` が `PipeReadWStreambuf` (wchar_t 単位) で読み取り、
1 行ずつ yamy のログウィンドウに表示する。

### `start()` — コマンドライン構築

```cpp
wchar_t cmdLine[1024];
swprintf_s(cmdLine,
           L"\"%s\" --ctrl=%llu --cmd=%llu",
           scripterPath.c_str(),
           (unsigned long long)(uintptr_t)hCtrlRead,
           (unsigned long long)(uintptr_t)hDataWrite);

si.hStdInput  = hNul;      // NUL (即 EOF)
si.hStdOutput = hMsgWrite; // ログパイプ
si.hStdError  = hMsgWrite; // 同じパイプにマージ

CreateProcess(NULL, cmdLine, NULL, NULL, TRUE /*bInheritHandles*/, ...);
```

---

## 変更対象ファイル (未実装部分)

以下は設計済みだが未実装の変更。

- `scripter_manager.cpp/h` — reload = 再起動、callFunc 追加、ExecFunc 処理
- `ctrl_stream_writer.cpp/h` — writeReload 削除、writeCallFunc 追加
- `ctrl_stream.h` — Reload 削除、CallFunc (0x01) 追加
- `cmd_stream.h` — ExecFunc (0x30) 追加
- `cmd_stream_reader.cpp/h` — ExecFunc 読み取り
- `cmd_processor.h/cpp` — ExecFunc コールバック追加
- `mayu.cpp` — WM_ScripterExecFunc ハンドラ追加
- `yamy.ini` — [yamy-scripter] セクション追加

---

## ScripterManager の将来の変更 (未実装)

### インターフェース変更

```cpp
class ScripterManager
{
public:
    // start(syms) に変更 (コマンドラインにシンボルを付加して起動)
    bool start(const Symbols& syms);

    // reload = 旧プロセス終了 + 新プロセス起動
    void reload(const Symbols& syms);  // sendQuit() + start(syms)

    // ユーザー定義関数の呼び出し (fire-and-forget)
    void callFunc(const wstringi& name,
                  const std::vector<YamyArg>& args,
                  const YamyFuncCallContext& ctx_info);

    std::unique_ptr<Setting> takeNewSetting();
    static const UINT WM_ScripterSettingReady;
    static const UINT WM_ScripterExecFunc;      // 新規追加
};
```

### start() の将来形

```cpp
bool ScripterManager::start(const Symbols& syms)
{
    // 1. yamy.ini から command= を読み取り
    wstringi cmdLine = readScripterCommand(exeDir);
    if (cmdLine.empty())
        cmdLine = L"yamy-scripter.exe";

    // 2. ${ENV_VAR} 展開
    cmdLine = expandEnvVars(cmdLine);

    // 3. 相対パスを yamy.exe ディレクトリ基準で解決
    resolveRelativePath(cmdLine, exeDir);

    // 4. シンボルを引数として追加
    for (auto& s : syms)
        cmdLine += L" -D" + s;

    // 5. CreateProcess
    BOOL result = CreateProcess(NULL, cmdLine.data(), ...);
}
```

### callFunc() の将来形

```cpp
void ScripterManager::callFunc(const wstringi& name,
                                const std::vector<YamyArg>& args,
                                const YamyFuncCallContext& ctx_info)
{
    if (!m_ctrlWriter) return;
    try {
        m_ctrlWriter->writeCallFunc(name, args, ctx_info);
    } catch (...) {}
}
```

---

## CtrlStream の将来の変更 (未実装)

### ctrl_stream.h

```cpp
enum class CtrlId : uint8_t {
    CallFunc = 0x01,  // 旧 Reload 0x01 を置き換え
    Quit     = 0xFF,  // 変更なし
};
```

### ctrl_stream_writer.h/cpp

```cpp
class CtrlStreamWriter
{
public:
    // 削除: writeReload()
    // 追加:
    void writeCallFunc(const wstringi& name,
                       const std::vector<YamyArg>& args,
                       const YamyFuncCallContext& ctx_info);
    void writeQuit();  // 変更なし
};
```

### ctrl_stream_reader.h/cpp (scripter 側)

```cpp
class CtrlStreamReader
{
public:
    bool readNext(CtrlId& ctrlId);
    // 削除: readReload()
    // 追加:
    void readCallFunc(wstringi& name,
                      std::vector<YamyArg>& args,
                      YamyFuncCallContext& ctx_info);
};
```

---

## CmdStream の将来の変更 (未実装)

### cmd_stream.h

```cpp
enum class CmdId : uint8_t {
    // 既存は変更なし (0x01〜0x25, 0xFF)
    ExecFunc = 0x30,  // 新規追加
};
```

### cmd_stream_reader.h/cpp

```cpp
struct CmdExecFuncData {
    wstringi                name;
    std::vector<YamyArg>    args;
};

using AnyCmd = std::variant<
    // 既存型...
    CmdExecFuncData   // 新規追加
>;
```

---

## CmdProcessor の将来の変更 (未実装)

```cpp
class CmdProcessor
{
public:
    // 既存: onCommit()
    // 追加:
    void onExecFunc(std::function<void(const wstringi& name,
                                       const std::vector<YamyArg>& args)> cb);
};
```

### データスレッドの動作変更

Commit 受信後もパイプを読み続け、ExecFunc を処理する:

```cpp
void ScripterManager::runReader()
{
    // ...
    processor.onCommit([this](std::unique_ptr<Setting> s) {
        { lock; m_pendingSetting = move(s); }
        PostMessage(m_hwndNotify, WM_ScripterSettingReady, 0, 0);
        // Commit 後もループ継続 (return しない)
    });

    processor.onExecFunc([this](const wstringi& name,
                                 const std::vector<YamyArg>& args) {
        auto* p = new ExecFuncPayload{name, args};
        PostMessage(m_hwndNotify, WM_ScripterExecFunc,
                    0, reinterpret_cast<LPARAM>(p));
    });

    processor.process(reader);  // パイプ EOF まで読み続ける
}
```

---

## mayu.cpp の将来の変更 (未実装)

```cpp
case WM_APP_scripterExecFunc: {
    auto* p = reinterpret_cast<ExecFuncPayload*>(lParam);
    std::unique_ptr<ExecFuncPayload> payload(p);
    This->m_engine.dispatchScripterFunc(payload->name, payload->args);
    return 0;
}
```

---

## yamy.ini の将来の変更 (未実装)

3 箇所 (ルート / Release / Debug) に以下のセクションを追加。

```ini
[yamy-scripter]
; scripter 起動コマンド。省略時: yamy-scripter.exe を使用
; 相対パス: yamy.exe のあるディレクトリからの相対
; ${VAR}: 環境変数展開
; command=yamy-scripter.exe
; command=${LOCALAPPDATA}\MyApp\custom-scripter.exe --option
; command=python ${APPDATA}\yamy\myscripter.py
```

---

## 未決事項

- `Engine::dispatchScripterFunc()` の実装詳細 (スレッド安全性、エラー処理)
- `YamyFuncCallContext` を Engine が KBDLLHOOKSTRUCT 等から構築するコード
- CallFunc を発行するトリガー: .mayu 言語の `&ScriptFunc("name")` をどう表現するか

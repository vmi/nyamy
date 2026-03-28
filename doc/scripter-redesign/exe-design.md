# EXE 設計 (薄いラッパー)

## 概要

`yamy-scripter.exe` は DLL の `scripter_engine(argc, argv)` を呼ぶだけの薄いラッパー。
将来 mruby などの言語ランタイムを内蔵した EXE も同じ設計パターンで作れる。

---

## 現在の実装

### `scripter/main.cpp`

```cpp
#include "yamy_scripter.h"

int wmain(int argc, wchar_t *argv[])
{
    scripter_engine(argc, argv);
    return 0;
}
```

### `scripter/yamy_scripter.h`

```c
#ifdef _YAMY_SCRIPTER_IMPL
#  define SCRIPTER_API __declspec(dllexport)
#else
#  define SCRIPTER_API __declspec(dllimport)
#endif

extern "C" SCRIPTER_API void scripter_engine(int argc, wchar_t *argv[]);
```

### `scripter/yamy_scripter.cpp` — `scripter_engine()` の実装

継承ハンドルの番号を環境変数 `YSCR_CTRL` / `YSCR_CMD` から取得し、
それぞれ CtrlStream / CmdStream の通信チャネルとして使用する。
stdin/stdout/stderr はバイナリプロトコルに使用しない。

```cpp
SCRIPTER_API void scripter_engine(int argc, wchar_t *argv[])
{
    // 環境変数 YSCR_CTRL / YSCR_CMD からハンドルを取得
    HANDLE hCtrlRead  = INVALID_HANDLE_VALUE;
    HANDLE hDataWrite = INVALID_HANDLE_VALUE;
    {
        wchar_t buf[32];
        if (GetEnvironmentVariableW(L"YSCR_CTRL", buf, 32) > 0)
            hCtrlRead  = reinterpret_cast<HANDLE>(
                             static_cast<uintptr_t>(wcstoull(buf, nullptr, 10)));
        if (GetEnvironmentVariableW(L"YSCR_CMD",  buf, 32) > 0)
            hDataWrite = reinterpret_cast<HANDLE>(
                             static_cast<uintptr_t>(wcstoull(buf, nullptr, 10)));
    }

    if (hCtrlRead == INVALID_HANDLE_VALUE || hDataWrite == INVALID_HANDLE_VALUE) {
        _setmode(_fileno(stderr), _O_U16TEXT);
        std::wcerr << L"error: YSCR_CTRL and YSCR_CMD environment variables are required" << std::endl;
        return;
    }

    // stderr を UTF-16 に設定 (wcerr のログを yamy が wchar_t 単位で読む)
    _setmode(_fileno(stderr), _O_U16TEXT);

    // 継承ハンドルを streambuf でラップ
    PipeReadStreambuf  ctrlBuf(hCtrlRead);
    PipeWriteStreambuf dataBuf(hDataWrite);
    std::istream ctrlStream(&ctrlBuf);
    std::ostream dataStream(&dataBuf);

    CtrlStreamReader ctrlReader(ctrlStream);
    CmdStreamWriter  dataWriter(dataStream);

    for (;;) {
        CtrlId id;
        if (!ctrlReader.readNext(id))
            break;  // ctrl pipe closed -> exit

        if (id == CtrlId::Quit) {
            break;
        } else if (id == CtrlId::Start) {
            Symbols syms = ctrlReader.readStart();
            doCompile(syms, dataWriter);
            dataStream.flush();
        } else if (id == CtrlId::ExecUserFunc) {
            auto req = ctrlReader.readExecUserFunc();
            execUserFunc(req.name, req.args, req.context);
        }
    }

    CloseHandle(hCtrlRead);
    CloseHandle(hDataWrite);
}
```

`doCompile()` は ConfigFiles → MayuParser → MayuCompiler → CmdStreamWriter の順に処理。
エラー時は Commit を書かず、`std::wcerr` に報告 (msg パイプ経由でログに表示される)。

---

## pipe_streambuf.h

`PipeWriteStreambuf` / `PipeReadStreambuf` / `PipeReadWStreambuf` は
`pipe_streambuf.h` に集約されており、`scripter_manager.cpp` と
`yamy_scripter.cpp` の両方からインクルードする。

```
pipe_streambuf.h
  PipeWriteStreambuf   : Win32 HANDLE → std::streambuf (書き込み)
  PipeReadStreambuf    : Win32 HANDLE → std::streambuf (読み込み、char)
  PipeReadWStreambuf   : Win32 HANDLE → std::wstreambuf (読み込み、wchar_t)
```

---

## 将来の設計 (未実装)

### argv シンボル渡し (プロセス再起動方式)

`-D` フラグでシンボルを渡す。ハンドルは引き続き `YSCR_CTRL`/`YSCR_CMD` 環境変数で渡す。

```
yamy-scripter.exe [-DSYM1 [-DSYM2 ...]]
```

### .mayu EXE の main.cpp (将来形)

ハンドルは DLL が `YSCR_CTRL`/`YSCR_CMD` 環境変数から取得するため (現行実装と同様)、
main.cpp は callbacks を渡して `ys_start()` を呼ぶだけでよい。

```cpp
#include "yamy_scripter.h"

static bool on_load_setting()
{
    return ys_load_mayu();  // .mayu ファイルをコンパイルしてキューに積む
}

int wmain(int argc, wchar_t* argv[])
{
    YsCallbacks cb = {};
    cb.on_load_setting = on_load_setting;
    return ys_start(&cb);
    // 返り値: 0 = Engine から終了コマンド受信, 1 = on_load_setting が false を返した
}
```

### mruby 内蔵 EXE のパターン (将来実装)

```cpp
#include "yamy_scripter.h"
#include "mruby.h"

static mrb_state* g_mrb = nullptr;

static bool on_load_setting()
{
    // mruby スクリプト実行 → ys_* API 呼び出しで設定を構築
    mrb_load_file(g_mrb, ...);
    return mrb_nil_p(g_mrb->exc);
}

static void on_exec_user_func(const char* name,
                               const YsFuncArgs* preset_args,
                               const YsTriggerInfo* trigger_info)
{
    mrb_funcall(g_mrb, mrb_top_self(g_mrb), name, 0);
}

int wmain(int argc, wchar_t* argv[])
{
    g_mrb = mrb_open();
    bind_ys_to_mruby(g_mrb);  // ys_* 関数を mruby モジュールとして登録

    YsCallbacks cb = {};
    cb.on_load_setting   = on_load_setting;
    cb.on_exec_user_func = on_exec_user_func;
    int ret = ys_start(&cb);

    mrb_close(g_mrb);
    return ret;
}
```

### FFI スクリプトのパターン (将来)

外部スクリプト (Python/Ruby) を yamy が直接 CreateProcess で起動する場合、
ハンドルは DLL が環境変数から自動取得するため、スクリプト側の処理は最小限になる。

```python
import ctypes

lib = ctypes.CDLL("yamy-scripter.dll")

LoadSettingFn  = ctypes.CFUNCTYPE(ctypes.c_bool)
ExecUserFuncFn = ctypes.CFUNCTYPE(None, ctypes.c_char_p,   # name
                                        ctypes.c_void_p,   # preset_args (YsFuncArgs*)
                                        ctypes.c_void_p)   # trigger_info (YsTriggerInfo*)

class YsCallbacks(ctypes.Structure):
    _fields_ = [
        ("on_load_setting",   LoadSettingFn),
        ("on_exec_user_func", ExecUserFuncFn),
    ]

def on_load_setting():
    return bool(lib.ys_load_mayu())  # .mayu コンパイル

def on_exec_user_func(name, preset_args, trigger_info):
    pass  # ユーザー定義関数の実行

cb = YsCallbacks(LoadSettingFn(on_load_setting), ExecUserFuncFn(on_exec_user_func))
lib.ys_start.restype = ctypes.c_int
lib.ys_start(ctypes.byref(cb))
```

---

## ソース構成 (現状)

```
scripter/main.cpp                 ← EXE エントリポイント (scripter_engine(argc,argv) を呼ぶだけ)
scripter/yamy_scripter.h/cpp      ← DLL エントリポイント (scripter_engine 実装)
scripter/ctrl_stream_reader.cpp/h ← DLL に含まれる (Reload 読み取り)
scripter/cmd_stream_writer.cpp/h  ← DLL に含まれる
scripter/lexer.cpp/h              ← DLL に含まれる
scripter/mayu_parser.cpp/h        ← DLL に含まれる
scripter/mayu_compiler.cpp/h      ← DLL に含まれる
scripter/config_files.cpp/h       ← DLL に含まれる
pipe_streambuf.h                  ← DLL/EXE 共通 streambuf ユーティリティ
compiler_specific_func.cpp        ← DLL に含まれる
registry.cpp                      ← DLL に含まれる
stringtool.cpp                    ← DLL に含まれる
windowstool.cpp                   ← DLL に含まれる
ctrl_stream_writer.cpp/h          ← yamy 側 (root)、DLL には含まれない
```

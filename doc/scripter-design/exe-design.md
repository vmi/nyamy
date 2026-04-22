# EXE 設計 (薄いラッパー)

## 概要

`yamy-scripter.exe` は mruby ランタイムを内蔵した薄いラッパー EXE。
C API (`ys_*`) 本体と .mayu コンパイラは `yamy-scripter.dll` 側にあり、
EXE は `<ProjectReference>` で DLL をリンクして `ys_start` 等を import する
(`mruby_main.cpp` / `mruby_binding.cpp` のみをコンパイル)。
`mruby_main.cpp` が `YsCallbacks` / `MRubyContext` を設定して `ys_start(&callbacks, &ctx)` を呼ぶ。
スクリプトパスは `argv[1]` で渡す。省略時はホームディレクトリの `.mayu.rb` を探索する。

なお `ys_start()` をはじめとする C API の実装 (`yamy_scripter.cpp`) は DLL 側にある。
以下で示す `ys_start()` の実装は DLL の内部動作だが、EXE の起動シーケンス理解のため併記する。

---

## 現在の実装

### `scripter/mruby_main.cpp`

UTF-8 activeCodePage マニフェスト (`mruby_main.manifest`) で `main` は UTF-8 引数を受け取る。
mruby 状態の初期化 (`mrb_open`) はコールバック `mruby_on_load_setting` 内で行われる。

```cpp
#include "yamy_scripter.h"
#include "mruby_binding.h"
#include <windows.h>

int main(int argc, char *argv[])
{
    MRubyContext ctx = { argc, (const char* const*)argv, nullptr };

    YsCallbacks callbacks = {};
    callbacks.on_load_setting = mruby_on_load_setting;
    callbacks.on_quit         = mruby_on_quit;

    return ys_start(&callbacks, &ctx);
}
```

### `scripter/yamy_scripter.cpp` — `ys_start()` の実装

継承ハンドルの番号を環境変数 `YS_CTRL` / `YS_CMD` から取得し、
それぞれ CtrlStream / CmdStream の通信チャネルとして使用する。
stdin/stdout/stderr はバイナリプロトコルに使用しない。

```cpp
YS_API int ys_start(const YsCallbacks* callbacks, void* exeCtx)
{
    // 環境変数 YS_CTRL / YS_CMD からハンドルを取得
    // CtrlStream ループ:
    //   Start(syms)     → callbacks->on_load_setting(exeCtx) 呼び出し → CmdStream 送出
    //   ExecUserFunc    → ys_reg_user_func で登録したハンドラを呼び出す
    //   Quit / EOF      → callbacks->on_quit(exeCtx); return 0
    // on_load_setting が false → return 1
}
```

エラー時は Commit を書かず `stderr` に出力 (msg パイプ経由でログに表示される)。

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

`-D` フラグでシンボルを渡す。ハンドルは引き続き `YS_CTRL`/`YS_CMD` 環境変数で渡す。

```
yamy-scripter.exe [-DSYM1 [-DSYM2 ...]]
```

### FFI スクリプトのパターン

外部スクリプト (Python/Ruby) を yamy が直接 CreateProcess で起動する場合、
ハンドルは `ys_start` が環境変数から自動取得するため、スクリプト側の処理は最小限になる。
使用例は [typed-args.md](typed-args.md) の FFI セクションを参照。

---

## ソース構成 (現状)

```
# --- yamy-scripter.exe (mruby ラッパー、DLL をリンク) ---
scripter/mruby_main.cpp           ← EXE エントリポイント (mruby 内蔵)
scripter/mruby_main.manifest      ← UTF-8 activeCodePage マニフェスト
scripter/mruby_binding.cpp/h      ← mruby DSL (Yamy::DSL / KeySeq / KeyMap 等)

# --- yamy-scripter.dll (公開 C API + .mayu コンパイラ) ---
scripter/yamy_scripter.h          ← 公開 C API 宣言 (EXE/FFI からも include)
scripter/yamy_scripter.cpp        ← C API 実装 (ys_start / ys_reg_keyseq 等)
scripter/ys_types.h               ← 内部型 (YsFuncArg / YsFuncArgs / YsStrs)
scripter/ctrl_stream_reader.cpp/h ← CtrlStream デシリアライズ
scripter/cmd_stream_writer.cpp/h  ← CmdStream シリアライズ
scripter/lexer.cpp/h              ← .mayu レキサー
scripter/mayu_parser.cpp/h        ← .mayu パーサー
scripter/mayu_compiler.cpp/h      ← .mayu コンパイラー
scripter/config_files.cpp/h       ← 設定ファイルパス解決

# --- 共通 / yamy 本体側 ---
pipe_streambuf.h                  ← DLL/yamy 共通 streambuf ユーティリティ
ctrl_stream_writer.cpp/h          ← yamy 側 (root)、scripter には含まれない
```

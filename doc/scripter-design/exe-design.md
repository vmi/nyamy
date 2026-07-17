# EXE 設計 (薄いラッパー)

## 概要

`nyamy-scripter.exe` は mruby ランタイムを内蔵した薄いラッパー EXE。
C API (`nys_*`) 本体と .mayu コンパイラは `nyamy-scripter.dll` 側にあり、
EXE は `<ProjectReference>` で DLL をリンクして `nys_start` 等を import する
(`mruby_main.cpp` / `mruby_binding.cpp` のみをコンパイル)。
`mruby_main.cpp` が `NYsCallbacks` / `MRubyContext` を設定して `nys_start(&callbacks, &ctx)` を呼ぶ。
スクリプトパスは `argv[1]` で渡す。省略時はホームディレクトリの `.mayu.rb` を探索する。

なお `nys_start()` をはじめとする C API の実装 (`nyamy_scripter.cpp`) は DLL 側にある。
以下で示す `nys_start()` の実装は DLL の内部動作だが、EXE の起動シーケンス理解のため併記する。

---

## 現在の実装

### `scripter/mruby_main.cpp`

UTF-8 activeCodePage マニフェスト (`mruby_main.manifest`) で `main` は UTF-8 引数を受け取る。
mruby 状態の初期化 (`mrb_open`) はコールバック `mruby_on_load_setting` 内で行われる。

```cpp
#include "nyamy_scripter.h"
#include "mruby_binding.h"
#include <windows.h>

int main(int argc, char *argv[])
{
    MRubyContext ctx = { argc, (const char* const*)argv, nullptr };

    NYsCallbacks callbacks = {};
    callbacks.on_load_setting = mruby_on_load_setting;
    callbacks.on_quit         = mruby_on_quit;

    return nys_start(&callbacks, &ctx);
}
```

### `scripter/nyamy_scripter.cpp` — `nys_start()` の実装

継承ハンドルの番号を環境変数 `NYS_CTRL` / `NYS_CMD` から取得し、
それぞれ CtrlStream / CmdStream の通信チャネルとして使用する。
stdin/stdout/stderr はバイナリプロトコルに使用しない。

```cpp
NYS_API int nys_start(const NYsCallbacks* callbacks, void* exeCtx)
{
    // 環境変数 NYS_CTRL / NYS_CMD からハンドルを取得
    // CtrlStream ループ:
    //   Start(syms)     → callbacks->on_load_setting(exeCtx) 呼び出し → CmdStream 送出
    //   ExecUserFunc    → nys_reg_user_func で登録したハンドラを呼び出す
    //   Quit / EOF      → callbacks->on_quit(exeCtx); return 0
    // on_load_setting が false → return 1
}
```

エラー時は Commit を書かず `stderr` に出力 (msg パイプ経由でログに表示される)。

---

## pipe_streambuf.h

`PipeWriteStreambuf` / `PipeReadStreambuf` / `PipeReadWStreambuf` は
`pipe_streambuf.h` に集約されており、`scripter_manager.cpp` と
`nyamy_scripter.cpp` の両方からインクルードする。

```
pipe_streambuf.h
  PipeWriteStreambuf   : Win32 HANDLE → std::streambuf (書き込み)
  PipeReadStreambuf    : Win32 HANDLE → std::streambuf (読み込み、char)
  PipeReadWStreambuf   : Win32 HANDLE → std::wstreambuf (読み込み、wchar_t)
```

---

## 将来の設計 (未実装)

### argv シンボル渡し (プロセス再起動方式)

`-D` フラグでシンボルを渡す。ハンドルは引き続き `NYS_CTRL`/`NYS_CMD` 環境変数で渡す。

```
nyamy-scripter.exe [-DSYM1 [-DSYM2 ...]]
```

### FFI スクリプトのパターン

外部スクリプト (Python/Ruby) を nyamy が直接 CreateProcess で起動する場合、
ハンドルは `nys_start` が環境変数から自動取得するため、スクリプト側の処理は最小限になる。
使用例は [typed-args.md](typed-args.md) の FFI セクションを参照。

---

## ソース構成 (現状)

```
# --- nyamy-scripter.exe (mruby ラッパー、DLL をリンク) ---
scripter/mruby_main.cpp           ← EXE エントリポイント (mruby 内蔵)
scripter/mruby_main.manifest      ← UTF-8 activeCodePage マニフェスト
scripter/mruby_binding.cpp/h      ← mruby DSL (NYamy::DSL / KeySeq / KeyMap 等)

# --- nyamy-scripter.dll (公開 C API + .mayu コンパイラ) ---
scripter/nyamy_scripter.h          ← 公開 C API 宣言 (EXE/FFI からも include)
scripter/nyamy_scripter.cpp        ← C API 実装 (nys_start / nys_reg_keyseq 等)
scripter/nys_types.h               ← 内部型 (NYsFuncArg / NYsFuncArgs / NYsStrs)
scripter/ctrl_stream_reader.cpp/h ← CtrlStream デシリアライズ
scripter/cmd_stream_writer.cpp/h  ← CmdStream シリアライズ
scripter/lexer.cpp/h              ← .mayu レキサー
scripter/mayu_parser.cpp/h        ← .mayu パーサー
scripter/mayu_compiler.cpp/h      ← .mayu コンパイラー
scripter/config_files.cpp/h       ← 設定ファイルパス解決

# --- 共通 / nyamy 本体側 ---
pipe_streambuf.h                  ← DLL/nyamy 共通 streambuf ユーティリティ
ctrl_stream_writer.cpp/h          ← nyamy 側 (root)、scripter には含まれない
```

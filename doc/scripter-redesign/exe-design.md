# EXE 設計 (薄いラッパー)

## 概要

`yamy-scripter.exe` は mruby ランタイムを内蔵した EXE。
`mruby_main.cpp` が `ys_start(mruby_on_load_setting)` を呼ぶ。
スクリプトパスは `--script=path.rb` または `argv[1]` で渡す。

---

## 現在の実装

### `scripter/mruby_main.cpp`

UTF-8 activeCodePage マニフェスト (`mruby_main.manifest`) で `main` は UTF-8 引数を受け取る。

```cpp
#include "yamy_scripter.h"
#include "mruby_binding.h"
#include <mruby.h>

int main(int argc, char *argv[])
{
    const char *script = findScriptPath(argc, argv);  // --script=path.rb または argv[1]
    if (!script) {
        fprintf(stderr,
            "error: script file required\n"
            "usage: yamy-scripter.exe [--script=]path.rb\n");
        return 1;
    }

    mrb_state *mrb = mrb_open();
    yamy_mruby_init(mrb);                   // Yamy::DSL 等を登録
    yamy_mruby_set_script(mrb, script);

    int ret = ys_start(mruby_on_load_setting);

    mrb_close(mrb);
    return ret;
}
```

### `scripter/yamy_scripter.cpp` — `ys_start()` の実装

継承ハンドルの番号を環境変数 `YSCR_CTRL` / `YSCR_CMD` から取得し、
それぞれ CtrlStream / CmdStream の通信チャネルとして使用する。
stdin/stdout/stderr はバイナリプロトコルに使用しない。

```cpp
YS_API int ys_start(ys_on_load_setting on_load_setting)
{
    // 環境変数 YSCR_CTRL / YSCR_CMD からハンドルを取得
    // CtrlStream ループ:
    //   Start(syms)     → on_load_setting() 呼び出し → CmdStream 送出
    //   ExecUserFunc    → ys_reg_user_func で登録したハンドラを呼び出す
    //   Quit / EOF      → return 0
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

`-D` フラグでシンボルを渡す。ハンドルは引き続き `YSCR_CTRL`/`YSCR_CMD` 環境変数で渡す。

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
scripter/mruby_main.cpp           ← EXE エントリポイント (mruby 内蔵)
scripter/mruby_main.manifest      ← UTF-8 activeCodePage マニフェスト
scripter/mruby_binding.cpp/h      ← mruby DSL (Yamy::DSL / KeySeq / KeyMap 等)
scripter/yamy_scripter.h          ← 公開 C API 宣言
scripter/yamy_scripter.cpp        ← C API 実装 (ys_start / ys_reg_keyseq 等)
scripter/ys_types.h               ← 内部型 (YsFuncArg / YsFuncArgs / YsStrs)
scripter/ctrl_stream_reader.cpp/h ← CtrlStream デシリアライズ
scripter/cmd_stream_writer.cpp/h  ← CmdStream シリアライズ
scripter/lexer.cpp/h              ← .mayu レキサー
scripter/mayu_parser.cpp/h        ← .mayu パーサー
scripter/mayu_compiler.cpp/h      ← .mayu コンパイラー
scripter/config_files.cpp/h       ← 設定ファイルパス解決
pipe_streambuf.h                  ← EXE/yamy 共通 streambuf ユーティリティ
ctrl_stream_writer.cpp/h          ← yamy 側 (root)、scripter には含まれない
```

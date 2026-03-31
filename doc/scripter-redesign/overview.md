# yamy-scripter 再設計 — 概要

## 背景と目的

yamy-scripter はかつて、.mayu ファイルをコンパイルして CmdStream を生成する
コンソール EXE (yamy-scripter32/64.exe) として実装されていた。

### 変更目的

凡例: ✅ 実装済み / 🔲 未実装 (設計済み)

1. **C API + mruby 内蔵 EXE** ✅: 公開 C API (`ys_*`) を `yamy_scripter.cpp` に実装し、
   mruby ランタイムを内蔵した EXE (`yamy-scripter.exe`) として提供する。
   Python/Ruby など他言語からも FFI 経由で同じ C API を利用できる。
2. **stdout/stderr 隔離** ✅: CtrlStream/CmdStream を非 stdio のパイプ (環境変数 `YSCR_CTRL`/`YSCR_CMD`) で渡し、
   scripter の stdin=NUL / stdout+stderr=ログパイプとする。
   scripter 実装が `printf` / `std::cout` を使っても CmdStream を汚染しない。
3. **コマンドキューイング** ✅: Def 系コマンドは Commit まで内部でキューイングし、
   `on_load_setting` が true を返したときのみ yamy へ一括送出する。
4. **ユーザー定義関数** ✅: `ys_reg_user_func` で関数を登録し、yamy からの CtrlStream
   `ExecUserFunc` コマンドでランタイムに呼び出す。mruby 側は `deffunc` で登録。
5. **プロセス再起動方式** 🔲: 設定再読み込み時は scripter プロセスを再起動する
   (CtrlId::Start を廃止、シンボルは argv で渡す)
6. **yamy.ini による起動コマンド設定** 🔲: フルコマンドライン + `${ENV_VAR}` 展開で
   任意の scripter 実装を指定可能にする

## 現在の構成 (実装済み)

scripter は mruby を内蔵した EXE として実装済み。
C API (`ys_*`) は `yamy_scripter.cpp` に実装されており、
mruby バインディング (`mruby_binding.cpp`) がそれを mruby DSL として公開する。

```
scripter/
  yamy_scripter.h           ← 公開 C API 宣言 (ys_start / ys_reg_keyseq 等)
  yamy_scripter.cpp         ← C API 実装 (CtrlStream ループ、設定キュー管理)
  ys_types.h                ← scripter 内部型 (YsFuncArg / YsFuncArgs / YsStrs)
  lexer.cpp/h
  mayu_parser.cpp/h
  mayu_compiler.cpp/h
  cmd_stream_writer.cpp/h
  config_files.cpp/h
  ctrl_stream_reader.cpp/h  ← CtrlStream デシリアライズ
  mruby_binding.cpp/h       ← mruby DSL (Yamy::DSL 等)
  mruby_main.cpp            ← EXE エントリポイント (ys_start(mruby_on_load_setting))

pipe_streambuf.h              ← PipeWriteStreambuf/PipeReadStreambuf/PipeReadWStreambuf

proj/
  yamy-scripter.vcxproj     ← yamy-scripter.exe (mruby 内蔵 EXE)
```

プロセスフロー (現状):
```
yamy → CreateProcess("yamy-scripter.exe path.rb",
                      stdin=NUL, stdout=stderr=msgPipe,
                      env: YSCR_CTRL=N, YSCR_CMD=M)

yamy → ctrl パイプ: Start(syms) → scripter → .rb 実行 → cmd パイプ: CmdStream → yamy
yamy → ctrl パイプ: ExecUserFunc → scripter → mruby proc 呼び出し → ExecKeySeq → yamy
yamy → ctrl パイプ: Quit         → scripter → 終了

yamy ← msg パイプ: ログテキスト (scripter の stdout+stderr をマージ)
```

## 想定ユースケース (将来)

| ケース | 起動コマンド (yamy.ini) | 動作 |
|--------|------------------------|------|
| mruby (.rb) ✅ | `yamy-scripter.exe path.rb` | EXE が mruby ランタイムで .rb を実行 |
| 標準 (.mayu) ✅ | `yamy-scripter.exe path.rb` (load_mayu 呼び出し) | .rb から `load_mayu` で .mayu をコンパイル |
| Python FFI 🔲 | `python myscripter.py` | Python スクリプトが ctypes で C API を呼ぶ |
| Ruby FFI 🔲 | `ruby myscripter.rb` | Ruby スクリプトが ffi gem で C API を呼ぶ |

## 成果物一覧

### 実装済み

| ファイル | 種別 | 説明 |
|---------|------|------|
| `scripter/yamy_scripter.h` | 新規 | 公開 C API 宣言 (`ys_start` / `ys_reg_keyseq` 等) |
| `scripter/yamy_scripter.cpp` | 新規 | C API 実装 (`YSCR_CTRL`/`YSCR_CMD` 取得、CtrlStream ループ、設定キュー管理) |
| `scripter/ys_types.h` | 新規 | 内部型 (`YsFuncArg` / `YsFuncArgs` / `YsStrs`) |
| `scripter/mruby_binding.cpp/h` | 新規 | mruby DSL (`Yamy::DSL` 等) |
| `scripter/mruby_main.cpp` | 新規 | EXE エントリポイント (`ys_start(mruby_on_load_setting)`) |
| `pipe_streambuf.h` | 新規 | PipeWriteStreambuf / PipeReadStreambuf / PipeReadWStreambuf |
| `proj/yamy-scripter.vcxproj` | 変更 | EXE: mruby 内蔵ビルド |

## 変更対象ファイル一覧

### 実装済み

| ファイル | 変更種別 | 主な内容 |
|---------|---------|---------|
| `scripter/yamy_scripter.h` | 新規 | 公開 C API 宣言 (`ys_start` / `ys_reg_keyseq` 等) |
| `scripter/yamy_scripter.cpp` | 新規 | `YSCR_CTRL`/`YSCR_CMD` 取得、CtrlStream ループ、設定キュー管理 |
| `scripter/ys_types.h` | 新規 | 内部型 (`YsFuncArg` / `YsFuncArgs` / `YsStrs`) |
| `scripter/mruby_binding.cpp/h` | 新規 | mruby DSL (`Yamy::DSL` 等) |
| `scripter/mruby_main.cpp` | 新規 | EXE エントリポイント (`ys_start(mruby_on_load_setting)`) |
| `pipe_streambuf.h` | 新規 | PipeWriteStreambuf / PipeReadStreambuf / PipeReadWStreambuf |
| `scripter_manager.cpp` | 変更 | 非 stdio パイプ方式、msgPipe (stdout+stderr マージ)、`YSCR_CTRL`/`YSCR_CMD` 環境変数渡し |
| `scripter_manager.h` | 変更 | `m_hStderrRead` → `m_hMsgRead`、`stderrThread` → `msgThread` |
| `proj/yamy-scripter.vcxproj` | 変更 | mruby 内蔵 EXE ビルド |

### 未実装 (設計済み)

| ファイル | 変更種別 | 主な内容 |
|---------|---------|---------|
| `scripter_manager.cpp/h` | 変更 | reload = 再起動 (プロセス再起動方式) |
| `yamy.ini` (3 箇所) | 変更 | [yamy-scripter] セクション追加 |

## 関連ドキュメント

- [protocol.md](protocol.md) — CtrlStream / CmdStream バイナリプロトコル仕様
- [c-api.md](c-api.md) — DLL 公開 C API 仕様
- [typed-args.md](typed-args.md) — 型付き引数システム (YsType / YsFuncArgs / FFI 使用例)
- [yamy-integration.md](yamy-integration.md) — yamy 側の変更 (ScripterManager / Engine)
- [build.md](build.md) — ビルドシステム変更
- [exe-design.md](exe-design.md) — EXE 設計 (薄いラッパー)

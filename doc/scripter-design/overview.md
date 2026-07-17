# nyamy-scripter 再設計 — 概要

## 背景と目的

nyamy-scripter はかつて、.mayu ファイルをコンパイルして CmdStream を生成する
コンソール EXE (nyamy-scripter32/64.exe) として実装されていた。

### 変更目的

凡例: ✅ 実装済み / 🔲 未実装 (設計済み)

1. **C API DLL + mruby 内蔵 EXE** ✅: 公開 C API (`nys_*`) を DLL (`nyamy-scripter.dll`) として提供し、
   mruby ランタイムを内蔵した EXE (`nyamy-scripter.exe`) がその DLL をリンクして使用する。
   Python/Ruby など他言語からも FFI 経由で同じ DLL の C API を直接利用できる。
2. **stdout/stderr 隔離** ✅: CtrlStream/CmdStream を非 stdio の継承パイプ (環境変数 `NYS_CTRL`/`NYS_CMD` でハンドル番号を渡す) で通信し、
   scripter の stdin=NUL / stdout+stderr=ログパイプとする。
   scripter 実装が `printf` / `std::cout` を使っても CmdStream を汚染しない。
3. **コマンドキューイング** ✅: Def 系コマンドは Commit まで内部でキューイングし、
   `on_load_setting` が true を返したときのみ nyamy へ一括送出する。
4. **ユーザー定義関数** ✅: `nys_reg_user_func` で関数を登録し、nyamy からの CtrlStream
   `ExecUserFunc` コマンドでランタイムに呼び出す。mruby 側は `deffunc` で登録。
5. **プロセス再起動方式** 🔲: 設定再読み込み時は scripter プロセスを再起動する
   (CtrlId::Start を廃止、シンボルは argv で渡す)
6. **nyamy.ini による起動コマンド設定** 🔲: フルコマンドライン + `${ENV_VAR}` 展開で
   任意の scripter 実装を指定可能にする

## 現在の構成 (実装済み)

成果物は 2 つに分かれている。

- **`nyamy-scripter.dll`** — 公開 C API (`nys_*`) と .mayu コンパイラ本体。
  `nyamy_scripter.cpp` に C API を実装し、CtrlStream ループ・設定キュー管理・
  lexer/parser/compiler を内包する。FFI クライアント (Python/Ruby) はこの DLL を直接ロードする。
- **`nyamy-scripter.exe`** — mruby ランタイムを内蔵した薄いラッパー。
  `mruby_main.cpp` / `mruby_binding.cpp` のみをコンパイルし、`nyamy-scripter.dll` をリンクして
  C API を import する。mruby バインディングが C API を mruby DSL として公開する。

```
scripter/
  # --- nyamy-scripter.dll に入る (C API + .mayu コンパイラ) ---
  nyamy_scripter.h           ← 公開 C API 宣言 (nys_start / nys_reg_keyseq 等)
  nyamy_scripter.cpp         ← C API 実装 (CtrlStream ループ、設定キュー管理)
  nys_types.h                ← scripter 内部型 (NYsFuncArg / NYsFuncArgs / NYsStrs)
  lexer.cpp/h
  mayu_parser.cpp/h
  mayu_compiler.cpp/h
  cmd_stream_writer.cpp/h
  config_files.cpp/h
  ctrl_stream_reader.cpp/h  ← CtrlStream デシリアライズ
  # --- nyamy-scripter.exe に入る (mruby ラッパー、DLL をリンク) ---
  mruby_binding.cpp/h       ← mruby DSL (NYamy::DSL 等)
  mruby_main.cpp            ← EXE エントリポイント (NYsCallbacks + MRubyContext を設定し nys_start(&callbacks, &ctx) を呼ぶ)

pipe_streambuf.h              ← PipeWriteStreambuf/PipeReadStreambuf/PipeReadWStreambuf

proj/
  nyamy-scripter-dll.vcxproj ← nyamy-scripter.dll (C API + .mayu コンパイラ, DynamicLibrary)
  nyamy-scripter.vcxproj     ← nyamy-scripter.exe (mruby 内蔵 EXE, DLL を ProjectReference)
```

プロセスフロー (現状):
```
nyamy → CreateProcess("nyamy-scripter.exe path.rb",
                      stdin=NUL, stdout=stderr=msgPipe,
                      env: NYS_CTRL=N, NYS_CMD=M)

nyamy → ctrl パイプ: Start(syms) → scripter → .rb 実行 → cmd パイプ: CmdStream → nyamy
nyamy → ctrl パイプ: ExecUserFunc → scripter → mruby proc 呼び出し → ExecKeySeq → nyamy
nyamy → ctrl パイプ: Quit         → scripter → 終了

nyamy ← msg パイプ: ログテキスト (scripter の stdout+stderr をマージ)
```

## 想定ユースケース (将来)

| ケース | 起動コマンド (nyamy.ini) | 動作 |
|--------|------------------------|------|
| mruby (.rb) ✅ | `nyamy-scripter.exe path.rb` | EXE が mruby ランタイムで .rb を実行 |
| 標準 (.mayu) ✅ | `nyamy-scripter.exe path.rb` (load_mayu 呼び出し) | .rb から `load_mayu` で .mayu をコンパイル |
| Python FFI 🔲 | `python myscripter.py` | Python スクリプトが ctypes で C API を呼ぶ |
| Ruby FFI 🔲 | `ruby myscripter.rb` | Ruby スクリプトが ffi gem で C API を呼ぶ |

## 成果物一覧

### 実装済み

| ファイル | 種別 | 説明 |
|---------|------|------|
| `scripter/nyamy_scripter.h` | 新規 | 公開 C API 宣言 (`nys_start` / `nys_reg_keyseq` 等) |
| `scripter/nyamy_scripter.cpp` | 新規 | C API 実装 (`NYS_CTRL`/`NYS_CMD` 取得、CtrlStream ループ、設定キュー管理) |
| `scripter/nys_types.h` | 新規 | 内部型 (`NYsFuncArg` / `NYsFuncArgs` / `NYsStrs`) |
| `scripter/mruby_binding.cpp/h` | 新規 | mruby DSL (`NYamy::DSL` 等) |
| `scripter/mruby_main.cpp` | 新規 | EXE エントリポイント (`NYsCallbacks` + `MRubyContext` を設定して `nys_start(&callbacks, &ctx)` を呼ぶ) |
| `pipe_streambuf.h` | 新規 | PipeWriteStreambuf / PipeReadStreambuf / PipeReadWStreambuf |
| `proj/nyamy-scripter-dll.vcxproj` | 新規 | DLL: C API + .mayu コンパイラ (DynamicLibrary) |
| `proj/nyamy-scripter.vcxproj` | 変更 | EXE: mruby 内蔵ビルド (DLL を ProjectReference) |

## 変更対象ファイル一覧

### 実装済み

| ファイル | 変更種別 | 主な内容 |
|---------|---------|---------|
| `scripter/nyamy_scripter.h` | 新規 | 公開 C API 宣言 (`nys_start` / `nys_reg_keyseq` 等) |
| `scripter/nyamy_scripter.cpp` | 新規 | `NYS_CTRL`/`NYS_CMD` 取得、CtrlStream ループ、設定キュー管理 |
| `scripter/nys_types.h` | 新規 | 内部型 (`NYsFuncArg` / `NYsFuncArgs` / `NYsStrs`) |
| `scripter/mruby_binding.cpp/h` | 新規 | mruby DSL (`NYamy::DSL` 等) |
| `scripter/mruby_main.cpp` | 新規 | EXE エントリポイント (`NYsCallbacks` + `MRubyContext` を設定して `nys_start(&callbacks, &ctx)` を呼ぶ) |
| `pipe_streambuf.h` | 新規 | PipeWriteStreambuf / PipeReadStreambuf / PipeReadWStreambuf |
| `scripter_manager.cpp` | 変更 | 非 stdio パイプ方式、msgPipe (stdout+stderr マージ)、`NYS_CTRL`/`NYS_CMD` 環境変数渡し |
| `scripter_manager.h` | 変更 | `m_hStderrRead` → `m_hMsgRead`、`stderrThread` → `msgThread` |
| `proj/nyamy-scripter-dll.vcxproj` | 新規 | C API + .mayu コンパイラ DLL ビルド |
| `proj/nyamy-scripter.vcxproj` | 変更 | mruby 内蔵 EXE ビルド (DLL を ProjectReference) |

### 未実装 (設計済み)

| ファイル | 変更種別 | 主な内容 |
|---------|---------|---------|
| `scripter_manager.cpp/h` | 変更 | reload = 再起動 (プロセス再起動方式) |
| `nyamy.ini` (3 箇所) | 変更 | [nyamy-scripter] セクション追加 |

## 関連ドキュメント

- [protocol.md](protocol.md) — CtrlStream / CmdStream バイナリプロトコル仕様
- [c-api.md](c-api.md) — DLL 公開 C API 仕様
- [typed-args.md](typed-args.md) — 型付き引数システム (NYsType / NYsFuncArgs / FFI 使用例)
- [nyamy-integration.md](nyamy-integration.md) — nyamy 側の変更 (ScripterManager / Engine)
- [build.md](build.md) — ビルドシステム変更
- [exe-design.md](exe-design.md) — EXE 設計 (薄いラッパー)

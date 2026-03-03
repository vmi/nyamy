# yamy-scripter 再設計 — 概要

## 背景と目的

yamy-scripter はかつて、.mayu ファイルをコンパイルして CmdStream を生成する
コンソール EXE (yamy-scripter32/64.exe) として実装されていた。

### 変更目的

凡例: ✅ 実装済み / 🔲 未実装 (設計済み)

1. **DLL 化** ✅: コンパイル機能を `yamy-scripter.dll` として切り出し、
   mruby / Ruby / Python など他言語から FFI 経由または組み込み形式で利用可能にする
2. **EXE 薄型化** ✅: EXE を DLL の API のみを使う薄いラッパーに変更する
   (将来 mruby 内蔵 EXE としても流用)
3. **stdout/stderr 隔離** ✅: CtrlStream/CmdStream を非 stdio のパイプ (`--ctrl=N`/`--cmd=N`) で渡し、
   scripter の stdin=NUL / stdout+stderr=ログパイプとする。
   scripter 実装が `printf` / `std::cout` を使っても CmdStream を汚染しない。
4. **コマンドキューイング** 🔲: Def 系コマンドは Commit まで内部でキューイングし、
   エラー無く Commit まで到達した場合のみ yamy へ一括送出する
5. **プロセス再起動方式** 🔲: 設定再読み込み時は scripter プロセスを再起動する
   (CtrlId::Reload を廃止、シンボルは argv で渡す)
6. **yamy.ini による起動コマンド設定** 🔲: フルコマンドライン + `${ENV_VAR}` 展開で
   任意の scripter 実装を指定可能にする
7. **ユーザー定義関数** 🔲: スクリプト言語側で関数を登録し、yamy からの CtrlStream
   `CallFunc` コマンドでランタイムに呼び出せるようにする
8. **ExecFunc** 🔲: ユーザー定義関数内から yamy 側組み込み関数 (&xxx) を呼び出せるようにする

## 現在の構成 (実装済み)

scripter は DLL 化済み。EXE は `scripter_engine(argc, argv)` を呼ぶだけの薄いラッパー。

```
scripter/
  yamy_scripter.h/cpp   ← DLL 実装 (scripter_engine(argc,argv) をエクスポート)
  lexer.cpp/h
  mayu_parser.cpp/h
  mayu_compiler.cpp/h
  cmd_stream_writer.cpp/h
  config_files.cpp/h
  ctrl_stream_reader.cpp/h  ← DLL に含まれる (Reload 読み取り)
  main.cpp                  ← EXE エントリポイント (scripter_engine(argc,argv) を呼ぶだけ)

pipe_streambuf.h              ← PipeWriteStreambuf/PipeReadStreambuf/PipeReadWStreambuf

proj/
  yamy-scripter-dll.vcxproj ← yamy-scripter.dll (DLL コア)
  yamy-scripter.vcxproj     ← yamy-scripter.exe (薄いラッパー)
```

プロセスフロー (現状):
```
yamy → CreateProcess("yamy-scripter.exe --ctrl=N --cmd=M"
                      stdin=NUL, stdout=stderr=msgPipe)

yamy → ctrl パイプ: Reload(syms) → scripter → compile → cmd パイプ: CmdStream → yamy
yamy → ctrl パイプ: Quit          → scripter → 終了

yamy ← msg パイプ: ログテキスト (scripter の stdout+stderr をマージ)
```

## 想定ユースケース (将来)

| ケース | 起動コマンド (yamy.ini) | 動作 |
|--------|------------------------|------|
| 標準 (.mayu) | `yamy-scripter.exe` | DLL が .mayu をコンパイル |
| mruby 内蔵 | `yamy-scripter-mruby.exe` | EXE が mruby ランタイムを組み込み、.rb を実行 |
| Python FFI | `python myscripter.py` | Python スクリプトが ctypes で DLL を呼ぶ |
| Ruby FFI | `ruby myscripter.rb` | Ruby スクリプトが ffi gem で DLL を呼ぶ |

## 成果物一覧

### 実装済み

| ファイル | 種別 | 説明 |
|---------|------|------|
| `scripter/yamy_scripter.h` | 変更 | DLL エクスポート宣言 (`scripter_engine(int,wchar_t**)`) |
| `scripter/yamy_scripter.cpp` | 新規 | DLL 実装 (--ctrl/--cmd 解析、CtrlStream ループ、.mayu コンパイル) |
| `pipe_streambuf.h` | 新規 | PipeWriteStreambuf / PipeReadStreambuf / PipeReadWStreambuf |
| `proj/yamy-scripter-dll.vcxproj` | 新規 | DLL ビルドプロジェクト (x64, TargetName: `yamy-scripter`) |
| `proj/yamy-scripter.vcxproj` | 変更 | EXE: main.cpp のみ + DLL 参照 |

### 未実装 (将来)

| ファイル | 種別 | 説明 |
|---------|------|------|
| `scripter/yamy_scripter.h` | 変更 | 詳細 C API 追加 (`yscr_start` / `yscr_reg_keyseq` 等) |
| `scripter/yamy_arg.h` | 新規 | 型付き引数定義 (DLL/yamy 内部型) |
| `yamy_func_signatures.h` | 新規 | yamy/DLL 共有の組み込み関数シグネチャ定義 |

## 変更対象ファイル一覧

### 実装済み

| ファイル | 変更種別 | 主な内容 |
|---------|---------|---------|
| `scripter/yamy_scripter.h` | 新規 | DLL エクスポートマクロ + `scripter_engine(int,wchar_t**)` 宣言 |
| `scripter/yamy_scripter.cpp` | 新規 | --ctrl/--cmd 解析、CtrlStream ループ、.mayu コンパイル |
| `scripter/main.cpp` | 変更 | `scripter_engine(argc, argv)` 呼び出しのみ |
| `pipe_streambuf.h` | 新規 | PipeWriteStreambuf / PipeReadStreambuf / PipeReadWStreambuf |
| `scripter_manager.cpp` | 変更 | 非 stdio パイプ方式、msgPipe (stdout+stderr マージ)、`--ctrl=N --cmd=M` 引数渡し |
| `scripter_manager.h` | 変更 | `m_hStderrRead` → `m_hMsgRead`、`stderrThread` → `msgThread` |
| `proj/yamy-scripter-dll.vcxproj` | 新規 | DLL プロジェクト、`pipe_streambuf.h` 追加 |
| `proj/yamy-scripter.vcxproj` | 変更 | main.cpp のみ + DLL 参照 |
| `proj/yamy.sln` | 変更 | DLL プロジェクト追加 |

### 未実装 (設計済み)

| ファイル | 変更種別 | 主な内容 |
|---------|---------|---------|
| `scripter/yamy_scripter.h` | 変更 | 詳細 C API 追加 (`yscr_start` 等) |
| `scripter/yamy_arg.h` | 新規 | 型付き引数定義 (DLL/yamy 内部型) |
| `yamy_func_signatures.h` | 新規 | シグネチャ定義 |
| `ctrl_stream.h` | 変更 | Reload 削除、CallFunc (0x01) 追加 |
| `ctrl_stream_writer.cpp/h` | 変更 | writeReload 削除、writeCallFunc 追加 |
| `scripter/ctrl_stream_reader.cpp/h` | 変更 | readReload 削除、readCallFunc 追加 |
| `cmd_stream.h` | 変更 | ExecFunc (0x30) 追加 |
| `cmd_stream_writer.cpp/h` | 変更 | writeExecFunc 追加 |
| `cmd_stream_reader.cpp/h` | 変更 | readExecFunc 追加 |
| `cmd_processor.h/cpp` | 変更 | onExecFunc コールバック追加 |
| `scripter_manager.cpp/h` | 変更 | reload = 再起動、callFunc 追加、ExecFunc 処理 |
| `mayu.cpp` | 変更 | WM_ScripterExecFunc ハンドラ追加 |
| `yamy.ini` (3 箇所) | 変更 | [yamy-scripter] セクション追加 |

## 関連ドキュメント

- [protocol.md](protocol.md) — CtrlStream / CmdStream バイナリプロトコル仕様
- [c-api.md](c-api.md) — DLL 公開 C API 仕様
- [typed-args.md](typed-args.md) — 型付き引数システム (YamyArg / シグネチャ DB)
- [yamy-integration.md](yamy-integration.md) — yamy 側の変更 (ScripterManager / Engine)
- [build.md](build.md) — ビルドシステム変更
- [exe-design.md](exe-design.md) — EXE 設計 (薄いラッパー)

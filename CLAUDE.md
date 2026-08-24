# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

NYamy は Windows 用キーボードリマッパー。本体は 64bit 専用、C++20。

## ビルド / テスト

MSBuild: `C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe`

```
MSBuild proj/nyamy.sln -p:Configuration=Release                                  # 本体一式 → Release/
MSBuild proj/nyamy-tests.vcxproj -p:Configuration=Debug -p:Platform=x64          # 単体テスト (tests/core) → Debug/nyamy-tests.exe
MSBuild proj/nyamy-scripter-tests.vcxproj -p:Configuration=Debug -p:Platform=x64 # 結合テスト (tests/scripter) → Debug/nyamy-scripter-tests.exe
```

- **個別 vcxproj ではなく sln 経由でビルドする** (プロジェクトごとに x64 / Win32 が混在)。OutDir は全プロジェクト共有なので `TargetName` の重複は implib の LNK1104 を招く。
- 前提となる生成物: mruby (`git submodule update --init` → `tools/build_mruby.ps1 Release Debug`)、`functions.h` (`tools/ps1exec.cmd tools/makefunc.ps1 engine.h functions.h`。gitignore 対象で、engine.h の関数定義を変えたら再生成)。
- `VERSION` は `proj/nyamy.props` の 1 行が唯一のソースで、全 TU にマクロ定義済み。**新規コードでこの識別子を使わない**。Release は C4703 (未初期化ポインタ) がエラー。
- `nyamy-scripter-tests` は名前に反して**本体のソースをほぼ全部コンパイルする**。本体に `.cpp` を足したら `nyamy.vcxproj` とこちらの両方へ。テストは `Debug/` へコピーされた設定を読むので、`.mayu` / `.mayu.rb` を編集したら要リビルド。
- `.vcxproj` / `.props` は直接編集してよい。

## アーキテクチャ

`nyamy.exe` (本体。低レベルフック + `SendInput`) / `nyamy-scripter.exe` (mruby 内蔵の設定コンパイラ) / `nyamyd32.exe` (32bit 注入ヘルパー) / 各アプリへ注入される `nyamy64.dll`・`nyamy32.dll` (フォーカス等をメールスロットで通知) の 4 種で動く。

```
.mayu / .mayu.rb → scripter (mruby + 独自 mayu パーサ → AST → compiler) → CmdStream → CmdProcessor → Setting → Engine
```

- nyamy ↔ scripter は 4 本のハンドル (ctrl=`NYS_CTRL` / cmd=`NYS_CMD` / msg=stdout+stderr をマージしたログ / stdin=NUL)。**stdin/stdout/stderr はバイナリプロトコルに使わない**。
- CmdStream は `Reset` … `Commit` が 1 つの設定定義ブロック。参照解決は Commit 時に遅延するので定義順は不問。**リロードは scripter プロセスの再起動**で、`load()` は非同期、完成通知は `WM_APP_scripterSettingReady`。
- ルート = 本体側、`scripter/` = コンパイラ側。設定の保存先はレジストリではなく `nyamy.ini` (UTF-8 BOM)。ログは固定長リングの `LogBuffer` が保持し、ログダイアログは表示器にすぎない。

## ドキュメント (`doc/`)

計画立案前に該当するものを確認すること。`doc/*.md` は開発者向けで配布物には含まれない (配布されるのは `doc/*.html`)。

- `event-flow.md`: フローとスレッド
- `input-injection.md`: 横取りと再注入・UIPI
- `dpi-aware.md`: **特に座標を扱う前に確認**
- `known-limitations.md`: **再提案の前に確認**
- `testing.md`: 実機テスト作成時、ログの誤読しやすい点や性能計測について確認
- `scripter-design/`: C API・mruby バインディング・プロトコル
- `doc/src/manual-ja/`: 配布マニュアルの原稿 (`tools/makedoc.ps1` で HTML 化)

## 規約

- `*.cpp` / `*.h`: コメントは**英語・ASCII のみ**、インデントは**タブ** (幅 4)、UTF-8 / CRLF
- `*.md` は**ハードラップしない** (1 段落 1 行)
- コミットメッセージは日本語で、**問題点と修正内容の概要だけ**を5-10行を目標に記述する。実装の詳細や実測表、検証手順などは書かない
- `README.md` の「主な変更点」は旧 Yamy との差異のみ

## 作業上の注意

- 設定ファイル (`.mayu.rb`)
    - 先頭には必ず `load "109.mayu.rb"` (英語配列なら `104.mayu.rb`) を書く。無いと `key` 定義が**すべて黙って捨てられる**
    - 数字キーの名前は `_0` … `_9`
- `nyamy-scripter.exe <script>` は ctrl へ `CtrlId::Start` を送って初めて設定読み込みが行われるので、何も送らないと壊れた設定でも何も出力せず正常終了する
- ログは誤読しやすい (`FocusChanged` はキーを押すまで出ない、等)。実機ログで判断する前に `doc/testing.md` 6 章を確認すること

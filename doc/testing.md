# テストと実機確認

自動テストの回し方と、**自動テストでは届かない部分をどう確かめるか**をまとめる。過去に空振りした確認方法も、同じ失敗を繰り返さないために残してある。

---

## 1. 自動テスト

3 本ある。**どれに書くかは「何に依存するか」で決める。**

| プロジェクト | 対象 | 依存 |
|---|---|---|
| `nyamy-tests` | 単体テスト。Windows にも本体にも依存しない部品 | 無し |
| `nyamy-engine-tests` | 単体テスト。Engine の設定反映まわり | 本体一式 (フックはスタブ) |
| `nyamy-scripter-tests` | 結合テスト。scripter → CmdStream → Setting のパイプライン | mruby + 本体一式 |

### 1.1 単体テスト (`nyamy-tests`)

```
MSBuild proj/nyamy-tests.vcxproj -p:Configuration=Debug -p:Platform=x64
Debug/nyamy-tests.exe
```

ライブラリ参照が無く数秒で終わる。**その軽さが線引きそのもの**で、ここに入れられるかどうかが「その部品の依存が切れているか」の判定になる。現在の対象は `LogBuffer` (テキストの保持) と `LogView` (編集欄に何を伝えるかの計算) で、どちらも窓を持たないので窓なしで検証できる。

テストを足すときは `tests/core/` に `test_*.cpp` を作り、`core_test.h` に `run*Tests()` を宣言して `core_test_main.cpp` から呼ぶ。フレームワークは `CORE_CHECK` だけ。**フィクスチャや前準備が要る時点で、それは向こう側のテスト**。

### 1.2 単体テスト (`nyamy-engine-tests`)

```
MSBuild proj/nyamy-engine-tests.vcxproj -p:Configuration=Debug -p:Platform=x64
Debug/nyamy-engine-tests.exe
```

対象は `Engine::applySetting()` と、その周辺 (`ensureKeymaps()` / `canRunAdHocKeySeq()` / `StickyNotice`)。ソース一覧は `nyamy-scripter-tests` から mruby と scripter を抜いたもので、`tests/scripter/hook_stub.cpp` を共有している。**OS フックは一切導入しない。**

テストを足すときは `tests/engine/` に `test_*.cpp` を作り、`engine_test.h` に `run*Tests()` を宣言して `engine_test_main.cpp` から呼ぶ。

**入口は `EngineTestAccess` (`engine_test.h`) で、`Engine` が `friend` に指定している。** `checkFocusWindow()` は `GetForegroundWindow()` から始まって 8 個の Win32 照会を辿るのでテストから制御できず、そこを避けて `applySetting()` などを直接叩くための窓口。`friend` はコード生成に影響しないので、出荷されるバイナリは変わらない。**振る舞いを差し替えるテスト専用マクロは使わない** (テストしたものと出荷するものが別になるため)。

ログの検証は `womsgstream` を attach せずに使い、`takeString()` で本文を取る。attach していなければ `PostMessage` は行われず、書かれたテキストがそのまま溜まる。詳細レベルの行を見たいときは `setThreshold(LogLevel::Debug)` を呼ぶ。

> `Engine` は `StrExprArg::setEngine()` でグローバルに自分を登録するので、**プロセス内に同時に 1 個まで**。テストごとにスコープを切って作り直すこと。

### 1.3 結合テスト (`nyamy-scripter-tests`)

```
MSBuild proj/nyamy-scripter-tests.vcxproj -p:Configuration=Debug -p:Platform=x64
Debug/nyamy-scripter-tests.exe
```

`.mayu` / `.mayu.rb` はビルド時に出力先へコピーされる。

> このプロジェクトは名前に反して**本体側のソースをほぼ全部コンパイルしている** (`engine.cpp` / `dlglog.cpp` / `function.cpp` など)。本体に新しい `.cpp` を足したら、**`nyamy.vcxproj` だけでなく、こちらと `nyamy-engine-tests.vcxproj` にも足すこと。** 呼ばれている関数がすべてヘッダ内 inline のうちはリンクが通ってしまうので、忘れても当面は気付けない。

> **設定ファイルを編集したら必ずリビルドしてからテストを実行すること。** テストは `Debug\` にコピーされた設定を読む (`NYAMY_ROOT` = exe のディレクトリ)。リポジトリ側だけ編集して `.exe` を直接実行すると古い設定で走り、「テストが差分を検出しない」という誤った結論になる。

`tests/scripter/test_main.cpp` は意図的に **ASCII のみ**。日本語をそのまま書くと MSVC が C4819 を出すので、キー名は `\u` エスケープか英字での説明にする。

### 等価比較の限界

設定の等価比較 (`dumpSetting` の一致) が証明するのは「2 つの設定ツリーが一致する」ことだけ。**両方が同じ分岐を飛ばしていれば一致してしまう**ので、分岐を実際に通す入力を用意しないと検証にならない。レジストリの Scancode Map を差し替える `NYAMY_SCANCODE_MAP` フック (`NYAMY_TEST_HOOKS` 付きビルドのみ) はこのために入れてある。

---

## 2. 実機確認が要るもの

自動テストが届かないのは次の 4 種類で、実機確認はここだけを狙えばよい。

1. **実行時にしか通らない経路** — ウィンドウ照合、`&Prefix`、`&ExecUserFunc` (キー押下 → scripter → ブロック実行)
2. **プロセス起動まわり** — `nyamy.ini` の `cmdLine`、`-I` / `-D`、`${VAR}` 展開
3. **人間が読むもの** — ログウィンドウに出るメッセージ
4. **リロード/再起動をまたぐ挙動**

### 手順書の型

確認の計画は、次の形の表で書くと過不足なく回せる。判定は**ログに特定の行が出るか**で行い、目視の印象では判断しない。

| ID | 確かめること | 操作 | 期待 |
|---|---|---|---|
| F2 | 関数引数の `Regexp` が実行時に届く | どこでも `C-A-S-R` | `RE abc123 src=/([a-z]+)[0-9]+/ MATCH $1="abc"` |

- 確認用の設定には `log.info` で目印を仕込み、**キーを押して該当行が出れば合格**とする
- 計器として `&DescribeBindings` (効いているキーマップと割り当て)、`&WindowIdentify` (対象ウィンドウのクラス名) を 1 キーずつ割り当てておく
- **設定を先にテストハーネスへ通し、意図したキーマップに入ることを確認しておく。** そうしておけば「反応しない」ときに、設定の書き方ではなく実行時側の問題だと切り分けられる

### 反復のしかた

症状によって「何を繰り返すか」が違う。連射が無意味なケースがある。

- **固着する症状** (登録が無い、状態が壊れたまま残る) は連射しても同じ結果にしかならない。反復すべきは**対象アプリと NYamy の起動サイクル**
- **競合で起きる症状** は再現率が 1 未満になる。再現しないことは仮説の反証にならない

---

## 3. 設定ファイルの落とし穴

- **設定フォルダの `.mayu.rb` は単独で読み込まれる。** 先頭で `load "109.mayu.rb"` (英語 104 キーボードなら `104.mayu.rb`) を書かないと `def key` が 1 つも無い状態になり、以降の `key` 定義は「キーが見つからない」として**すべて黙って捨てられる**。1 つも反応しないときはまずここを疑う
- **数字キーの名前は `_0` … `_9`**。`"C-A-S-0"` は `nys_assign_key: invalid modifier key` で弾かれる
- **`def subst` / `defsubst` の右辺の `*` は Setting に影響しない。** `to: "Esc"` と `to: "*Esc"` は同一の Setting にコンパイルされる。`.mayu` 側と表記を揃える意味はあるが機能差はない
- `class:` / `title:` を文字列で書く場合は前後のスラッシュを付けない。正規表現リテラル `/.../` も使える (マニュアルの[正規表現の制限](src/manual-ja/06-ruby-dsl.md)を参照)

---

## 4. 設定を scripter 単体でコンパイル検証する

`nyamy-scripter.exe <script>` は**引数だけでは何もコンパイルしない**。`NYS_CTRL` / `NYS_CMD` に継承可能なパイプハンドルを環境変数で渡し、ctrl 側へ `CtrlId::Start` (0x01) を送って初めてスクリプトが走る。

**これを送らないと、壊れた設定でも exit 0・stderr 空になる。**検証したつもりで何も見ていないことになるので注意 (一度これで空振りした)。

Start の書式は `ctrl_stream_writer.cpp` の `writeStart()` を参照 ([scripter-design/protocol.md](scripter-design/protocol.md))。PowerShell なら `AnonymousPipeServerStream` (Inheritable) 2 本で足りる。

> **ハーネスは必ず壊れた設定で先に試し、エラーを出すことを確かめてから使うこと。**

---

## 5. フック通知の再起動テスト

**素朴な再起動テストは偽陽性になる。** 通常終了は `uninstallMessageHook()` が `WM_NULL` をブロードキャストして DLL をアンロードさせるため、再注入された DLL が新しいメールスロットを開き、**修正が無くても成功してしまう**。

1. NYamy 起動 → メモ帳起動 → **一度クリックして**フォーカス通知が出ることを確認 (ログダイアログの**詳細をオン**。フォーカス通知は `Debug` レベル)
2. メモ帳を**サスペンドする** (`ntdll!NtSuspendProcess`)
3. **タスク マネージャーから NYamy を強制終了する** (`WM_NULL` ブロードキャストを走らせないため)
4. `tasklist /m nyamy64.dll` で notepad.exe が残っていることを確認
5. NYamy を再起動 → メモ帳をレジュームし、**メモ帳をクリック** → 通知が出るか

> **サスペンドは省略できない。** グローバルフックの DLL がアンロードされるのは「注入先が次にメッセージを取り出したとき」で、通常の GUI アプリはこれを常時行っている。「触らずに放置する」だけでは手で次の操作をするまでの数秒で先にアンロードされ、**条件が成立しないまま 4 を通過してしまう** (2026-08-17 実測)。サスペンドできない場合は、kill と再起動を 1 本のスクリプトで連続実行して隙間を潰す。
>
> 見るのは `FocusChanged` ではなく **`HWND:` / `THREADID:` / `CLASS:` / `TITLE:` の 4 行ブロック**。理由は 6 章。

直接証拠は Debug ビルド + DebugView (**Capture Global Win32** を有効化。ツールについては 7 章)。`HOOK_RPT` は Debug のみ有効で、`WriteFile to mailslot failed(...), reopening` → `open mailslot successed` の 2 行が出れば再オープンが発火している。**この 2 行が無いまま成功した場合は DLL が再注入されただけで、検証になっていない。**

---

## 6. ログの読み方 (誤解しやすいもの)

- フォーカス通知が出るのは**アクティブなウィンドウが実際に変わったとき**だけ。フックが `notifySetFocus()` を撃つのは `WM_ACTIVATEAPP` / `WM_NCACTIVATE` / `WM_ACTIVATE` / `WM_MOUSEACTIVATE` / `WM_SETFOCUS` とメニューループの出入り (`hook.cpp`) で、**カーソルを乗せただけでは 1 つも発生しない**。切り替えはクリックか Alt+Tab で行うこと。同じウィンドウ内をクリックし直しても、`GetFocus()` の変化を見ているので 2 回目以降は出ない
- `FocusChanged` を出す `checkFocusWindow()` は、**入力キューからイベントを 1 つ取り出したときにしか呼ばれない** (`engine.cpp`)。`mouse-event` を使っていない設定ではマウスはキューに入らないので、**ウィンドウを切り替えただけでは出ない。キーを 1 つ押して初めて出る** (Alt+Tab はキー入力を兼ねるので両方出る)。フォーカス通知そのものが届いたかを見たいなら、フックから来て `mayu.cpp` の `notifyHandler()` が出す `HWND:` / `THREADID:` / `CLASS:` / `TITLE:` の 4 行ブロックを見る (こちらはキー入力が要らない)
- `FocusChanged` は**変化したときだけ**出る。毎回出ないのは正常
- `FocusChanged` が**一度も**出ないときは、`m_focusOfThreads` の照会が外れてグローバルフォーカスへ落ちている。この経路は `GLOBAL FOCUS` を **1 行出したきり沈黙する** (`m_currentFocusOfThread != &m_globalFocus` が 2 回目以降は偽になるため) ので、黙っていること自体は手がかりにならない。**ログを `GLOBAL FOCUS` / `NO GLOBAL FOCUS` で検索して確かめる**。原因は 2 つあり、**ログ上は見分けが付かない**。(a) 登録側と照会側のスレッド ID の食い違い → [event-flow.md](event-flow.md) 2 章、(b) 強制終了後の再起動で古い DLL が名乗り直さない → [known-limitations.md](known-limitations.md) 2.1 節。**切り分けはクリーン起動で再現するかどうか**で行う
- 終了時のメッセージ (`ScripterManager: scripter did not exit; terminating` など) は**ログダイアログでは読めない**。ダイアログが終了処理で破棄されるため。`LOG_TO_FILE` はソースを書き換えないと有効にならないので逃げ道も無い。**scripter の終了コードで判定する**: `0` = 自力で正常終了 (メッセージは出ていない)、`1` = nyamy の `TerminateProcess(h, 1)` に殺された (メッセージが出た)、`2` = scripter が自分で自分を殺した (スクリプトが `kScripterQuitTimeoutMillisec` で返らなかった)。PowerShell なら終了前に `Get-Process` して `$s.Handle` に触れておけば、終了後も `$s.ExitCode` を読める
- **終了までの所要時間で判定してはいけない。** `Stopwatch` を回してからトレイの [終了] を選ぶ形だと、メニューを操作する時間が丸ごと乗る (2026-08-17 に 12.9 秒を計測。実際の終了は一瞬で、ほぼ全部が操作時間だった)
- 編集欄は**表示器であって保管場所ではない**。テキストは `LogBuffer` が持ち、編集欄は 100ms のタイマーで追いつく。したがって **1 行流した直後にダイアログを見ても、まだ出ていないことがある** (最大 100ms)。出ないと判断する前にもう一度見ること
- ログダイアログが**非表示の間、編集欄は更新されない**。行は溜まり続けるので内容は失われないが、「表示した瞬間にまとめて出る」のは正常
- 保管場所は起動時に一度だけ確保される `logMaxSize` 文字 (既定 100000) の固定長リング。溢れた分は**古い方から行単位で**捨てられ、先頭に切れかけの行は残らないので、**残る先頭行は設定値どおりに決まる**。上限を上げても増えるのは起動時に確保するメモリだけで、1 行あたりのコストは変わらない
- 編集欄を**スクロールして遡っている間は表示位置が保たれる**。末尾への追従が起きるのは、末尾が見えている状態で流したときだけ
- `NoFocusWindow` はアクティブ化遷移の一瞬 (`WM_ACTIVATEAPP` がフォーカス確定前に届く) で頻出するが正常。危険信号は「**実 hwnd の報告が続かない** `NoFocusWindow`」であって、出現頻度ではない
- ウィンドウ別キーマップは 1 つだけ選ばれるのではなく、**マッチしたものが連なりとして積まれる**。複数のパターンが同じクラス連鎖にマッチすれば、どちらのキーも生きる
- **設定が engine に反映されたかどうかは `setting activated:` の行で見る。** scripter 側の完了メッセージ (`[scripter]` が付く) や `loader: setting committed` はコンパイルの終わりを言っているだけで、engine が実際に切り替えたかどうかは言っていない。件数も出るので、**`keys` が極端に少なければキーボード定義の `load` を忘れた設定**だと分かる (3 章)
- **`FocusChanged` の `KEYMAPS:` が 0 のときは、そのスレッドに設定が反映されていない。** キーマップが 1 つも無いのでそのウィンドウではリマップが効かず、`internal error: m_currentKeymap == NULL` が出る。[known-limitations.md](known-limitations.md) 2.1 節 (古い DLL が名乗り直さない) とは**別物で、ログ上も区別が付く**: あちらは `GLOBAL FOCUS` へ落ちるだけでリマップ自体は効き、内部エラーは出ない
- **内部エラーは状態ごとに 1 回しか出ない。** `m_currentFocusOfThread == NULL` / `m_currentKeymap == NULL` は最初の 1 回だけ出て、復帰時に `recovered: ... (suppressed N more)` が出る。**同じ行が並んでいないことは「1 回しか起きていない」ことを意味しない**ので、`recovered:` の側の件数を見ること
- **`no setting yet; keys are not remapped` は異常ではない。** scripter が最初の設定を渡すまでの数秒はキーをそのまま通すのが正しい動作で、この行はその状態を 1 回だけ知らせるもの (Info)。**復帰行は出ない**。設定が入ったことは直後の `setting activated:` が言う。この行のあとに `setting activated:` が続かないときだけが異常
- 昇格ウィンドウがアクティブな間にマウスの `IN` 行が出ないのは正常 ([input-injection.md](input-injection.md))
- **`FN ` の行は 2 箇所が別々に書いている。** 前半 (`FN ` と関数名) は `engine.cpp` の `generateActionEvents()`、**末尾の括弧は関数自身** (`function.cpp`)。キーマップを辿る関数はここに**解決先のキーマップ名**を出すので、`FN  &KeymapParent(EmacsEdit)` は「親を辿った結果 `EmacsEdit` に行き着いた」の意味であって、`&KeymapParent` の引数ではない (この関数は引数を取らない)。`&Keymap(X)` と `&Prefix(X, ...)` は関数名の側も引数を出すため **`FN  &Keymap(X) (X)` と括弧が 2 回**並ぶ。`&Prefix` は改行を出さないので**次の出力が同じ行に続く**。いずれも異常ではない

行の書式とレベルはマニュアルの[ログの書式](src/manual-ja/05-customize.md)を参照。

---

## 7. ログ処理の性能を測る

ログの書き手 (とりわけ keyboardHandler スレッド) がどれだけ待たされているかを測る計装が [`log_profile.h`](../log_profile.h) にある。**平均ではなく最大値と分布を見る**ための道具で、平均は問題の起きているケースをちょうど隠す。

### 有効にする

`log_profile.h` の `//#  define NYAMY_LOG_PROFILE` のコメントを外してビルドする。どのプロジェクトファイルでも定義していないので、**戻し忘れてもコミットの diff に出る**。無効時は `Acquire` から計装が丸ごと消える。

### 測る

結果は `OutputDebugString` へ出るので、それを拾うツールが要る。**Sysinternals の DebugView** (`Dbgview.exe`、<https://learn.microsoft.com/sysinternals/downloads/debugview>) を起動しておくこと。Sysinternals Suite にも含まれている。

nyamy.exe 自身の出力なので、フック DLL を見る 5 章と違って *Capture Global Win32* は要らない。nyamy は非昇格で動くので、DebugView も管理者として実行する必要はない。

> **デバッガを接続していると DebugView には出ない。** `OutputDebugString` の出力先はデバッガが優先されるため。Visual Studio から実行して[出力]ウィンドウで読むこともできるが、待ち時間を測るのが目的なのでデバッガは付けない方がよい。

出力の契機は 2 つ。どちらも出力後にカウンタを 0 に戻すので、**シナリオの切り替えは「ログの消去」ボタンで行う**。

| 契機 | タグ |
|---|---|
| ログダイアログの [ログの消去] | `log cleared` |
| nyamy の終了 | `exit` |

観測する組み合わせは 4 通り。**それぞれの間で必ず [ログの消去] を押す**。負荷は**キーリピートの押しっぱなし** (適当なキーを 10 秒ほど押し続ける) で、詳細モードではこれでほぼ全部が同じブロックになる。

| # | | 打鍵する場所 |
|---|---|---|
| 1 | 通常モード × ログダイアログ非表示 | **調査ダイアログのスキャンコード欄** |
| 2 | 通常モード × ログダイアログ表示 | **調査ダイアログのスキャンコード欄** |
| 3 | 詳細モード × ログダイアログ非表示 | メモ帳など |
| 4 | 詳細モード × ログダイアログ表示 | メモ帳など |

> **通常モードでメモ帳を叩いても、ログは 1 行も出ない。** 打鍵ごとのログ出力は全て Debug レベルで、通常モードの閾値 (`kLogLevelNormal` = `Info`) では捨てられる。Info レベルの打鍵ログは `engine.cpp` の `outputInputToLog(..., LogLevel::Info)` 2 箇所だけで、**どちらも `if (m_isLogMode)` の中**にある。`m_isLogMode` が立つのは `dlginvestigate.cpp` の `wmFocusNotify()`、**調査ダイアログのスキャンコード欄にフォーカスがある間だけ**。2026-08-20 の計測はこれに気付かずメモ帳で行い、通常モードの 2 条件が空振りした。

**`n` が二桁で止まっていたら負荷が掛かっていない。** 詳細モードで 1000 件以上、通常モード (ログモード) で数百件は出るはず。

scripter からの出力も混ぜたいなら、`.mayu.rb` にエラーを含めて stderr を流す。

### 読み方

- **`wait`** = `Acquire` の構築で待たされた時間。**書き手が「ログを書く」と決めてから 1 文字も書けずにいる時間**であり、これが実害そのもの。見るのは `max`
- **`hold`** = `acquire()` から `release()` が返るまで。他の書き手を待たせている量で、整形コストの目安になる

読み手 (ログ画面への反映) がロックを持つのは文字列を受け取る一瞬だけで、編集欄の更新はロックの外で行う。したがって **`wait` は他の書き手の整形を待った分にほぼ等しくなり、`hold` の `max` と桁が揃う**。`wait` の `max` だけが突出していたら、**排他区間の中に長い処理が入り込んだ合図**なので、まずそれを疑う。

### 注意

- **`n` の桁が違うもの同士を比べない。** ヒストグラムの棒は全体に対する割合なので形は比較できるが、`max` は試行回数が多いほど大きい値を引きやすい。押しっぱなしの秒数を揃えること
- **これは条件同士だけでなく、同じ条件の変更前/変更後の間にも当てはまる。** `wouldLog()` のガードを増減させる変更は `n` そのものを変える。ガードが落とすのは安価な出力なので、**改善していてもヒストグラムは長い側に寄る**。分布の形ではなく、`n` × バケット上限で総保有時間を概算して比べること
- 計装自体が `Acquire` ごとに `QueryPerformanceCounter` を 3 回呼ぶ。**計装入りのビルドの絶対値を、計装なしのビルドの性能として読まないこと**。見るのは同じ計装入りビルド同士の前後差

---

## 8. 関連

- [event-flow.md](event-flow.md) — フローとスレッド構成
- [input-injection.md](input-injection.md) — 横取りと再注入
- [known-limitations.md](known-limitations.md) — 対処しないと決着した制約

# 15. history {#HISTORY}

200?/??/?? 3.31

:   - 「窓使いの憂鬱」以外のアプリケーションから「窓使いの憂鬱」の動作を on/off できるような仕組みを用意しました。使用する場合は、ソースの `mayuipc.h` を見てください。

    - WindowsXP の「ユーザーの切り替え」機能に今度こそ対応…したつもり。

    - security という章を追加。

2005/04/18 3.30

:   モディファイヤ

    :   - カナロックを表すモディファイヤ `KL-` を追加。オプション (KL-)をよく読むこと。

        - モディファイヤ `MAX-, MIN-, MMAX-, MMIN-` を追加。(Thanks to 小林)

        - Java アプリケーションなどで `IL-` が検出されないことがあるバグを修正。

        - キーリピート有 One Shot (`!!!`) のキーリピートが始まるまでの時間を オプション (delay-of !!!) で指定できるようにした。

    `⟨FUNCTION⟩`

    :   - `⟨FUNCTION⟩` `&LoadSetting` などで、設定ファイルをロードした時に、ログにロードしたことが表示されるようにした。

        - `⟨FUNCTION⟩` `&Prefix` の説明を正しく修正。

        - `⟨FUNCTION⟩` `&SetImeStatus` を追加。(Thanks to 小林)

        - `⟨FUNCTION⟩` `&SetImeString` を追加。(Thanks to 小林)

        - `⟨FUNCTION⟩` `&ShellExecute` が失敗した時のエラーメッセージが表示されないことがあるバグを修正。

        - `⟨FUNCTION⟩` `&VK` で入力できるマウスボタンに XBUTTON1 と XBUTTON2 を追加。他、さまざまな仮想キーを追加。

        - `⟨FUNCTION⟩` `&WindowMonitor` を追加。(Thanks to Hosaka Yuji)

        - `⟨FUNCTION⟩` `&WindowMonitorTo` を追加。(Thanks to Hosaka Yuji)

        - `⟨FUNCTION⟩` の引数置換機能を追加。(Thanks to 小林)

        - 既存 `⟨FUNCTION⟩` のマルチモニタ対応。(Thanks to Hosaka Yuji)

    マニュアル

    :   - acknowledgements 更新。

        - related work を更新。

        - references を更新。

    ビルド

    :   - Windows9x/Me/NT4.0 非対応になりました。

        - Borland C++ 5.5.1 でビルド不可能になりました。

        - Visual C++ Toolkit 2003 でビルドするための様々な注意事項を記載。(Thanks to 小林)

    ドライバ

    :   - 2000/XP 用ドライバの SMP/HT 対応。(Thanks to 小林)

        - 2000/XP 用ドライバの PS/2 専用版と USB 対応版のバイナリ統合。(Thanks to 小林)

        - 復旧用ドライバを追加。(Thanks to 小林)

    その他

    :   - WindowsXP の「ユーザーの切り替え」機能に対応。

        - ごくたまに、[タスクトレイのアイコンが表示されない問題](http://support.microsoft.com/kb/835874/JA/)を修正したつもり。また、表示されていないときに、もう一度起動すればアイコンがタスクレイに表示されるようにした。(「既に窓使いの憂鬱は動作中です」というダイアログボックスが出ると同時にアイコンが表示されます)

        - ログを消去した時刻をログ表示するようにした。

        - `109.mayu` に MultiMedia Keyboard 用の特殊キーなどを追加。

        - スナップショット版で「↑↓←→」という文字がキー名として使用できなくなっていたバグの修正。

2003/07/16 3.29

:   - Windows XP 用のドライバ。(Thanks to 小林)

    - `⟨FUNCTION⟩` `&PlugIn` を追加。

    - `⟨FUNCTION⟩` `&Recenter` を追加。

    - `contrib/109onAX.mayu` を追加。(Thanks to 松本博紀)

    - `104.mayu` と `contrib/ax.mayu` の記述ミスを修正。(Thanks to 松本博紀)

    - `contrib/DVORAKon109.mayu` を追加。(Thanks to 2ch)

    - その他修正…

2001/10/08 3.28

:   - チュートリアルとタスクトレイメニューの章を追加。(Thanks to hanawa)

    - related work に Key Bat を追加。

    - メニューをアクセスしている時のウィンドウクラス名は最後に `:MENU` がつく。

    - `=>` の代わりに `=` が使えるようにした。

    - subst の右側に `M0-` などのモディファイヤが指定できないバグを修正。

    - キーマップが影響する定義の説明を追加。

    - W2k で USB ドライバを簡単にインストールできるようにした。

    - その他修正。(Thanks to 小林)

2001/09/02 3.27

:   - 3.26 で `⟨FUNCTION⟩` `&Sync` が動作しなくなっていたバグを修正。`C-k` もちゃんと動作します。

    - `⟨FUNCTION⟩` `&KeymapPrevPrefix` を追加。

    - FAQ に「右 [[Windows]] キーを押すと IME のオンオフができるようにしたい。」を追加。

2001/08/26 3.26

:   - One Shot モディファイヤの例が間違っていたので修正。(Thanks to 小林)

    - Windows 側では離されていることになっているキーについて、「窓使いの憂鬱」がキーを離すスキャンコードを Windows 側へ送ることがないようにした。(Thanks to 小林)

    - related work に Word Emacs を追加。

    - `mayu-mode.el` で `M-;` (`indent-for-comment`) が使用できるようになった (Thanks to 永野圭一郎)

    - 戯れに `⟨FUNCTION⟩` `&DirectSSTP` を追加してみた。

2001/08/13 3.25

:   - `*.mayu` のアイコンが間違っていたのを直した。

    - `-=` で複数キーを設定しても一つしか有効にならないバグの修正。

    - WindowsNT4.0 以外 (9x, Me, 2000) で、「窓使いの憂鬱」を終了させたあとに何かのキーを押さないと終了しないバグの退治。(Thanks to 小林)

    - 設定ファイルで隣り合う複数の文字列を一つの文字列として取り扱うようにした。 key X = &HelpMessage("サンプル C-x-", \\  

      "C-x C-s\\t上書き保存\\r\\n" \\  

      "C-x C-f\\t開く\\t\\r\\n" \\  

      "C-x k\\t\\t新規作成\\r\\n" \\  

      "C-x C-c\\t終了")

    - `⟨FUNCTION⟩` `&DescribeBindings` を微妙に改良。

    - アンインストールができなくなっていたのを修正。(Thanks to 小林)

2001/08/05 3.24

:   - `default.mayu` で、[[英数]] を [[Control]] として使用している場合に `C-Q` が正しく働かない問題を修正。version 3.19 で修正したものと原因は同じ。(Thanks to hanawa)

    - Windows9x のドライバを更新。青画面でも操作できるようになりました。(Thanks to 小林)

    - One Shot モディファイヤであると宣言したキーが押されている間にキーが押された時のみ、モディファイヤとして扱われるようにした。今までは、離された時にもモディファイヤ扱いになっていた。

    - 一時停止にしたときにタスクトレイのアイコンを変化させるようにした。

    - `R-` を新設。(Thanks to hanawa)。`mayu-mode.el` で色がつくようにした。

    - [Borland C++ 5.5.1](http://www.borland.co.jp/cppbuilder/freecompiler/index.html) でビルドできるようにした。

    - One Shot モディファイヤを離したときに有効なモディファイヤは押したときと同じになるようにした。

    - `&EmacsEditKillLineFunc`, `&EmacsEditKillLinePred` の説明を追加。

    - USB 対応ドライバは今回は収録していません。

    - BETA ではないです。

2001/07/06 3.23 (BETA)

:   - `104on109.mayu` で「\`」が入力できなかったバグを直した。(Thanks to Oiwa)

    - One Shot (キーリピート有) を追加。

    - `⟨FUNCTION⟩` `&Toggle` で、強制的にオンオフできるようにした。

    - `109.mayu` に VAIO タワー用の特殊キーを追加。

    - WindowsNT4.0 でインストールできなくなっていた問題を修正。(Makefile の書き方のミス)

    - Windows9x のドライバを更新。

    - 新規インストールした時に「ホームディレクトリから」が選択されていてキー入力ができなくなってしまう問題を修正。

    - support に 2ch を追加。

2001/03/24 3.22 (BETA)

:   - 代用定義を追加した。おかげで `104on109.mayu` などがスマートに定義できるようになった。

    - `mayu-mode.el` で代用定義にも色がつくようにした。

    - 間違ってデバッグビルドしたものを収録していたのをリリースビルドを収録するようにした。

    - 間違って壊れた Windows9x のドライバを収録してしまったので正しいドライバを入れた。

2001/03/23 version 3.21 (BETA)

:   - Windows95 で CPU 時間を 100% 占めてしまう問題がなおった。(Thanks to 小林義明)

    - NEC PC-98x1 で動作しない問題がなおった。(Thanks to 小林義明)

    - `⟨FUNCTION⟩` `&LoadSetting` で、どの設定をロードするか指定できるようになった。

    - `contrib/98x1.mayu` を追加。(Thanks to HAJANO Naòqui)

    - ホームディレクトリ に、`mayu.exe` のカレントディレクトリを追加。

2001/03/10 version 3.20 (BETA)

:   - Windows95 でも動作するようになった。

    - acknowledgements 更新。

    - `cab32.dll` ではなく `iexpress` を使用してパッケージを作成するようにした。

    - ソースをパッケージから分離した。

2001/03/05 version 3.19 (BETA)

:   - このバージョンは内部構造を大幅に変えたので不安定である可能性が高いです。どうしてもこのバージョンが必要な人のみ使用してください。

    - [[Control]] + [[U]] 操作の改良。

    - `default.mayu` を使っている場合は、`Global` キーマップで [[英数]] を [[Control]] にしているが、`Global` キーマップを親キーマップとして持たない `keymap2` の中では (当然ながら) [[英数]] を [[Control]] として使用できないので、ちゃんと `Global` キーマップを親キーマップとして持つように設定ファイルを修正。

    - `default.mayu` で、`-DMAP-ESCAPE-TO-META` とすると [[Alt]] + [[X]] などを [[ESCAPE] [X]] で代用するようにした。

    - WindowsNT/2000 版のプログラム内部では全て UNICODE で処理するようにした。

    - WindowsNT/2000 版では、設定ファイルの文字コードに、shift\_jis だけでなく UTF-16LE/BE や UTF-8 を使用できるようになった。「メモ帳」で、「Unicode」や「Unicode big endien」や「UTF-8」を選んでセーブすれば、日本語だけでなくハングルなどの UNICODE で扱える文字が全て設定ファイルに書けるようになる。現在は文字コードは自動判別され、自動判別に失敗した場合の救済措置はない。自動判別に失敗するようなら自動判別できそうな文字をコメントにでも書いておくこと。

    - 正規表現ライブラリを [Regex++](http://ourworld.compuserve.com/homepages/John_Maddock/regexpp.htm) に変更した。

    - いろいろ変えた結果多分 bcc32 ではコンパイルできなくなった。少し修正するだけだと思うが。

    - `contrib/ax.mayu` を追加。(Thanks to KAWABE Nobukazu)

    - related work に MetaX を追加。

    - デフォルトモディファイヤの変更のサンプルが間違っていたので直した。(`key IC-* =` ではなく `key *IC- =` が正しい) (Thanks to Hirotaka Kasaki)

    - デフォルトキーの中で定義中のキーマップ名を参照できなかったバグを修正した。つまり、`keymap Hoge = &Prefix(Hoge)` などができるようになった。(Thanks to HANAWA Yoshio)

    - Windows9x 用のドライバを作成されたので、このバージョンから WindowsNT/2000 用のパッケージと Windows9x 用のパッケージが存在します。(Thanks to 小林義明)

    - 現在、Windows9x では、`&HelpMessage`、`&HelpVariable`、`&WindowSetAlpha` は使用できません。

    - `default.mayu` を使用している場合、シンボル `ZXCV` を定義しておくと、[[Control]] + [[Z]], [[Control]] + [[X]], [[Control]] + [[C]], [[Control]] + [[V]] が Windows と同じ動きをするようになります。(Thanks to HANAWA Yoshio)

2000/12/17 version 3.18

:   - ヘルプが開けないバグの修正。

    - `mayu-mode.el` が展開されていなかったのを直した。ついでに少しだけ修正。

    - `IC-` が時々無視されるバグが直ったはず。

    - `⟨FUNCTION⟩` `&HelpMessage` を追加。`default.mayu` を使っている場合はメモ帳で [[Control]] + [[X]] を押してみよう (ただし IE5.0 以上を使っている場合のみ)。

    - `event` を追加。

    - これらの追加により、プレフィックスキーが押された時にメッセージを表示することが可能になった。

    - `⟨FUNCTION⟩` `&KeymapWindow` を追加。

    - `⟨FUNCTION⟩` `&Variable` を追加。

    - `⟨FUNCTION⟩` `&Repeat` を追加。

    - `⟨FUNCTION⟩` `&HelpVariable` を追加。

    - これらの追加により Emacs の [[Control]] + [[U]] が可能になった。私にメールを送るために 1000000000000000000000000000000000000000000000000000000000000@tsg.ne.jp と入力するのもらくらくです。

    - チュートリアル・FAQ の充実は次回に延期。

2000/12/03 version 3.17

:   - メーリングリストを [Source Forge](http://lists.sourceforge.net/mailman/listinfo/mayu-support) へ移動。

    - `⟨FUNCTION⟩` `&WindowMoveTo` を追加。

    - `⟨FUNCTION⟩` `&WindowResizeTo` を追加。

    - `⟨FUNCTION⟩` `&MayuDialog` を追加。

    - `⟨FUNCTION⟩` `&DescribeBindings` を追加。表示形式がいまいち分かりにくいのでどうしようか思案中。

    - related work に Pastel Touch を追加。

    - related work に KeyMap を追加。

    - related work に dvorak kr, swap2k を追加。

    - related work に Q's Nicolatter を追加。

    - FAQ を追加。

    - `mailto:` をドキュメントから削除。

    - [DOC++](http://www.linuxsupportline.com/~doc++/) で表示できるようにソースのコメントを書き換えた。

    - Emacs で設定ファイルを編集する人のために `mayu-mode.el` を追加。ただし、色がつくだけです。

    - `⟨FUNCTION⟩` `&WindowIdentify` で、ウィンドウの位置と大きさも表示するようにした。

    - `contrib/keitai.mayu` を追加。テンキーで携帯電話式ひらがな入力をしようという画期的設定ファイル (注:Joke)。(Thanks to HANAWA Yoshio)

    - 設定ファイルが見つからなかったときはエラーを出すようになった。

    - 設定ファイルを更新。

    - 調査ダイアログを動かすとログウィンドウも動くようにした。

    - 設定ダイアログ

        - タスクトレイのメニューから沢山開けるバグを修正。

        - 「編集」を押して編集ダイアログが出ている最中にもう一度「編集」を押せるバグの修正。

        - 「編集」で、ファイル名を空にできないバグの修正。

        - サイズ変更できるようにした。

2000/05/28 version 3.16

:   - インストーラがテンポラリフォルダに作った contrib を削除しないバグを直した。

    - アンインストーラが「窓使いの憂鬱」関連のレジストリを全て消すように変更。

    - バージョンダイアログの「最新バージョンのダウンロード」ボタンが利かなくなっていたバグを直した。

2000/05/23 version 3.15

:   - ドライバを起動時にスタートさせることにした。ので、Administrator じゃなくても窓使いの憂鬱を起動できるようになったはず。ただしこれを有効にするには、窓使いの憂鬱を一旦アンインストールして、コンピュータをリセットしてから再びインストールすること。

    - アプリケーション終了時に時々エラーが起きていたバグを退治。

    - 「窓使いの憂鬱の設定」ダイアログの改良。

    - version 3.10 あたりから、ホームディレクトリのうち「設定(S)... で指定したファイルのあるディレクトリ」が検索されなくなっていたバグを修正。

    - 以下の `⟨FUNCTION⟩` の最後の引数に `MDI` を指定すると、MDI 子ウィンドウを操作できるようにした。 `&WindowClingToBottom`, `&WindowClingToLeft`, `&WindowClingToRight`, `&WindowClingToTop`, `&WindowClose`, `&WindowHMaximize`, `&WindowLower`, `&WindowMaximize`, `&WindowMinimize`, `&WindowMove`, `&WindowMoveVisibly`, `&WindowRaise`, `&WindowVMaximize`

    - 設定ファイルを更新。

2000/05/19 version 3.14

:   - 正規表現ライブラリを再び BUG FIX。

    - 設定ファイルを更新。

2000/05/13 version 3.13

:   - 正規表現ライブラリの BUG FIX。

2000/05/08 version 3.12

:   - 正規表現ライブラリの BUG FIX。「マインスイーパ」にマッチさせることができるようになった。

2000/05/06 version 3.11

:   - `contrib/dvorak.mayu` を更新。(Thanks to KANAI Makoto)

    - `contrib/mayu-settings.txt` を更新。(Thanks to HANAWA Yoshio)

2000/05/05 version 3.10

:   - [Borland C++ Compiler 5.5](http://www.inprise.com/bcppbuilder/freecompiler) でビルドできるようにした。(Thanks to HANAWA Yoshio)

    - 入力されたキーと同じモディファイヤの指定 ができるようにした。

    - 複数のキー設定をメニューで切り替え可能にした。

    - 設定ファイルを更新。

    - DLL 更新。

    - (非公開)

2000/04/24 version 3.09

:   - `contrib/dvorak.mayu` を追加。(Thanks to KANAI Makoto)

    - `⟨FUNCTION⟩` `&InvestigateCommand` を追加。`&PostMessage` の為の調査用。

    - `⟨FUNCTION⟩` `&EditNextModifier` を追加。[[Alt]] + [[X]] などを [[ESCAPE]] [[X]] などで代用することが可能に。

    - `⟨FUNCTION⟩` `&Prefix` を変更。`keymap2` を使うときのお約束 (`mod !shift !alt !control !windows`) が不要に。

    - 設定ファイルを更新。

2000/04/15 version 3.08

:   - `⟨FUNCTION⟩` `&WindowSetAlpha` を追加。

    - `⟨FUNCTION⟩` `&WindowRedraw` を追加。

    - related work に KillGates を追加。

    - `contrib/mayu-settings.txt` を更新。(Thanks to HANAWA Yoshio)

    - 設定ファイルを更新。

2000/04/01 version 3.07

:   - `⟨FUNCTION⟩` `&Wait` を追加。

    - `⟨FUNCTION⟩` `&MouseWheel` を追加。

    - `⟨FUNCTION⟩` `&VK` を拡張してマウスボタンを入力できるようにした。

    - マニュアルの内容をい く つか修正。

    - 設定ファイルを更新。

    - 英語リソースをつけてみた。が、使い方不明。

    - 連絡先の修正。

    - わたしはエイプリルフールは嫌いです。

2000/03/21 version 3.06

:   - いくつかの typo を修正。

    - `⟨FUNCTION⟩` `&WindowHMaximize`, `&WindowVMaximize` で、H と V を両方同じウィンドウへ適用した場合の動作を修正。

    - `⟨FUNCTION⟩` `&ClipboardCopy` を追加。

    - 設定ファイルを更新。

    - `IL-` を導入し、`I-` は `IC-` に名前を変更。ただし、互換性の為 `I-` も引き続き使用可能です。

    - コンソールのフォーカスの追跡に時々失敗していたのが直ったかもしれない。(かもしれないばっかだ…)

2000/03/10 version 3.05

:   - セットアップを修正。

    - コンソールのフォーカスの追跡に時々失敗していたのが直ったかもしれない。

    - (非公開)

2000/03/08 version 3.04

:   - 設定ファイルを修正。(デフォルトで英数キーを Control に)

    - マニュアルのバグを直した。

    - マニュアルを Mozilla でもちゃんと表示できるようにした。(IE で見るときはフォントサイズ「小」にするとかっこいいが、よみにくいのでしなくていいです)

2000/03/04 version 3.03

:   - `⟨FUNCTION⟩` `&WindowHMaximize` を追加。

    - `⟨FUNCTION⟩` `&WindowIdentify` を追加。

    - 設定ファイルを大幅に更新した。

    - `contrib/mayu-settings.txt` の追加。(Thanks to HANAWA Yoshio)

    - Windows2000 用のドライバをパワーマネジメント対応にした。

    - コンソールのフォーカスの追跡に時々失敗していたのが直ったかもしれない。

    - PlayStation2 の発売日。

1999/11/01 version 3.02

:   - setup 時にキーボードの種類を聞くようにした。

    - KeyboardLayout/ほげほげ の廃止 (日本語の場合はキーボードによらず、IME によって決定されるため)。

    - 設定ファイルを再読み込みした後に、コントロールキーなどが押されっぱなしになることがあるバグを直した。

    - One Shot モディファイヤの説明が間違っていたので修正。

    - デフォルトのモディファイヤを変更できるようにした。

    - include では、まず、レジストリで指定してある設定ファイルと同じフォルダを検索するようにした。

    - インストーラを少々変更

    - `⟨FUNCTION⟩` `&VK` で、`U-` と `D-` が逆になっていたバグを修正。

1999/10/30 version 3.01

:   - `104on109.mayu` と `109on104.mayu` を作った。

1999/10/29 version 3.00

1999/09/08 cmkey version 2.21

1999/09/05 cmkey version 2.20

1999/05/31 cmkey version 2.19

1999/05/31 cmkey version 2.18

1999/05/30 cmkey version 2.17

1999/05/29 cmkey version 2.16

1999/05/26 cmkey version 2.15

1999/05/26 cmkey version 2.14

1999/02/10 cmkey version 2.13

1998/12/02 cmkey version 2.12

1998/10/07 cmkey version 2.11

1998/07/23 cmkey version 2.10

1998/07/20 cmkey version 2.09

1998/07/17 cmkey version 2.08

1998/07/12 cmkey version 2.07

1998/07/11 cmkey version 2.06

1998/07/11 cmkey version 2.05

1998/07/05 cmkey version 2.04

1998/06/07 cmkey version 2.03

1998/05/24 cmkey version 2.02

1998/05/19 cmkey version 2.01

1998/05/19 cmkey version 2.00

1996/07/?? WinModMap version 1.03

1996/06/30 WinModMap version 1.02

1996/06/22 WinModMap version 1.01

1996/06/21 WinModMap version 1.00

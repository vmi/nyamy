## appendix {#APPENDIX}

### sample settings {#appendix_samples}

NYamy に付属するサンプル設定です。いずれも Ruby DSL (`.mayu.rb`) で書かれています。

- [`dot.mayu.rb`](../dot.mayu.rb) — トップレベルの設定。シンボルに応じて以下を読み込みます
- [`109.mayu.rb`](../109.mayu.rb) — 日本語 109 キーボード定義
- [`104.mayu.rb`](../104.mayu.rb) — 英語 104 キーボード定義
- [`104on109.mayu.rb`](../104on109.mayu.rb) — 109 キーボードを 104 風に
- [`109on104.mayu.rb`](../109on104.mayu.rb) — 104 キーボードを 109 風に
- [`default.mayu.rb`](../default.mayu.rb) — Emacs ライクなさまざまな設定
- [`emacsedit.mayu.rb`](../emacsedit.mayu.rb) — エディットコントロールの Emacs 風操作
- [`workaround.mayu.rb`](../workaround.mayu.rb) — リモートデスクトップ等で `E0-` 付きスキャンコードが届く環境向けの対処

### syntax {#appendix_syntax}

`.mayu` 形式の文法定義です。

- [`syntax.txt`](syntax.txt)

### 窓使いの憂鬱時代の資産 {#appendix_legacy}

「窓使いの憂鬱」用の `.mayu` 形式の設定ファイルは NYamy でも `.mayu.rb` から `load` することでそのまま利用できます。

インストール先の `legacy\` フォルダ (Dvorak 配列などは `legacy\contrib\`) に同梱されていますので、必要に応じて設定フォルダにコピーして利用してください。(`legacy\` フォルダは設定ファイルの検索対象外です)

なお、`.mayu.rb` 形式のサンプル設定に対応する `.mayu` 形式のファイル(`109.mayu.rb`に対応する`109.mayu`など)は、同一の内容になるよう修正を加えていますので、調整不要です。

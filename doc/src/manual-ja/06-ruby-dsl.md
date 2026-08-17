## 6. Ruby DSL (.mayu.rb) {#RUBYDSL}

NYamy は mruby を内蔵しており、設定を Ruby の DSL (Domain Specific Language) として記述できます。拡張子 `.mayu.rb` (または `.rb`) のファイルがこの形式です。付属のサンプル設定 ([`dot.mayu.rb`](../dot.mayu.rb) など) もすべてこの形式で書かれています。

Ruby DSL では、[カスタマイズ](#CUSTOMIZE)の章で説明した `.mayu` 形式の各機能に加えて、Ruby の制御構造 (条件分岐・ループ・メソッド定義など) やレジストリ [Scancode Map の照会](#dsl_scancodemap)、[ユーザー定義関数](#dsl_deffunc)が使用できます。

ファイルの内容はそのまま設定として実行されます。おまじない (クラス定義や `require` など) は不要です。

```mayu
# .mayu.rb の例: これで全体
load "109.mayu.rb"

keyseq "$WindowClose", "A-F4"

keymap "Global" do
  key["C-S-K", "C-A-K"] = "$WindowClose"
  key["C-S-L", "C-A-L"] = "&WindowLower"
end
```

キー名・モディファイヤ (`C-` など)・`&FUNCTION` の意味は `.mayu` 形式と共通です。以下、`.mayu` 形式の各構文に対応する DSL の書き方を説明します。

### ファイルのロード (`load` / `require`) {#dsl_load}

**`.mayu` 相当:** `include "filename.mayu"`

```mayu
load "109.mayu.rb"     # .rb を同一コンテキストで実行
load "some.mayu"       # .mayu 形式のファイルも読み込める (拡張子で自動判別)
require "mylib"        # load と同様だが、同じファイルは一度しか読み込まない
```

`.rb` の相対パスは `$LOAD_PATH` の順、すなわち

1. 実行中のスクリプトのあるフォルダ
2. [`-I` オプション](#cmdline)で追加したフォルダ (指定順)
3. [`NYAMY_LOAD_PATH`](#cmdline) で追加したフォルダ (指定順)
4. `NYAMY_CONFIG` (`%LOCALAPPDATA%\NYamy\Config`)
5. `NYAMY_HOME\Lib` (`%LOCALAPPDATA%\NYamy\Lib`)
6. `NYAMY_ROOT` (`nyamy.exe` のあるフォルダ)

で検索されます ([設定フォルダ](#HOME))。カレントディレクトリは検索されません。重複するフォルダは取り除かれます。`$LOAD_PATH` はスクリプトから追加・変更できます。

実際に使われた `$LOAD_PATH` は、設定を読み込むたびにログに出力されます (「[ログ(<u>L</u>)...](#menu-l)」で確認できます)。

`.mayu` の読み込み (`load "some.mayu"` や `.mayu` 内の `include`) は `$LOAD_PATH` ではなく設定ファイルの検索順 (`NYAMY_CONFIG`、`NYAMY_ROOT`) に従います。`NYAMY_HOME\Lib` は `.rb` ライブラリ専用です。

### キーシーケンス定義 (`keyseq`) {#dsl_keyseq}

**`.mayu` 相当:** `keyseq $Name = ...` ([キーシーケンス定義](#keyseq))

```mayu
# 形式 A: 名前付き。名前は "$" で始まる文字列
keyseq "$WindowClose", "A-F4"

# 形式 B: 名前なし。返り値の KeySeq オブジェクトを変数に保持して参照する
$WINDOW_CLOSE = keyseq "A-F4"

# 形式 C: 両方
$WINDOW_CLOSE = keyseq "$WindowClose", "A-F4"
```

名前の先頭の `$` は keyseq の名前であることを表す記号で、参照側 (`key[...] = "$WindowClose"` や `.mayu` ファイル内の `$WindowClose`) と表記が一致します。

### グローバル定義 (`def` 系) {#dsl_def}

[キーボード定義](#keyboard)の各構文に対応します。

```mayu
# def key Esc Escape = 0x01
defkey "Esc", "Escape", scan: "0x01"

# 複数スキャンコードを発生するキー (Pause など)
defkey "Pause", scan: ["E1-0x1d", "0x45"]

# def mod shift = LShift RShift
defmod "Shift", keys: ["LShift", "RShift"]

# def sync = 0x7e
defsync "0x7e"

# def alias ↑ = Up
defalias "↑", as: "Up"

# def subst *A = *B
defsubst "*A", to: "*B"

# def option KL- = enable / def option delay-of !!! = 20
defoption "KL-", value: true
defoption "delay-of !!!", value: 20
```

### キーマップ定義 (`keymap` / `keymap2` / `window`) {#dsl_keymap}

**`.mayu` 相当:** [キーマップ定義](#keymap)

キー割り当てはブロック (`do ... end`) の中に書きます。

```mayu
# keymap Global
keymap "Global" do
  # ...
end

# keymap EmacsMove : Global
keymap "EmacsMove", parent: "Global" do
  # ...
end

# keymap2 GlobalEscape : Global = &KeymapParent
keymap2 "GlobalEscape", parent: "Global", default: "&KeymapParent" do
  # ...
end

# window EditControl /:Edit$/ : EmacsEdit
window "EditControl", class: /:Edit$/, parent: "EmacsEdit" do
  # ...
end

# window Some ( /class/ && /title/ ) : Global
window "Some", class: /class/, title: /title/, parent: "Global" do
  # ...
end

# && の代わりに || にする場合は op: "||" を指定
window "Some", class: /class/, title: /title/, op: "||", parent: "Global"
```

`class:` / `title:` には正規表現リテラル (`/.../`) か文字列を指定します。`default:` には[デフォルトキー](#defaultKey)を指定します。

正規表現リテラルを渡した場合、キーマップに登録されるのはパターン文字列だけです。ウィンドウの照合は常に大文字小文字を区別しない ECMAScript 方言で行われるため、`i` / `m` を付けても効果がなく、指定するとログに警告が出ます (`x` は例外で、後述のとおりパターン自体を書き換えるので有効です)。詳しくは[正規表現の制限](#dsl_regexp)を参照してください。

#### ブロックの有無と適用範囲 {#dsl_keymap_scope}

キーマップ定義は「キーマップを宣言する」文であり、そのあとに書いたキー割り当ての行き先を決めるのはブロック (`do ... end`) です。

ブロックを**付けた**場合、そのキーマップはブロックの中だけに適用されます。ブロックを抜けると、直前のキーマップに戻ります。入れ子にもできます。

```mayu
keymap "Global" do
  key["C-A"] = "Home"          # Global
end

window "Opera", class: 'Opera\.exe:' do
  key["C-B"] = "End"           # Opera
end

key["C-C"] = "Delete"          # Global に戻っている
```

ブロックを**付けなかった**場合は宣言だけを行い、キー割り当ての行き先は変わりません。親 (`parent:`) やデフォルトキー (`default:`)、ウィンドウの条件 (`class:` / `title:`) だけを先に決めておき、中身は後から別のブロックで足す、という書き方ができます。

```mayu
# 宣言だけ。親は EmacsEdit
window "Opera", class: 'Opera\.exe:', parent: "EmacsEdit"

key["C-B"] = "End"             # Global (Opera には入らない)

# 中身は後から足す。parent: を書き直す必要はない
window "Opera" do
  key["C-C"] = "Delete"        # Opera
end
```

トップレベル (どのブロックの中でもない位置) に書いたキー割り当ては、`Global` キーマップに入ります。`load` したファイルの中で何を定義していても、`load` から戻ればトップレベルに戻ります。

**`.mayu` とは規則が違います。** `.mayu` にはブロックがなく、`keymap` 文は次のキーマップ定義が現れるまで適用され続けます。この継続は `.mayu` ファイルの中で完結するので、`.mayu` を `load` しても呼び出し元には影響しません。

### キー割り当て (`key`) {#dsl_key}

**`.mayu` 相当:** `key C-A = Home` ([キー割り当ての変更](#key))

キーマップブロックの中で `key[左辺] = 右辺` の形で書きます。

```mayu
keymap "Global" do
  key["C-A"] = "Home"

  # 左辺複数 (.mayu の key A B = X に相当)
  key["C-S-M", "C-A-M"] = "Applications"

  # 右辺はキー・&FUNCTION・$keyseq 参照を並べた文字列
  key["C-S-D"] = "&WindowIdentify &MayuDialog(Log, SHOW)"
  key["C-S-K"] = "$WindowClose"

  # 右辺に KeySeq オブジェクト (keyseq 形式 B) も指定できる
  key["C-F1"] = $WINDOW_CLOSE
end
```

### イベント割り当て (`event`) {#dsl_event}

**`.mayu` 相当:** [イベント定義](#event)

```mayu
keymap2 "GlobalEscape", parent: "Global", default: "&KeymapParent" do
  event["prefixed"]        = '&HelpMessage("Global", "ESC-")'
  event["before-key-down"] = "&HelpMessage"
end
```

### モディファイヤ割り当て (`mod`) {#dsl_mod}

**`.mayu` 相当:** [モディファイヤキー割り当ての変更](#mod)

```mayu
keymap "Global" do
  # mod control += 英数
  mod[:control] += "英数"
  mod[:control] += ["英数", "E0英数"]   # 複数まとめて

  # mod control -= CapsLock
  mod[:control] -= "CapsLock"

  # mod shift = LShift RShift (再設定)
  mod[:shift] = ["LShift", "RShift"]

  # 真のモディファイヤ (!) や One Shot (!!, !!!) はキー名の前に付ける
  mod[:shift]   += "!!Space"      # SandS
  mod[:control] += "!!!CapsLock"
end
```

### 条件シンボル (`define` / `symbol_defined?`) {#dsl_cond}

**`.mayu` 相当:** [条件分岐](#cond)

`.mayu` の `if ( SYM )` の代わりに、Ruby の条件分岐と `symbol_defined?` を使用します。

```mayu
define "KBD109"                # .mayu の define KBD109

if symbol_defined?("USE104")   # .mayu の if ( USE104 )
  load "104.mayu.rb"
else
  load "109.mayu.rb"
end

load "default.mayu.rb" if symbol_defined?("USEdefault")
```

シンボルは「[設定(<u>S</u>)...](#menu-s)」の `-Dシンボル名` で定義されたものと、[`nyamy-scripter` の `-D` オプション](#cmdline)で定義されたものと、設定ファイル内で `define` したものの集合です。これに加えて、NYamy が設定の読み込み開始時に自動で定義するシンボルがあります ([Scancode Map の照会](#dsl_scancodemap) を参照)。`SCM-` で始まる名前は NYamy の予約接頭辞なので、`define` で使わないでください。

定義されたシンボルは `symbol: 名前` の形で 1 個につき 1 行ログに出力されます (「[ログ(<u>L</u>)...](#menu-l)」で確認できます)。出力されるのは最初に定義されたときだけで、同じシンボルを二度 `define` してもエラーにはならず、二度目以降は何も起きません。

なお、`load "some.mayu"` で読み込んだ `.mayu` の中の `define` は、`symbol_defined?` からは**見えません**。`.mayu` の読み込みは Ruby スクリプトを実行し終えたあとにまとめて行われるためです。逆向き (Ruby の `define` を `.mayu` の `if` から参照する) は問題なく動きます。`.mayu` 側で条件分岐したいシンボルは、Ruby 側で `define` してください。

```mayu
define "USEdefault"        # .mayu の if ( USEdefault ) から参照できる
load "some.mayu"           # この中の define は symbol_defined? から見えない
```

`.mayu` どうしであれば、先に読み込んだファイルの `define` は後のファイルの `if` から参照できます。

### Scancode Map の照会 (`sc` / `ScancodeMap`) {#dsl_scancodemap}

レジストリの Scancode Map (`HKLM\SYSTEM\CurrentControlSet\Control\Keyboard Layout`) で既にキーが入れ替えられている環境で、NYamy 側で同じキーを二重に入れ替えないように条件分岐するための照会機能です。読み取りのみで、レジストリへの書き込みは行いません。

```mayu
sc("LShift")           # キー名 → スキャンコード整数 (0x2a)
sc("E1-0x0f")          # スキャンコード文字列 → 整数 (0xE10F)

ScancodeMap["CapsLock"]         # 変換後のスキャンコード整数。マッピングがなければ nil
ScancodeMap.to["LeftControl"]   # この変換先を持つ変換元の配列。なければ空配列
```

順引き・逆引きとも `[]` で書きます。`ScancodeMap.to` 単体は逆引き用のオブジェクトを返すだけなので、`if ScancodeMap.to` のように真偽判定に使わないでください (常に真になります)。

使用例: レジストリ側で [[CapsLock]] が入れ替え済みなら NYamy 側の入れ替えをスキップする。

```mayu
if ScancodeMap["CapsLock"].nil? && ScancodeMap.to["CapsLock"].empty?
  defsubst "*CapsLock", to: "*LControl"
end
```

変換元・変換先のどちらか一方にでも現れていれば「レジストリ側で処理済み」と見なすため、上の例のように 2 つの条件を `&&` で並べます。

#### 自動定義シンボル `SCM-REMAP-ESC` / `SCM-REMAP-LCTRL` {#dsl_scm_symbols}

よく使う 2 つのキーについては、NYamy が設定の読み込み開始時に判定してシンボルを定義します。`.mayu` からも `.mayu.rb` からも同じように参照できるので、両形式の設定ファイルを同じロジックで書けます。

| シンボル | 定義される条件 |
|---|---|
| `SCM-REMAP-ESC` | [[Esc]] (`0x01`) が Scancode Map の変換元または変換先に現れる |
| `SCM-REMAP-LCTRL` | [[LControl]] (`0x1D`) が Scancode Map の変換元または変換先に現れる |

```mayu
unless symbol_defined?("SCM-REMAP-ESC")
  key["*Esc"] = "*半角/全角"       # レジストリ側が Esc を触っていない時だけ
end
```

`.mayu` からは `if ( ! SCM-REMAP-ESC )` と書きます。

### 環境変数の参照 (`ENV`) {#dsl_env}

環境変数を `ENV` で読むことができます。マシンごとに違うフォルダを設定に埋め込みたいときなどに使います。

```mayu
ENV["HOME"]                          # 未定義なら nil
ENV.fetch("EDITOR", "notepad.exe")   # 既定値つき。既定値を省くと KeyError
ENV.key?("NYAMY_DEBUG")              # 定義されているか
ENV.keys                             # 名前の配列
ENV.to_h                             # 名前 → 値のハッシュ
ENV.each { |name, value| ... }
```

使用例:

```mayu
$LOAD_PATH.push "#{ENV['HOME']}\\nyamy-lib" if ENV.key?("HOME")

if ENV["COMPUTERNAME"] == "WORK-PC"
  load "work.mayu.rb"
end
```

**読み取り専用**です。`ENV[...] = ...` による書き込みはできません。設定を読み込むのは `nyamy-scripter` プロセスで、そこから他のプロセスを起動することはないため、書き込めても影響する先がないからです。`nyamy.ini` の [`cmdLine`](#cmdline) の展開は NYamy 本体側で行われるので、こちらにも影響しません。

`HOME` は Windows が定義していない場合でも、NYamy が `%USERPROFILE%` と同じ値を補って `nyamy-scripter` に渡します ([scripter の起動](#cmdline))。

### ログ出力 (`log`) {#dsl_log}

設定ファイルの中から[ログウインドウ](#menu-l)へメッセージを出力できます。設定が意図どおり読まれているかを確かめたいときや、[`deffunc`](#dsl_deffunc) のブロックの動きを追いたいときに使います。

```mayu
log.error "読み込みに失敗しました"
log.warn  "非推奨の書き方です"
log.info  "設定を読み込みました"
log.debug "keymap=#{name} parent=#{parent}"
```

出力は `[scripter]` の印を付けてログウインドウに出ます。レベルは行頭の `E` / `W` / `I` / `D` で区別できます ([ログの書式](#logformat))。

#### 出力される条件 {#dsl_log_level}

`debug` のメッセージは[ログ](#menu-l)ダイアログの「□詳細」がチェックされているときだけ出ます。ほかのレベルは常に出ます。

閾値は NYamy 本体側 (「□詳細」チェックボックス) とスクリプト側の 2 つが別々に保持され、**厳しいほうが効きます**。

```mayu
log.level          # => :info   実際に出力される閾値
log.level = :warn  # スクリプト側の閾値だけを変更する
```

`log.level` は**読み出しと書き込みが非対称**です。読み出すと「実際に何が出るか」(2 つのうち厳しいほう) が返り、書き込むとスクリプト側の閾値だけが変わります。そのため「□詳細」がチェックされていない状態では、`log.level = :debug` と書いた直後に `log.level` が `:info` を返します。指定できるのは `:error` / `:warn` / `:info` / `:debug` で、ほかの値を渡すと `ArgumentError` になります。

2 つを別々に持つのは、スクリプトが `debug` を出しっぱなしにしていても、「□詳細」の ON/OFF で出す/止めるを切り替えられるようにするためです。

メッセージの組み立てが重いときは、先に判定できます。`log.error?` / `log.warn?` / `log.info?` / `log.debug?` があります。

```mayu
log.debug "keymaps=#{dump_all_keymaps}" if log.debug?
```

#### 反映のタイミング {#dsl_log_timing}

設定ファイルが読まれるのは NYamy の起動時と[再読み込み(<u>R</u>)](#menu-r)のときだけです。そのため、**「□詳細」をチェックしただけでは、読み込み中に出るはずの `debug` メッセージは出てきません**。チェックしてから再読み込みしてください (この案内はログには出ません)。

[`deffunc`](#dsl_deffunc) のブロックの中で出すメッセージはキーを押すたびに評価されるので、チェックを変えた時点で切り替わります。

### ユーザー定義関数 (`deffunc`) {#dsl_deffunc}

Ruby のブロックを関数として登録し、キーに割り当てることができます。キー側からは [`&ExecUserFunc`](#function_ExecUserFunc) で呼び出します。

```mayu
deffunc "NotifyTime" do
  exec_keyseq "&HelpMessage(Time, #{Time.now.strftime('%H:%M')})"
end

deffunc "ShowMsg" do |msg|
  exec_keyseq "&HelpMessage(Msg, #{msg})"
end

keymap "Global" do
  key["C-F1"] = "&ExecUserFunc(NotifyTime)"
  key["C-F2"] = '&ExecUserFunc(ShowMsg, "hello")'
end
```

ブロック内では `exec_keyseq "アクション文字列"` で任意のキーシーケンスや `&FUNCTION` を実行できます (無限ループ防止のため、`&ExecUserFunc` を含む文字列は実行できません)。

ブロックが呼ばれるのは**キーを押したときだけ**です。離したときには呼ばれません。

`&ExecUserFunc` に渡した引数はブロックの引数として受け取れます。文字列は `String`、数値は `Integer`、正規表現は `Regexp` として渡されます。`$キーシーケンス名` は [`NYamy::KeySeq`](#dsl_keyseq)、モディファイヤ (`M-` など) は `NYamy::Modifier`、括弧でくくったキーの並びは `String` の配列として渡されます。

### 正規表現の制限 {#dsl_regexp}

正規表現リテラル (`/.../`) は `class:` / `title:` のほか、スクリプトの中で `=~` や `match` にも使えます。

```ruby
window "Explorer", class: /
    EXPLORER\.EXE :       # プロセス名
    SHELLDLL_DefView :    # 中間クラス
    .* SysListView32 $    # 末端
  /x, parent: "SysListView32"

host = "desktop-01.example.jp"
if host =~ /^([^.]+)\./
  define "HOST-#{$1.upcase}"
end
```

ただし NYamy の正規表現は Ruby (Onigmo) ではなく **C++ 標準ライブラリの ECMAScript 方言**で処理されます。NYamy 本体がウィンドウの照合に使っているものと同じエンジンなので、`.mayu.rb` で書いたパターンはそのままの意味で照合に使われますが、Ruby と同じではありません。

#### 使えない構文

名前付きキャプチャ `(?<name>...)`、インライン修飾子 `(?i)`、後読み `(?<=...)`、`\p{...}` などの Unicode プロパティは使えません。指定すると `RegexpError` になり、設定の読み込みが止まります。

**次の 6 つには特に注意してください。**`\A` `\z` `\Z` `\h` `\R` `\G` はエラーにならず、バックスラッシュを無視した**ただの文字**として扱われます (`\A` は「文字 `A`」、`\h` は「文字 `h`」)。気づかないまま意図と違うパターンで照合されるため、文字列の先頭・末尾には `^` / `$` を、16 進数字には `[0-9a-fA-F]` を使ってください。

`\d` `\w` `\s` `\b`、`[[:alpha:]]` などの POSIX 文字クラス、先読み `(?=...)` `(?!...)`、非貪欲 `*?` `+?`、後方参照 `\1` は使えます。

#### `^` と `$`

Ruby と異なり、既定では**文字列全体の先頭と末尾にのみ**一致します (行頭・行末ではありません)。行アンカーにしたい場合は `m` を付けてください。ウィンドウクラス名やタイトルは改行を含まないため、`class:` / `title:` では違いは現れません。

#### フラグ

| フラグ | 動作 |
|---|---|
| `i` | 大文字小文字を区別しません。`class:` / `title:` では**無視されます** (ウィンドウ照合は常に区別しません)。指定すると警告が出ます |
| `m` | **Ruby と意味が違います。**`.` が改行に一致するようになるのではなく、`^` / `$` が改行位置にも一致するようになります (ECMAScript の multiline)。指定するとログに情報が出ます。Ruby の `/m` 相当が欲しい場合は `.` の代わりに `[\s\S]` と書いてください |
| `x` | Ruby と同じく、パターン中の空白と `#` 以降のコメントが無視されます。これはパターン文字列そのものの書き換えなので、`class:` / `title:` でも有効です |
| `u` / `n` | 無視されます (常に Unicode として扱います) |

`Regexp#source` は書いたままのパターンを返します。`x` を付けた場合、実際に照合に使われるのは空白を除去した後のパターンで、こちらは `Regexp#pattern` で参照できます。

#### その他

- `String#sub` / `gsub` / `split` / `scan` / `index` に正規表現は渡せません (mruby の実装が文字列専用のためです)。`Regexp#match` / `=~` を使ってください
- `$~` / `$1`..`$9` は Ruby のようなフレームローカルではなく、**ただのグローバル変数**です。メソッドをまたいで前回の照合結果が残ります
- クラスの本名は `NYamy::Regexp` で、`Regexp` はその別名です。エラーメッセージには `NYamy::Regexp` と表示されます

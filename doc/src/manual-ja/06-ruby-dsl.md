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

相対パスは、実行中のスクリプトのあるフォルダ、続いて[ホームディレクトリ](#HOME)の順 (`$LOAD_PATH`) で検索されます。

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

`class:` / `title:` には Ruby の正規表現リテラル (`/.../`) か文字列を指定します。`default:` には[デフォルトキー](#defaultKey)を指定します。

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

シンボルは「[設定(<u>S</u>)...](#menu-s)」の `-Dシンボル名` で定義されたものと、設定ファイル内で `define` したものの集合です。

### Scancode Map の照会 (`sc` / `ScancodeMap`) {#dsl_scancodemap}

レジストリの Scancode Map (`HKLM\SYSTEM\CurrentControlSet\Control\Keyboard Layout`) で既にキーが入れ替えられている環境で、NYamy 側で同じキーを二重に入れ替えないように条件分岐するための照会機能です。読み取りのみで、レジストリへの書き込みは行いません。

```mayu
sc("LShift")           # キー名 → スキャンコード整数 (0x2a)
sc("E1-0x0f")          # スキャンコード文字列 → 整数 (0xE10F)

ScancodeMap["CapsLock"]      # 変換後のスキャンコード整数。マッピングがなければ nil
ScancodeMap.to("LeftControl") # この変換先を持つ変換元の配列。なければ空配列
```

使用例: レジストリ側で [[CapsLock]] が入れ替え済みなら NYamy 側の入れ替えをスキップする。

```mayu
if ScancodeMap["CapsLock"].nil? && ScancodeMap.to("CapsLock").empty?
  defsubst "*CapsLock", to: "*LControl"
end
```

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

`&ExecUserFunc` に渡した引数はブロックの引数として受け取れます。文字列は `String`、数値は `Integer`、正規表現は `Regexp` として渡されます。

# mruby バインディング仕様

> **状態: 実装済み**
> `scripter/mruby_binding.cpp/h` および `scripter/mruby_main.cpp` として実装済み。

## 目的

`nyamy-scripter.exe` 内蔵の mruby ランタイム向けに、
`.mayu` 構文と同等の設定を **Ruby らしく** 記述できる DSL を提供する。

- `NYsFuncArgs*` / `NYsStrs*` などの C 構造体管理を Ruby 側に完全隠蔽する
- ブロック・シンボル・配列・演算子などの Ruby イディオムを活かす
- `.mayu` の主要機能をすべて表現できる
- `.mayu` を `load` で読み込みつつ Ruby で追記する使い方も可能にする

---

## ファイル構成

```
scripter/
  mruby_binding.h/cpp   ← mruby C 拡張 (NYamy::DSL 等を登録; mruby_on_load_setting 実装)
  mruby_main.cpp        ← nyamy-scripter.exe エントリポイント
```

---

## 暗黙の実行コンテキスト

`.rb` ファイルはそのまま `load_setting` コールバックの本体として実行される。
`NYamy.start { ... }` の囲みは不要。

```ruby
# nyamy_config.rb — これが全体
load "104.mayu"          # キーボード定義を .mayu からロード
load "emacsedit.rb"      # 別の .rb を同一コンテキストで実行

keyseq "$WindowClose", "A-F4"
keyseq "$ToggleIME",   "A-BackQuote"

keymap "Global" do
  key["C-S-K C-A-K"] = "$WindowClose"
  key["C-S-L C-A-L"] = "&WindowLower"
end

deffunc "NotifyTime" do
  exec_keyseq "&OSD(#{Time.now.strftime('%H:%M')})"
end
```

内部では `NYamy::DSL` のインスタンスを生成し、ファイル内容を `instance_eval` で実行する。
再読み込み (scripter プロセス再起動) のたびにこのファイルが再実行される。

---

## `load` — ファイルのロード

**.mayu 相当:** `include "filename.mayu"`

```ruby
load "104.mayu"          # .mayu をコンパイルしてキューに積む (nys_include_mayu)
load "emacsedit.rb"      # .rb を同一 DSL コンテキストで instance_eval
```

拡張子で自動判別する。パス解決は `.mayu` と同じルール (設定ファイルと同ディレクトリ優先)。

Ruby の `Kernel#load` を DSL スコープ内で上書きする。
`Module#include` は上書きしないため、モジュールの include は通常通り使える。

---

## `load_mayu` — 現行 `.mayu` ファイルのロード

```ruby
load_mayu   # ConfigFiles が解決する .mayu をコンパイル (nys_load_mayu)
```

`load "104.mayu"` で個別ファイルを読むのではなく、
`nyamy.ini` が指す `.mayu` をそのまま使いたい場合に利用する。

---

## キーシーケンス定義

### `keyseq` — キーシーケンスの定義

**.mayu 相当:** `keyseq $name = actions`

```ruby
# 形式 A: 名前付き。名前は `$` で始まる String 必須
#          (参照側の "$WindowClose" と表記が一致する)
keyseq "$WindowClose", "A-F4"
keyseq "$ToggleIME",   "A-BackQuote"

# 形式 B: 名前なし、KeySeq オブジェクトを変数に保持
$WINDOW_CLOSE = keyseq "A-F4"   # グローバル変数に保持 (.mayu の $NAME に視覚的に対応)
WM_CLOSE      = keyseq "A-F4"   # 定数への代入も可

# 形式 C: 名前登録 + 変数保持の両立
$WINDOW_CLOSE = keyseq "$WindowClose", "A-F4"
```

先頭の `$` は「keyseq 名前空間を選ぶシジル」であり、登録される内部名には
含まれない (`keyseq "$WindowClose"` の内部名は `WindowClose`)。
`$` を欠く名前や String 以外 (Symbol 等) の名前はエラーになる。

形式 B / C の使い分け:

| | 名前参照 | 変数参照 |
|---|---|---|
| 名前テーブルへの登録 | ✅ (`nys_get_keyseq_idx` で引ける) | ❌ (形式 B) / ✅ (形式 C) |
| `key[...] = rhs` で使える | ✅ `"$Name"` として | ✅ `$VAR` として |
| `to:` kwarg での見た目 | `to: "$Name"` | `to: $WINDOW_CLOSE` |
| `.mayu` ファイルからの参照 | ✅ `$Name` として | ❌ |

形式 B で名前テーブル登録が不要な理由: `key["C-S-K"] = $WINDOW_CLOSE` は
`KeySeq#idx` を直接参照するため名前解決を介さない。
`.mayu` との相互運用が不要な純 Ruby 設定では形式 B で十分。

C API 対応: `nys_reg_keyseq(name, actions)`

- `"$Name"` → 先頭 `$` を除いた `"Name"` で登録
- 重複登録は `nys_get_keyseq_idx` で既存インデックスを返す (べき等)
- 戻り値は `NYamy::KeySeq` オブジェクト (内部に `keyseq_idx` を保持)

---

## グローバル定義 (`def` 系)

### `defkey` — キー定義

**.mayu 相当:** `def key Name1 Name2 = 0xNN` / `= E0-0xNN`

```ruby
defkey "Esc", "Escape",      scan: "0x01"
defkey "Enter", "Return",    scan: "0x1c"
defkey "LControl", "LCtrl",  scan: "0x1d"
defkey "RControl", "RCtrl",  scan: "E0-0x1d"
defkey "NumEnter", "NumReturn", scan: "E0-0x1c"

# 複数スキャンコード (Pause など)
defkey "Pause", scan: ["E1-0x1d", "0x45"]

# 名前を配列で渡す形式も可
defkey ["Esc", "Escape"], scan: "0x01"
```

C API 対応: `nys_def_key(names, scancodes)`

---

### `defmod` — モディファイア定義

**.mayu 相当:** `def mod Shift = LShift RShift`

```ruby
defmod "Shift",   keys: ["LShift", "RShift"]
defmod "Alt",     keys: ["LAlt", "RAlt"]
defmod "Control", keys: ["LControl", "RControl"]
defmod "Windows", keys: ["LWindows", "RWindows"]

# 単一キーの場合は文字列も可
defmod "Shift", keys: "LShift"
```

C API 対応: `nys_def_mod(modifier_name, key_names)`

---

### `defsync` — 同期キー定義

**.mayu 相当:** `def sync = 0x7e`

```ruby
defsync "0x7e"
defsync ["E1-0x1d", "0x45"]
```

C API 対応: `nys_def_sync(scan_codes)`

---

### `defalias` — エイリアス定義

**.mayu 相当:** `def alias ↑ = Up`

```ruby
defalias "↑",   as: "Up"
defalias "↓",   as: "Down"
defalias "←",   as: "Left"
defalias "→",   as: "Right"
defalias "Yen", as: "BackSlash"
```

C API 対応: `nys_def_alias(alias_name, key_name)`

---

### `defsubst` — サブスト定義

**.mayu 相当:** `def subst *-LButton = $someseq`

```ruby
# キーワード to: で RHS を指定
defsubst "*-LButton", to: $WINDOW_CLOSE          # KeySeq 変数
defsubst "*-LButton", to: "$SomeSeq"             # 名前参照 ($ シジル)
defsubst "*-LButton", to: "S-A B C"              # インラインアクション文字列

# LHS 複数指定
defsubst ["*-LButton", "*-RButton"], to: $WINDOW_CLOSE
```

C API 対応: `nys_def_subst(lhs_mod_keys, rhs_keyseq_idx)`

RHS (`to:` の値) の解決は `key[...] =` と同じ規則 (String / Symbol / KeySeq)。

---

### `defoption` — オプション定義

**.mayu 相当:** `def option KL- = true`

```ruby
defoption "KL-",            value: true
defoption "delay-of !!!",   value: 500
defoption "mouse-event",    value: true
defoption "drag-threshold", value: 10
```

C API 対応: `nys_def_option(option_name, value)`

`value:` の値は `to_s` で文字列化して渡す (`true` → `"true"`, `500` → `"500"`)。

---

## スキャンコード照会 (`sc` / `ScancodeMap`)

レジストリ Scancode Map (HKLM レベルのキー入れ替え) が既に設定されている環境で、
nyamy 側で同じキーを二重に入れ替えないよう条件分岐するための照会 API。

Scancode Map は kbdclass ドライバで適用されるため、nyamy には変換**後**のスキャンコードが
届く。「レジストリで済んでいれば nyamy 側マッピングをスキップ」という判定に使う。

### スキャンコード整数表現

`sc` / `ScancodeMap` はスキャンコードを整数で受け渡しする。
レジストリ Scancode Map の WORD 表現 (上位バイト = 0xE0/0xE1 プレフィクス、
下位バイト = コード) とそのまま一致する。

| 範囲 | 意味 |
|------|------|
| `0x00`–`0xFF` | プレーンスキャンコード |
| `0xE000`–`0xE0FF` | E0 プレフィクス (拡張キー) |
| `0xE100`–`0xE1FF` | E1 プレフィクス |

範囲外の整数は `ArgumentError`。

### `sc(キー名 or スキャンコード)` — 正規化

キー名またはスキャンコード表現をスキャンコード整数に解決する。

```ruby
sc(0x1c)        # => 0x1c        (整数はそのまま。範囲検査のみ)
sc(0xE10F)      # => 0xE10F
sc("0x1c")      # => 0x1c        (スキャンコード文字列)
sc("E1-0x0f")   # => 0xE10F
sc("28")        # => 0x1c        (十進文字列)
sc("LShift")    # => 0x2a        (defkey 定義済みキー名。第一引数の正規名に一致)
sc("lshift")    # => 0x2a        (大文字小文字は非区別)
```

- 文字列はまず `defkey` 定義済みキー名 (エイリアス含む) として解決し、
  見つからなければスキャンコードリテラルとしてパースする。
- 実在するキー名はスキャンコードリテラル (`0x`/`E0-`/`E1-`/十進) と衝突しない。
- キー名でもスキャンコード表現でも解釈不能な場合、範囲外整数の場合は `ArgumentError`。
- キー名解決はそのキーの `defkey` 実行後 (例: `load "109.mayu.rb"` 後) に有効。

C API 対応: `nys_sc_resolve(str)` (整数の範囲検査はバインディング層)

### `ScancodeMap` — レジストリマップの参照 (読み取り専用モジュール)

```ruby
ScancodeMap[変換前]        # => 変換後スキャンコード整数 (マッピングなしは nil)
ScancodeMap.to[変換後]     # => [変換元スキャンコード整数, ...] (なければ空配列)
```

- 引数は整数・キー名 String・スキャンコード文字列いずれも可 (`sc()` と同じ解決)。
- 順引き・逆引きとも `[]` で書く。`ScancodeMap.to` は逆引き用のネストモジュール
  `ScancodeMap::To` を返すだけなので、単体では真偽判定に使えない (常に真)。
- `.to[...]` は複数の変換元が同じ変換先を持ちうるため配列を返す。
- 変換先 `0x0000` (キー無効化) は整数 `0` を返す (`nil` = マッピングなしと区別できる)。
- レジストリ値なし / パース不能時は空マップ扱い。
- 実レジストリ Scancode Map は E0 のみ対応するため、結果に E1 (`0xE1nn`) は現れない。
- 旧仕様の `ScancodeMap.from(x)` は廃止 (`NoMethodError`)。
  `ScancodeMap.to(x)` も引数を取らなくなったため `ArgumentError` になる
  (`mrb_get_args(mrb, "")` で明示的に弾いている)。

使用例:

```ruby
# LAlt⇔RAlt: レジストリ Scancode Map で両キーが未使用のときのみ nyamy で入れ替え
if ScancodeMap["LeftAlt"].nil?  && ScancodeMap.to["LeftAlt"].empty? &&
   ScancodeMap["RightAlt"].nil? && ScancodeMap.to["RightAlt"].empty?
  defsubst "*LAlt", to: "*RAlt"
  defsubst "*RAlt", to: "*LAlt"
end
```

`ScancodeMap["LeftAlt"].nil?` だけでは「他キー→LeftAlt へのマップ (LeftAlt が
変換先として使われているケース)」を見落とすため、上記のように `.to[...].empty?` を
併用する。変換元・変換先のどちらか一方にでも現れていれば「レジストリ側で処理済み」と
見なす、というのが判定の意味。

C API 対応: `nys_scancode_map_length()` / `nys_scancode_map_entry(idx, from, to)`

- レジストリ読み取りは HKLM の `SYSTEM\CurrentControlSet\Control\Keyboard Layout`
  の `Scancode Map` 値 (`RegGetValueW`)。読み取りに管理者権限は不要。
- 初回照会時に読んでキャッシュし、設定ロード (`resetQueue`) でキャッシュ破棄する
  (本番はリロード毎にプロセス再起動のため実質毎回読み直し)。
- **テスト用フック**: `NYAMY_TEST_HOOKS` 付きでビルドした場合に限り、環境変数
  `NYAMY_SCANCODE_MAP` が設定されていればレジストリの代わりにその値を読む。
  値はレジストリ blob の 16 進表記 (16 進数字以外は区切りとして無視)。
  マッピング 0 個の blob を渡せば「Scancode Map なし」を再現できる。
  この定義は `nyamy-scripter-dll` の Debug 構成にのみ入るため、配布ビルドには
  この分岐自体が存在せず、環境変数でレジストリ読み取りを差し替えることはできない。

### 自動定義シンボル `SCM-REMAP-ESC` / `SCM-REMAP-LCTRL`

`.mayu` には Scancode Map を照会する構文が無いため、`.mayu` と `.mayu.rb` を同じ
ロジックで書けるよう、よく使う 2 キーの判定結果をシンボルとして供給する。

| シンボル | 定義条件 |
|---|---|
| `SCM-REMAP-ESC` | `0x01` (Esc) が Scancode Map の変換元または変換先に現れる |
| `SCM-REMAP-LCTRL` | `0x1D` (LControl) が変換元または変換先に現れる |

- `SCM-` は予約接頭辞。`0xE01D` は RightControl なので `SCM-REMAP-LCTRL` には含めない。
- 定義は `nyamy_scripter.cpp` の `defineScancodeMapSymbols()`。Start ジョブ処理の
  `resetQueue()` 直後、`on_load_setting` の呼び出し前に `g_symbols` へ挿入する。
  `.mayu` の `if` は `flushQueue()` 内の `MayuCompiler` が `g_symbols` を見て評価するので、
  それより前に確定していなければならない。
- **キー名では判定できない**: `nys_sc_resolve` が使う名前表は `nys_def_key`
  (= `104.mayu` / `109.mayu` のロード) で初めて埋まるため、この時点では空。
  よってスキャンコード直値で判定する。
- `resetQueue()` がマップのキャッシュを捨てた後に呼ぶので、設定ロードのたびに
  レジストリを読み直す。

### 対応範囲

本機能は mruby DSL (`.mayu.rb`) のみに実装する。レジストリ参照＋条件分岐という
手続き的機能であり、宣言的なレガシーテキスト `.mayu` 形式には実装しない。
純テキスト設定のユーザーは薄い `.mayu.rb` ラッパから `include_mayu "既存.mayu"` で
取り込みつつ本機能を利用できる。

### 実装メモ (キー名→スキャンコード表)

`defkey` は元来 nyamy へのコマンド送信のみで名前→スキャンコードを保持しないため、
`nys_def_key` 内でキュー push と同時に併走テーブル `g_keyNameToScan`
(`std::map<wstringi, uint16_t>`、大文字小文字非区別) へ第一スキャンコードの WORD を
登録する。既存キーは後定義が優先 (`Keyboard::addKey` の後勝ちルックアップと一致)。
このテーブルも `resetQueue` でクリアされる。

---

## 条件シンボル (`define` / `symbol_defined?`)

`.mayu` の `define SYM` と `if ( SYM )` に対応する DSL。
シンボル集合は Start コマンドで渡されたもの (USE104 等) に、`define` で追加したものを加えた集合。

**.mayu 相当:**
```
define KBD104
if ( KBD109 ) and ( ! KBD104on109 )
  ...
endif
```

```ruby
# シンボルを定義する (.mayu の `define SYM`)
define "KBD104"

# シンボルの有無を問い合わせる (.mayu の `if ( SYM )`)
if symbol_defined?("KBD109") && !symbol_defined?("KBD104on109")
  # ...
end

load "104.mayu.rb" if symbol_defined?("USE104")
```

C API 対応: `nys_define_symbol(name)` / `nys_has_symbol(name)`

- `define` は呼び出し時点でシンボル集合に追加するため、後続の `symbol_defined?` から見える
  (`.mayu` のファイル順序と同じ意味論)。
- `flushQueue` が flush 時点のシンボル集合をすべて `DefSymbol` として出力するため、
  `define` したシンボルも最終的な `Setting.m_symbols` に含まれる。
- コマンドラインの `-D` は `nys_add_default_symbol()` 経由で、Start が運ぶ集合に
  上乗せされる。Start は集合を丸ごと置き換えるので、Start のたびに再適用している。
- 初めて集合に入ったシンボルは `symbol: 名前` として info でログに出す
  (`logSymbolDefined()`)。二度目以降の `define` は無視され、ログにも出ない。

### `.mayu` の `define` との非対称

`load "x.mayu"` は**パスをキューに積むだけ**で、パースもコンパイルも
`flushQueue()` まで遅延する (keyseq のインデックス空間が Ruby の実行完了まで
確定しないため)。したがって `.mayu` 内の `define` は `symbol_defined?` からは
見えない。逆向き (Ruby の `define` → `.mayu` の `if`) は `g_symbols` 経由で見える。

`.mayu` どうしについては、`flushQueue()` が include 1 個をコンパイルするたびに
`MayuCompiler::symbols()` を `g_symbols` にマージするので、先に読み込んだファイルの
`define` は後のファイルの `if` から見える (`.mayu` の中で `include` を入れ子にした
場合と挙動が揃う)。

---

## キーマップ定義

### `keymap` / `keymap2` / `window`

**.mayu 相当:**
```
keymap  Name : Parent = &Default
keymap2 Name : Parent = &Default
window  Name /class_re/ : Parent = &Default
window  Name /class_re/ && /title_re/ : Parent = &Default
```

```ruby
# シンプルなキーマップ
keymap "Global" do
  key["C-S-L C-A-L"] = "&WindowLower"
end

# 親キーマップ付き
keymap "EmacsMove", parent: "Global" do
  key["C-A"] = "Home"
  key["C-E"] = "End"
end

# デフォルトキーシーケンス付き
keymap "KeymapDefault", default: "&Default" do
  key["C-N"] = "Down"
end

# keymap2
keymap2 "GlobalEscape", parent: "Global", default: "&KeymapParent" do
  event["prefixed"]        = '&HelpMessage("Global", "ESC-")'
  event["before-key-down"] = "&HelpMessage"
  key["M-C-G"]             = "&Ignore"
end

# window: クラス名のみ
window "EditControl",
       class: /:(Edit|TEdit|RichEdit(20[AW])?)$/,
       parent: "EmacsEdit" do
  key["M-N M-P"] = "A-Down"
end

# window: クラス + タイトル (AND 条件、デフォルト)
window "MayuLog",
       class: /mayu\.exe:#32770:Button/,
       title: /ログ - 窓使いの憂鬱/,
       parent: "Global" do
  key["C-G"] = $WINDOW_CLOSE
  key["Esc"] = $WINDOW_CLOSE
end

# window: OR 条件
window "SomeWin",
       class: /foo\.exe/,
       title: /bar/,
       op:    "||",
       parent: "Global"

# ブロックなし (キー割り当て不要)
window "EmacsEdit", class: /:Edit$/, parent: "EmacsMove"
```

C API 対応: `nys_begin_keymap(keymap_type, name, window_class, window_title, op, parent_name, default_keyseq_idx)`

| Ruby 引数 | C API 引数 |
|-----------|-----------|
| `class: /pattern/` | `window_class = pattern.source` |
| `class: "string"` | `window_class = string` |
| `title: /pattern/` | `window_title = pattern.source` |
| `op: "&&"` (デフォルト) / `"||"` | `op` |
| `parent: "Name"` | `parent_name` |
| `default: "&Default"` | `default_keyseq_idx` (自動登録) |
| `default: "$SeqName"` | `default_keyseq_idx` (名前参照) |

#### ブロックのスコープ

ブロックを渡した場合のみ、`dsl_begin_keymap()` は
`nys_push_keymap()` → `nys_begin_keymap()` → `instance_eval` → `nys_pop_keymap()`
の順に呼ぶ。ブロックを抜けると直前のキーマップに戻るので、ブロックの後に書いた
`key` は書いたとおりの位置に入る。入れ子も同様に動く。

ブロックなしの形式は push / pop を呼ばず、次の `nys_begin_keymap()` まで有効なまま
になる。`.mayu` と同じ挙動で、既存の設定ファイルはこれに依存している。

ブロック内で例外が起きると pop されないまま巻き戻るが、その場合は
`on_load_setting` が false を返して設定全体が破棄されるので、中途半端なスコープの
電文が適用されることはない。

---

### `key` — キー割り当て (キーマップブロック内)

**.mayu 相当:** `key C-S-M C-A-M = Applications`

`key` はキーマップコンテキストの `KeyMap` オブジェクトを返すメソッド。
`key[LHS...] = rhs` の形式で割り当てる。`[]` は可変長引数を受け付けるため
複数の LHS を二重括弧なしで記述できる。

```ruby
# LHS: 単一
key["*IC-C-Yen"]  = "$ToggleIME"   # * = down+up
key["D-Z"]        = "&VK(RButton)" # D- = down only
key["U-Z"]        = "&Ignore"      # U- = up only

# LHS: 複数 (varargs — 二重括弧不要)
key["C-S-M", "C-A-M"]         = "Applications"
key["C-S-Left", "C-A-Left"]   = "&WindowMove(-16, 0)"
key["C-S-K", "C-A-K"]         = $WINDOW_CLOSE

# LHS: スペース区切り文字列も引き続き使用可 (簡便記法)
key["C-S-M C-A-M"]            = "Applications"

# RHS: インラインアクション文字列 (匿名 keyseq として自動登録)
key["C-S-D"]  = "&WindowIdentify &MayuDialog(Log, SHOW)"
key["W-Left"] = "&MouseMove(-16, 0)"

# RHS: KeySeq オブジェクト
seq = keyseq "A B C"
key["C-F1"] = seq
```

`key["C-S-M", "C-A-M"] = rhs` の展開:

```
obj["C-S-M", "C-A-M"] = rhs
→ obj.[]=("C-S-M", "C-A-M", rhs)   # Ruby の []= 展開規則
→ args = ["C-S-M", "C-A-M", rhs]
→ value = args.pop  # rhs
→ lhs   = args      # ["C-S-M", "C-A-M"]
```

C API 対応: `nys_assign_key(lhs_mod_keys, rhs_keyseq_idx)`

**LHS の解決:**

| Ruby 記法 | 変換 |
|-----------|------|
| `key["C-S-M", "C-A-M"]` | `NYsStrs(["C-S-M","C-A-M"])` |
| `key["C-S-M C-A-M"]` | 空白分割 → `NYsStrs(["C-S-M","C-A-M"])` |
| `key["C-S-L"]` | `NYsStrs(["C-S-L"])` |

**RHS の解決 (key / event / defsubst `to:` / `default:` 共通):**

| Ruby 値 | 例 | 変換 |
|---------|-----|------|
| `String` | `"$WindowClose"` / `"A-F4"` | アクション文字列としてパースし `nys_reg_keyseq(nil, actions)` で匿名登録。`$Name` は名前付き keyseq への参照、裸のトークンはキー名 |
| `Symbol` | `:"$WindowClose"` / `:Escape` | 同等の String と完全に同一視 (`:X` ≡ `"X"`) |
| `KeySeq` | `$WINDOW_CLOSE` | `.idx` を直接使用 (名前テーブル不要) |

#### `KeyMap` の概念実装

```ruby
class KeyMap
  def initialize(ctx)
    @ctx = ctx
  end

  # obj[lhs1, lhs2, ...] = rhs → []=(lhs1, lhs2, ..., rhs)
  def []=(*args)
    rhs     = args.pop
    lhs_arr = args.length == 1 ? args[0].split : args
    @ctx.__assign_key(lhs_arr, @ctx.__resolve_rhs(rhs))
  end
end
```

---

### `event` — イベント割り当て (キーマップブロック内)

**.mayu 相当:** `event before-key-down = &HelpMessage`

`event` も同様に `EventMap` オブジェクトを返すメソッドとして設計する。

```ruby
event["prefixed"]        = '&HelpMessage("Global", "ESC-")'
event["before-key-down"] = "&HelpMessage"
event["after-key-up"]    = "$MyHandler"
```

C API 対応: `nys_assign_event(event_name, rhs_keyseq_idx)`

RHS の解決は `key` と同じ。

#### `EventMap` の概念実装

```ruby
class EventMap
  def initialize(ctx)
    @ctx = ctx
  end

  def []=(event_name, rhs)
    @ctx.__assign_event(event_name.to_s, @ctx.__resolve_rhs(rhs))
  end
end
```

---

### `mod` — モッド割り当て (キーマップブロック内)

**.mayu 相当:**
```
mod control += 英数
mod shift   -= LShift
mod alt      = LAlt RAlt
```

`mod` はキーマップコンテキストの `ModMap` オブジェクトを返すメソッドとして設計する。
`mod[:control] += "英数"` は Ruby の `obj[key] += value` 展開規則により
`mod[:control] = mod[:control] + "英数"` に変換され、
`ModMap#[]` / `ModMap#[]=` を通じて適切な C API 呼び出しになる。

```ruby
# mod control += 英数  (最も頻出)
mod[:control] += "英数"
mod[:control] += ["英数", "E0英数"]

# アサインモード付きキー (!! など)
mod[:alt]     += ["!!無変換", "!!E0無変換"]
mod[:control] += "!!!CapsLock"

# mod control -= CapsLock
mod[:control] -= "CapsLock"

# mod control = LControl RControl (reset)
mod[:control] = ["LControl", "RControl"]
mod[:shift]   = "LShift"

# prefix 付き (mod !Shift shift += LShift など、使用頻度は低い)
mod.prefix("!Shift")[:shift]             += "LShift"
mod.prefix(["!Shift", "!!!Ctrl"])[:shift] += ["LShift", "RShift"]
```

`mod[:control] += "英数"` の展開ステップ:

```
1. mod[:control]          → ModValue.placeholder  (op="=", keys=[])
2. .+("英数")             → ModValue.new(op="+=", keys=["英数"])
3. mod[:control] = <↑>   → nys_assign_mod(nil, "control", "+=", NYsStrs(["英数"]))
```

#### `ModMap` / `ModValue` の概念実装

```ruby
class ModMap
  def initialize(ctx, prefixes = nil)
    @ctx, @prefixes = ctx, prefixes
  end

  def [](name)
    ModValue.placeholder   # +/- のためのプレースホルダー
  end

  def []=(name, value)
    v = value.is_a?(ModValue) ? value : ModValue.new("=", Array(value))
    @ctx.__assign_mod(@prefixes, name.to_s, v.op, v.keys)
  end

  def prefix(prefixes)
    ModMap.new(@ctx, Array(prefixes))
  end
end

class ModValue
  attr_reader :op, :keys

  def self.placeholder = new("=", [])

  def initialize(op, keys)
    @op, @keys = op, Array(keys)
  end

  def +(other_keys) = ModValue.new("+=", Array(other_keys))
  def -(other_keys) = ModValue.new("-=", Array(other_keys))
end
```

C API 対応: `nys_assign_mod(prefixes, modifier_name, op, keys)`

| Ruby | C API |
|------|-------|
| `mod[:control] += "英数"` | `nys_assign_mod(NULL, "control", "+=", ["英数"])` |
| `mod[:control] -= "CapsLock"` | `nys_assign_mod(NULL, "control", "-=", ["CapsLock"])` |
| `mod[:control] = ["LC", "RC"]` | `nys_assign_mod(NULL, "control", "=", ["LC","RC"])` |
| `mod.prefix("!Shift")[:shift] += "LShift"` | `nys_assign_mod(["!Shift"], "shift", "+=", ["LShift"])` |

モディファイア名は `Symbol` / `String` どちらでも受け付け、`to_s` して渡す。

---

## ユーザー定義関数

### `deffunc` — 関数登録

**.mayu 相当:** `&ExecUserFunc`

Engine 側の `.mayu` で `&ExecUserFunc(FuncName, arg1, arg2, ...)` が実行されると
登録したブロックが呼ばれる。Engine から渡された引数は型変換表に従い Ruby 値として
ブロックに展開される。

```ruby
# 引数なし
deffunc "NotifyTime" do
  require "time"
  exec_keyseq "&OSD(#{Time.now.strftime('%H:%M')})"
end

# Engine から渡される引数を受け取る (例: &ExecUserFunc(ShowMsg, "hello"))
deffunc "ShowMsg" do |msg|
  exec_keyseq "&OSD(#{msg})"
end

keymap "Global" do
  key["C-F1"] = "&ExecUserFunc(NotifyTime)"
  key["C-F2"] = "&ExecUserFunc(ShowMsg, \"hello\")"
end
```

`deffunc` は内部で:

1. `nys_reg_user_func(func_name, handler)` でエンジンに登録
2. `func_name → block` テーブル (mruby Hash) に保存

`on_exec_user_func` コールバック受信時:

1. `func_name` でハッシュを検索
2. `NYsFuncArgs*` → Ruby 値の配列に変換 (型変換表参照)
3. `mrb_yield_argv` でブロックを呼び出す

### キーシーケンス文字列内でのユーザー定義関数呼び出し

アクション文字列中でユーザー定義関数を呼び出す方法は 2 通り提供する。

#### 方法 A: `&ExecUserFunc` (C API の生の形式)

```ruby
key["C-F1"] = "&ExecUserFunc(NotifyTime)"
```

#### 方法 B: `@FuncName` プレフィックス (未実装)

バインディング層が `nys_reg_keyseq` に渡す前に `@Name` を `&ExecUserFunc(Name)` へ置換する
糖衣構文。現状は未実装のため、方法 A (`&ExecUserFunc(Name)`) を使うこと。


### `exec_keyseq` — キーシーケンス実行 (deffunc ブロック内)

DSL オブジェクトのメソッドとして提供される (`nys_exec_keyseq` の薄いラッパー)。
`on_exec_user_func` コールバック実行中のみ有効で、受信時のトリガーコンテキストが自動的に引き継がれる。

```ruby
deffunc "DoAction" do
  exec_keyseq "&OSD.Display(\"hello\")"
end
```

制約:
- `on_load_setting` 内では `nys_exec_keyseq` が false を返す
- `&ExecUserFunc` を含む actions は C API レベルでガードされ false を返す (無限ループ防止)

---

## 型変換

### `NYsFuncArgs` → Ruby 値 (on_exec_user_func 受信時)

`NYsFuncArgs*` の各要素を Ruby 値に変換してブロックに渡す:

| `NYsType`             | Ruby 型              | 変換方法 |
|----------------------|----------------------|----------|
| `NYsType_String`      | `String`             | UTF-8 文字列 |
| `NYsType_Number`      | `Integer`            | `int32_t` |
| `NYsType_Regexp`      | `Regexp`             | `"pattern".to_regexp` (mruby 拡張) |
| `NYsType_KeySeqIdx`   | `NYamy::KeySeq`       | インデックスをラップ |
| `NYsType_ModifierSpec`| `NYamy::Modifier`     | `modifiers` + `dontcares` の 2 値 |
| `NYsType_TokenSeq`    | `Array` of `String`  | `NYsStrs*` → 文字列配列 |

---

## `ENV` — 環境変数 (読み取り専用)

`NYamy::Env` のシングルトンをトップレベル定数 `ENV` として置く
(`ARGV` と同じく `mrb_define_global_const`)。`mruby-env` gem は使わない
(gem を足すと mruby のビルド構成に影響するため)。

```ruby
ENV["HOME"]                          # 未定義なら nil
ENV.fetch("EDITOR", "notepad.exe")   # 既定値なしで未定義なら KeyError
ENV.key?("X") / ENV.include?("X") / ENV.has_key?("X")
ENV.keys / ENV.to_h / ENV.each { |name, value| ... }
```

- 実装は `GetEnvironmentVariableW` / `GetEnvironmentStringsW`。UTF-8 ⇔ UTF-16 変換は
  既存の `utf8ToWide` / `wideToUtf8`。
- 未定義と「定義済みで空文字列」は長さがどちらも 0 なので、`GetLastError()` の
  `ERROR_ENVVAR_NOT_FOUND` で区別する。
- 列挙時、名前が空のエントリ (Windows がブロックに置く `=C:=C:\...` 形式のドライブ別
  カレントディレクトリ) はスキップする。
- **書き込み (`ENV[]=`) は用意しない**。scripter は設定を読むだけで他プロセスを
  起動しないため書いても届かず、`nyamy.ini` の `cmdLine` の展開は NYamy 本体側で
  既に終わっているので、そちらにも影響しない。

`HOME` は `ScripterManager` が子プロセスの環境ブロックに載せる (未定義なら
`%USERPROFILE%`)。NYamy 自身の環境には入れないので、`&ShellExecute` の起動先には
伝播しない。

---

## `log` — ログ出力

`NYamy::Log` を `mrb_define_class_under` で定義し、`NYamy::DSL#log` (`dsl_log`) が
そのシングルトンを返す。インスタンスは DSL オブジェクトの `@__log__` に遅延生成して
保持するので、呼ぶたびに作られることはない。メソッド名は Ruby の Logger の慣例に
合わせてある (`trace` ではなく `debug`)。

```ruby
log.error "…"
log.warn  "…"
log.info  "…"
log.debug "…"

log.error? / log.warn? / log.info? / log.debug?   # 出力されるか (重い生成の回避用)

log.level          # => :info   実効閾値
log.level = :warn  # スクリプト側の閾値だけを更新する
```

- 出力の実体は `nysWouldLog()` → `nysLogUtf8()`。閾値の扱いは
  [c-api.md](c-api.md) の「閾値を 2 つ持つ理由」を参照。
- **`log.level` の getter と setter は非対称**。getter は実効閾値 (nyamy 側と
  スクリプト側の厳しいほう) を返し、setter はスクリプト側だけを更新する。
  `log.level = :debug` の直後に `log.level` が `:info` を返すことがある。
  読みたいのはたいてい「実際に何が出るか」なので getter は実効閾値でよい、と判断した。
  この非対称性はユーザー向けマニュアルにも明記してある。
- レベルは `:error` / `:warn` / `:info` / `:debug` のシンボルまたは同名の文字列を
  受ける (`logLevelFromRuby`)。ほかの値は `ArgumentError`。getter が返すのは常に
  シンボル (`logLevelToRuby`)。
- `log` はトップレベル (DSL の `instance_eval` 下) でも `deffunc` のブロックの中でも
  同じように使える。前者はロード時、後者はキー押下のたびに評価される。

---

## 実装済み C API

mruby バインディング実装時に追加された C API:

- `nys_include_mayu(path)` — 指定パスの .mayu をコンパイルしてキューに積む
- `nys_last_error()` — 最後のエラーメッセージを返す (UTF-8 NUL 終端、なければ NULL)
- `nys_exec_keyseq(actions)` — キーシーケンスを実行 (`on_exec_user_func` 内でのみ有効)
- `nys_sc_resolve(str)` — キー名 / スキャンコード文字列をスキャンコード WORD に解決 (`sc` の実体)
- `nys_scancode_map_length()` / `nys_scancode_map_entry(idx, from, to)` — レジストリ Scancode Map の列挙 (`ScancodeMap` の実体)
- `nys_push_keymap()` / `nys_pop_keymap()` — キーマップのスコープを囲む (ブロック付き `keymap` / `window` の実体)
- `nys_add_default_symbol(name)` — Start が運ぶシンボル集合への上乗せ (`-D` の実体。`nys_start()` の前に呼ぶ)

コールバック typedef:

```c
// exeCtx: nys_start() に渡した呼び出し元コンテキストポインタ (MRubyContext* など)
typedef bool (*nys_on_load_setting)(void* exeCtx);
typedef void (*nys_on_exec_user_func)(void*             /* exeCtx */,
                                     const char*       /* func_name */,
                                     const NYsFuncArgs* /* args */);

typedef struct NYsCallbacks {
    bool (*on_load_setting)(void* exeCtx);
    void (*on_quit)(void* exeCtx);   // Quit 直前に呼ばれる (NULL 可)
} NYsCallbacks;
```

`nys_start` は `NYsCallbacks` テーブルと `exeCtx` を受け取る:

```c
NYS_API int nys_start(const NYsCallbacks* callbacks, void* exeCtx);
```

`on_exec_user_func` は `nys_reg_user_func` で関数ごとに個別登録する:

```c
NYS_API bool nys_reg_user_func(const char* func_name, nys_on_exec_user_func on_exec_user_func);
```

`nys_exec_keyseq` での `&ExecUserFunc` ガード (実装済み):
- actions 文字列に `&ExecUserFunc` が含まれる場合は即 false を返す (無限ループ防止)

---

## 完全な設定例

`default.mayu` + `104.mayu` の Ruby 版 (抜粋):

```ruby
# nyamy_config.rb

# キーボード定義を .mayu からロード
load "104.mayu"

# キーシーケンス
# 形式 A: 名前登録 (他の .rb / .mayu ファイルから参照する場合に有用)
keyseq "$ToggleIME",  "A-BackQuote"

# 形式 B: 変数保持 (コロンお見合い回避、to: $VAR で使いやすい)
$WINDOW_CLOSE       = keyseq "A-F4"
$WM_VSCROLL_PAGEUP  = keyseq "&PostMessage(ToItself, 277, 2, 0)"
$WM_VSCROLL_PAGEDOWN = keyseq "&PostMessage(ToItself, 277, 3, 0)"

# Global キーマップ
keymap "Global" do
  key["*IC-C-Yen"]                  = "$ToggleIME"
  key["C-S-M",   "C-A-M"]          = "Applications"
  key["C-S-L",   "C-A-L"]          = "&WindowLower"
  key["C-S-R",   "C-A-R"]          = "&WindowRaise"
  key["C-S-Z",   "C-A-Z"]          = "&WindowMaximize"
  key["C-S-I",   "C-A-I"]          = "&WindowMinimize"
  key["C-S-Left",  "C-A-Left"]     = "&WindowMove(-16, 0)"
  key["C-S-Right", "C-A-Right"]    = "&WindowMove(16, 0)"
  key["C-S-Up",    "C-A-Up"]       = "&WindowMove(0, -16)"
  key["C-S-Down",  "C-A-Down"]     = "&WindowMove(0, 16)"
  key["C-A-A"]                     = "&WindowClingToLeft"
  key["C-A-E"]                     = "&WindowClingToRight"
  key["W-Left"]                    = "&MouseMove(-16, 0)"
  key["W-Right"]                   = "&MouseMove(16, 0)"
  key["W-Up"]                      = "&MouseMove(0, -16)"
  key["W-Down"]                    = "&MouseMove(0, 16)"
  key["C-S-K",   "C-A-K"]          = $WINDOW_CLOSE
  key["C-S-S"]                     = "&LoadSetting &HelpMessage(Mayu, \"再読込完了\")"

  # CapsLock を Control に (104 キーボード)
  mod[:control] += ["CapsLock", "E0CapsLock"]
  key["*CapsLock"]   = "*LControl"
  key["*E0CapsLock"] = "*LControl"
end

# GlobalEscape keymap2
keymap2 "GlobalEscape", parent: "Global", default: "&KeymapParent" do
  event["prefixed"]        = '&HelpMessage("Global", "ESC-")'
  event["before-key-down"] = "&HelpMessage"
  key["M-C-G"]             = "&Ignore"
end

# EmacsMove (emacsedit.rb からも load 可能)
keymap "EmacsMove", parent: "Global" do
  key["Home"]   = "C-Home"
  key["End"]    = "C-End"
  key["C-A"]    = "Home"
  key["C-B"]    = "Left"
  key["M-B"]    = "C-Left"
  key["C-E"]    = "End"
  key["C-F"]    = "Right"
  key["C-G"]    = "Escape"
  key["C-N"]    = "Down"
  key["C-P"]    = "Up"
  key["M-V"]    = "Prior"
  key["S-Home"] = "S-C-Home"
  key["S-End"]  = "S-C-End"
end

# ウィンドウ特化キーマップ
window "EditControl",
       class: /:(Edit|TEdit|RichEdit(20[AW])?)$/,
       parent: "EmacsEdit"

window "SysListView32", class: /:SysListView32$/, parent: "EmacsMove"
window "SysTreeView32", class: /:SysTreeView32$/, parent: "EmacsMove"

window "DialogBox", class: /:#32770:/, parent: "Global" do
  key["C-G"] = "Escape"
end

window "MayuLog",
       class:  /mayu\.exe:#32770:Button/,
       title:  /ログ - 窓使いの憂鬱/,
       parent: "Global" do
  key["C-G"] = $WINDOW_CLOSE
  key["Esc"] = $WINDOW_CLOSE
end

window "ConsoleWindowClass", class: /^ConsoleWindowClass$/, parent: "Global" do
  key["C-S-K C-A-K"] = "A-Space C"
  key["S-Prior"]      = $WM_VSCROLL_PAGEUP
  key["S-Next"]       = $WM_VSCROLL_PAGEDOWN
end

# ユーザー定義関数
deffunc "NotifyCurrentTime" do
  require "time"
  exec_keyseq "&OSD(#{Time.now.strftime('%H:%M:%S')})"
end

keymap "Global" do
  key["C-S-F12"] = "&ExecUserFunc(NotifyCurrentTime)"
end
```

---

## 内部実装メモ (mruby C 拡張)

### コンテキスト構造体

```cpp
// mruby_binding.h に定義
struct MRubyContext {
    int                argc;
    const char* const* argv;
    mrb_state*         mrb;  // initially nullptr; set by mruby_on_load_setting
};
```

### グローバル状態

```cpp
// mruby_binding.cpp 内

static mrb_value g_funcTable;  // mruby Hash: String => Proc (GC保護済み)
```

`mrb_state*` は `MRubyContext` が保持する。スクリプトパスは `mruby_on_load_setting` 内で
`ctx->argv[1]` を `nys_resolve_config_path()` に渡して絶対パスへ解決する
(相対パスは `NYAMY_CONFIG` → `NYAMY_ROOT` の順に探索。カレントディレクトリは見ない)。
`argv[1]` が無いのは内部エラーで、`main()` が使用法を表示して終了コード 2 で先に弾く。

### コールバック

```cpp
// mruby_binding.h / mruby_binding.cpp

// NYsCallbacks.on_load_setting に渡す。mrb_open してDSLクラス登録→スクリプト実行
bool mruby_on_load_setting(void* exeCtx);  // exeCtx = MRubyContext*

// NYsCallbacks.on_quit に渡す。mrb_close して mrb を nullptr に
void mruby_on_quit(void* exeCtx);          // exeCtx = MRubyContext*

// nys_reg_user_func で関数ごとに登録する。g_funcTable からブロックを検索し呼び出す
void mruby_on_exec_user_func(void* exeCtx, const char* func_name, const NYsFuncArgs* args);
```

### `mruby_main.cpp` の構成

```cpp
#include "nyamy_scripter.h"
#include "mruby_binding.h"
#include <windows.h>

int main(int argc, char *argv[])  // UTF-8 activeCodePage マニフェスト使用
{
    MRubyContext ctx = { argc, (const char* const*)argv, nullptr };

    NYsCallbacks callbacks = {};
    callbacks.on_load_setting = mruby_on_load_setting;
    callbacks.on_quit         = mruby_on_quit;

    return nys_start(&callbacks, &ctx);
}
```

---

## 設計上の決定事項

- **`mod` のモディファイア名正規化**: `:control` / `"control"` など大文字小文字の表記ゆれを
  内部で正規化する。`defmod` で登録した名前に合わせる方針 (登録時に `capitalize` して
  正規形を保持し、`mod[key]` 呼び出し時も同じ正規化を適用して照合する)。

- **`load` の第2引数**: Ruby の `Kernel#load(path, wrap=false)` を上書きするが、
  `wrap` 引数は無視する。元の `Kernel#load` が必要なら `Kernel.load(path)` で呼べる。

- **`$VAR` のスコープ**: keyseq はもともとプロセス終了まで保持されるグローバルなリソース。
  Ruby グローバル変数 `$VAR` で保持することと整合しており、スコープ問題は生じない。

- **keyseq 名の `$` 必須化**: `keyseq` の第一引数 (名前) は `"$Name"` 形式の String
  のみ受理する。`$` は keyseq 名前空間を選ぶシジルで、参照側 (`"$Name"` /
  `.mayu` の `$Name`) と定義側の表記が一致する。内部名は `$` を除いた裸の名前で
  登録するため、パーサ・コンパイラ・旧 .mayu ローダーは影響を受けない。
  Symbol による名前指定は廃止。

- **RHS の Symbol は String と同一視**: `key[...] = :X` は `key[...] = "X"` と完全に
  同じ意味 (アクション文字列としてパース)。かつての「Symbol は keyseq レジストリ
  専用検索」という特殊動作は、`:Escape` (keyseq 参照) と `"Escape"` (キー押下) が
  別物になり混乱を招くため廃止した。keyseq 参照は表記によらず `$` シジルで行う。

- **`deffunc` とキーマップ定義の順序**: 順序制約なし。
  `nys_reg_user_func` の呼び出しタイミングはキューイングの順序に影響しない。
  未登録関数への `&ExecUserFunc` 呼び出しは Engine 側でエラー扱いになる。

---

## 関連ドキュメント

- [c-api.md](c-api.md) — DLL 公開 C API 仕様
- [typed-args.md](typed-args.md) — 型付き引数システム
- [exe-design.md](exe-design.md) — EXE 設計パターン
- [overview.md](overview.md) — 全体構成と変更目的

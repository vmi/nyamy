# mruby バインディング仕様

> **状態: 実装済み**
> `scripter/mruby_binding.cpp/h` および `scripter/mruby_main.cpp` として実装済み。

## 目的

`yamy-scripter.exe` 内蔵の mruby ランタイム向けに、
`.mayu` 構文と同等の設定を **Ruby らしく** 記述できる DSL を提供する。

- `YsFuncArgs*` / `YsStrs*` などの C 構造体管理を Ruby 側に完全隠蔽する
- ブロック・シンボル・配列・演算子などの Ruby イディオムを活かす
- `.mayu` の主要機能をすべて表現できる
- `.mayu` を `load` で読み込みつつ Ruby で追記する使い方も可能にする

---

## ファイル構成

```
scripter/
  mruby_binding.h/cpp   ← mruby C 拡張 (Yamy::DSL 等を登録; mruby_on_load_setting 実装)
  mruby_main.cpp        ← yamy-scripter.exe エントリポイント
```

---

## 暗黙の実行コンテキスト

`.rb` ファイルはそのまま `load_setting` コールバックの本体として実行される。
`Yamy.start { ... }` の囲みは不要。

```ruby
# yamy_config.rb — これが全体
load "104.mayu"          # キーボード定義を .mayu からロード
load "emacsedit.rb"      # 別の .rb を同一コンテキストで実行

keyseq :window_close, "A-F4"
keyseq :toggle_ime,   "A-BackQuote"

keymap "Global" do
  key["C-S-K C-A-K"] = :window_close
  key["C-S-L C-A-L"] = "&WindowLower"
end

deffunc "NotifyTime" do
  exec_keyseq "&OSD(#{Time.now.strftime('%H:%M')})"
end
```

内部では `Yamy::DSL` のインスタンスを生成し、ファイル内容を `instance_eval` で実行する。
再読み込み (scripter プロセス再起動) のたびにこのファイルが再実行される。

---

## `load` — ファイルのロード

**.mayu 相当:** `include "filename.mayu"`

```ruby
load "104.mayu"          # .mayu をコンパイルしてキューに積む (ys_include_mayu)
load "emacsedit.rb"      # .rb を同一 DSL コンテキストで instance_eval
```

拡張子で自動判別する。パス解決は `.mayu` と同じルール (設定ファイルと同ディレクトリ優先)。

Ruby の `Kernel#load` を DSL スコープ内で上書きする。
`Module#include` は上書きしないため、モジュールの include は通常通り使える。

---

## `load_mayu` — 現行 `.mayu` ファイルのロード

```ruby
load_mayu   # ConfigFiles が解決する .mayu をコンパイル (ys_load_mayu)
```

`load "104.mayu"` で個別ファイルを読むのではなく、
`yamy.ini` が指す `.mayu` をそのまま使いたい場合に利用する。

---

## キーシーケンス定義

### `keyseq` — キーシーケンスの定義

**.mayu 相当:** `keyseq $name = actions`

```ruby
# 形式 A: 名前付き (シンボルで参照可能)
keyseq :window_close, "A-F4"
keyseq :toggle_ime,   "A-BackQuote"

# 形式 B: 名前なし、KeySeq オブジェクトを変数に保持
$WINDOW_CLOSE = keyseq "A-F4"   # グローバル変数に保持 (.mayu の $NAME に視覚的に対応)
WM_CLOSE      = keyseq "A-F4"   # 定数への代入も可

# 形式 C: 名前登録 + 変数保持の両立
$WINDOW_CLOSE = keyseq :window_close, "A-F4"
```

形式 B / C の使い分け:

| | シンボル参照 | 変数参照 |
|---|---|---|
| 名前テーブルへの登録 | ✅ (`ys_get_keyseq_idx` で引ける) | ❌ (形式 B) / ✅ (形式 C) |
| `key[...] = rhs` で使える | ✅ `:name` として | ✅ `$VAR` として |
| `to:` kwarg での見た目 | `to: :window_close` (`:` が並ぶ) | `to: $WINDOW_CLOSE` (すっきり) |
| `.mayu` ファイルからの参照 | ✅ `$window_close` として | ❌ |

形式 B で名前テーブル登録が不要な理由: `key["C-S-K"] = $WINDOW_CLOSE` は
`KeySeq#idx` を直接参照するため `ys_get_keyseq_idx` を介さない。
`.mayu` との相互運用が不要な純 Ruby 設定では形式 B で十分。

C API 対応: `ys_reg_keyseq(name, actions)`

- シンボル `:name` → 文字列 `"name"` に変換して登録
- 重複登録は `ys_get_keyseq_idx` で既存インデックスを返す (べき等)
- 戻り値は `Yamy::KeySeq` オブジェクト (内部に `keyseq_idx` を保持)

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

C API 対応: `ys_def_key(names, scancodes)`

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

C API 対応: `ys_def_mod(modifier_name, key_names)`

---

### `defsync` — 同期キー定義

**.mayu 相当:** `def sync = 0x7e`

```ruby
defsync "0x7e"
defsync ["E1-0x1d", "0x45"]
```

C API 対応: `ys_def_sync(scan_codes)`

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

C API 対応: `ys_def_alias(alias_name, key_name)`

---

### `defsubst` — サブスト定義

**.mayu 相当:** `def subst *-LButton = $someseq`

```ruby
# キーワード to: で RHS を指定
defsubst "*-LButton", to: $WINDOW_CLOSE          # KeySeq 変数 (コロンが並ばない)
defsubst "*-LButton", to: :someseq               # シンボル (to: :name でコロンが並ぶ)
defsubst "*-LButton", to: "S-A B C"              # インラインアクション文字列

# LHS 複数指定
defsubst ["*-LButton", "*-RButton"], to: $WINDOW_CLOSE
```

`to: :someseq` はコロンが並んで見えるが、Ruby の構文上 `{to: :someseq}` として
正しく解析される。視覚的に気になる場合は `$VAR` 形式を使う。

C API 対応: `ys_def_subst(lhs_mod_keys, rhs_keyseq_idx)`

RHS (`to:` の値) の解決は `key[...] =` と同じ規則 (Symbol / String / KeySeq)。

---

### `defoption` — オプション定義

**.mayu 相当:** `def option KL- = true`

```ruby
defoption "KL-",            value: true
defoption "delay-of !!!",   value: 500
defoption "mouse-event",    value: true
defoption "drag-threshold", value: 10
```

C API 対応: `ys_def_option(option_name, value)`

`value:` の値は `to_s` で文字列化して渡す (`true` → `"true"`, `500` → `"500"`)。

---

## 条件シンボル (`define` / `symbol?`)

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
if symbol?("KBD109") && !symbol?("KBD104on109")
  # ...
end

load "104.mayu.rb" if symbol?("USE104")
```

C API 対応: `ys_define_symbol(name)` / `ys_has_symbol(name)`

- `define` は呼び出し時点でシンボル集合に追加するため、後続の `symbol?` から見える
  (`.mayu` のファイル順序と同じ意味論)。
- `flushQueue` が flush 時点のシンボル集合をすべて `DefSymbol` として出力するため、
  `define` したシンボルも最終的な `Setting.m_symbols` に含まれる。

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

C API 対応: `ys_begin_keymap(keymap_type, name, window_class, window_title, op, parent_name, default_keyseq_idx)`

| Ruby 引数 | C API 引数 |
|-----------|-----------|
| `class: /pattern/` | `window_class = pattern.source` |
| `class: "string"` | `window_class = string` |
| `title: /pattern/` | `window_title = pattern.source` |
| `op: "&&"` (デフォルト) / `"||"` | `op` |
| `parent: "Name"` | `parent_name` |
| `default: "&Default"` | `default_keyseq_idx` (自動登録) |
| `default: :seq_name` | `default_keyseq_idx` (シンボル解決) |

---

### `key` — キー割り当て (キーマップブロック内)

**.mayu 相当:** `key C-S-M C-A-M = Applications`

`key` はキーマップコンテキストの `KeyMap` オブジェクトを返すメソッド。
`key[LHS...] = rhs` の形式で割り当てる。`[]` は可変長引数を受け付けるため
複数の LHS を二重括弧なしで記述できる。

```ruby
# LHS: 単一
key["*IC-C-Yen"]  = :toggle_ime    # * = down+up
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

C API 対応: `ys_assign_key(lhs_mod_keys, rhs_keyseq_idx)`

**LHS の解決:**

| Ruby 記法 | 変換 |
|-----------|------|
| `key["C-S-M", "C-A-M"]` | `YsStrs(["C-S-M","C-A-M"])` |
| `key["C-S-M C-A-M"]` | 空白分割 → `YsStrs(["C-S-M","C-A-M"])` |
| `key["C-S-L"]` | `YsStrs(["C-S-L"])` |

**RHS の解決 (key / event / defsubst `to:` / `default:` 共通):**

| Ruby 値 | 例 | 変換 |
|---------|-----|------|
| `Symbol` | `:window_close` | `ys_get_keyseq_idx("window_close")` でインデックス取得 |
| `String` | `"A-F4"` | `ys_reg_keyseq(nil, actions)` で匿名登録しインデックス取得 |
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
event["after-key-up"]    = :my_handler
```

C API 対応: `ys_assign_event(event_name, rhs_keyseq_idx)`

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
3. mod[:control] = <↑>   → ys_assign_mod(nil, "control", "+=", YsStrs(["英数"]))
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

C API 対応: `ys_assign_mod(prefixes, modifier_name, op, keys)`

| Ruby | C API |
|------|-------|
| `mod[:control] += "英数"` | `ys_assign_mod(NULL, "control", "+=", ["英数"])` |
| `mod[:control] -= "CapsLock"` | `ys_assign_mod(NULL, "control", "-=", ["CapsLock"])` |
| `mod[:control] = ["LC", "RC"]` | `ys_assign_mod(NULL, "control", "=", ["LC","RC"])` |
| `mod.prefix("!Shift")[:shift] += "LShift"` | `ys_assign_mod(["!Shift"], "shift", "+=", ["LShift"])` |

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

1. `ys_reg_user_func(func_name, handler)` でエンジンに登録
2. `func_name → block` テーブル (mruby Hash) に保存

`on_exec_user_func` コールバック受信時:

1. `func_name` でハッシュを検索
2. `YsFuncArgs*` → Ruby 値の配列に変換 (型変換表参照)
3. `mrb_yield_argv` でブロックを呼び出す

### キーシーケンス文字列内でのユーザー定義関数呼び出し

アクション文字列中でユーザー定義関数を呼び出す方法は 2 通り提供する。

#### 方法 A: `&ExecUserFunc` (C API の生の形式)

```ruby
key["C-F1"] = "&ExecUserFunc(NotifyTime)"
```

#### 方法 B: `@FuncName` プレフィックス (未実装)

バインディング層が `ys_reg_keyseq` に渡す前に `@Name` を `&ExecUserFunc(Name)` へ置換する
糖衣構文。現状は未実装のため、方法 A (`&ExecUserFunc(Name)`) を使うこと。


### `exec_keyseq` — キーシーケンス実行 (deffunc ブロック内)

DSL オブジェクトのメソッドとして提供される (`ys_exec_keyseq` の薄いラッパー)。
`on_exec_user_func` コールバック実行中のみ有効で、受信時のトリガーコンテキストが自動的に引き継がれる。

```ruby
deffunc "DoAction" do
  exec_keyseq "&OSD.Display(\"hello\")"
end
```

制約:
- `on_load_setting` 内では `ys_exec_keyseq` が false を返す
- `&ExecUserFunc` を含む actions は C API レベルでガードされ false を返す (無限ループ防止)

---

## 型変換

### `YsFuncArgs` → Ruby 値 (on_exec_user_func 受信時)

`YsFuncArgs*` の各要素を Ruby 値に変換してブロックに渡す:

| `YsType`             | Ruby 型              | 変換方法 |
|----------------------|----------------------|----------|
| `YsType_String`      | `String`             | UTF-8 文字列 |
| `YsType_Number`      | `Integer`            | `int32_t` |
| `YsType_Regexp`      | `Regexp`             | `"pattern".to_regexp` (mruby 拡張) |
| `YsType_KeySeqIdx`   | `Yamy::KeySeq`       | インデックスをラップ |
| `YsType_ModifierSpec`| `Yamy::Modifier`     | `modifiers` + `dontcares` の 2 値 |
| `YsType_TokenSeq`    | `Array` of `String`  | `YsStrs*` → 文字列配列 |

---

## 実装済み C API

mruby バインディング実装時に追加された C API:

- `ys_include_mayu(path)` — 指定パスの .mayu をコンパイルしてキューに積む
- `ys_last_error()` — 最後のエラーメッセージを返す (UTF-8 NUL 終端、なければ NULL)
- `ys_exec_keyseq(actions)` — キーシーケンスを実行 (`on_exec_user_func` 内でのみ有効)

コールバック typedef:

```c
// exeCtx: ys_start() に渡した呼び出し元コンテキストポインタ (MRubyContext* など)
typedef bool (*ys_on_load_setting)(void* exeCtx);
typedef void (*ys_on_exec_user_func)(void*             /* exeCtx */,
                                     const char*       /* func_name */,
                                     const YsFuncArgs* /* args */);

typedef struct YsCallbacks {
    bool (*on_load_setting)(void* exeCtx);
    void (*on_quit)(void* exeCtx);   // Quit 直前に呼ばれる (NULL 可)
} YsCallbacks;
```

`ys_start` は `YsCallbacks` テーブルと `exeCtx` を受け取る:

```c
YS_API int ys_start(const YsCallbacks* callbacks, void* exeCtx);
```

`on_exec_user_func` は `ys_reg_user_func` で関数ごとに個別登録する:

```c
YS_API bool ys_reg_user_func(const char* func_name, ys_on_exec_user_func on_exec_user_func);
```

`ys_exec_keyseq` での `&ExecUserFunc` ガード (実装済み):
- actions 文字列に `&ExecUserFunc` が含まれる場合は即 false を返す (無限ループ防止)

---

## 完全な設定例

`default.mayu` + `104.mayu` の Ruby 版 (抜粋):

```ruby
# yamy_config.rb

# キーボード定義を .mayu からロード
load "104.mayu"

# キーシーケンス
# 形式 A: シンボルで名前登録 (他の .rb / .mayu ファイルから参照する場合に有用)
keyseq :toggle_ime,   "A-BackQuote"

# 形式 B: 変数保持 (コロンお見合い回避、to: $VAR で使いやすい)
$WINDOW_CLOSE       = keyseq "A-F4"
$WM_VSCROLL_PAGEUP  = keyseq "&PostMessage(ToItself, 277, 2, 0)"
$WM_VSCROLL_PAGEDOWN = keyseq "&PostMessage(ToItself, 277, 3, 0)"

# Global キーマップ
keymap "Global" do
  key["*IC-C-Yen"]                  = :toggle_ime
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
`ctx->argv[1]` または `ys_get_home_directories()` を使って解決する。

### コールバック

```cpp
// mruby_binding.h / mruby_binding.cpp

// YsCallbacks.on_load_setting に渡す。mrb_open してDSLクラス登録→スクリプト実行
bool mruby_on_load_setting(void* exeCtx);  // exeCtx = MRubyContext*

// YsCallbacks.on_quit に渡す。mrb_close して mrb を nullptr に
void mruby_on_quit(void* exeCtx);          // exeCtx = MRubyContext*

// ys_reg_user_func で関数ごとに登録する。g_funcTable からブロックを検索し呼び出す
void mruby_on_exec_user_func(void* exeCtx, const char* func_name, const YsFuncArgs* args);
```

### `mruby_main.cpp` の構成

```cpp
#include "yamy_scripter.h"
#include "mruby_binding.h"
#include <windows.h>

int main(int argc, char *argv[])  // UTF-8 activeCodePage マニフェスト使用
{
    MRubyContext ctx = { argc, (const char* const*)argv, nullptr };

    YsCallbacks callbacks = {};
    callbacks.on_load_setting = mruby_on_load_setting;
    callbacks.on_quit         = mruby_on_quit;

    return ys_start(&callbacks, &ctx);
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

- **`deffunc` とキーマップ定義の順序**: 順序制約なし。
  `ys_reg_user_func` の呼び出しタイミングはキューイングの順序に影響しない。
  未登録関数への `&ExecUserFunc` 呼び出しは Engine 側でエラー扱いになる。

---

## 関連ドキュメント

- [c-api.md](c-api.md) — DLL 公開 C API 仕様
- [typed-args.md](typed-args.md) — 型付き引数システム
- [exe-design.md](exe-design.md) — EXE 設計パターン
- [overview.md](overview.md) — 全体構成と変更目的

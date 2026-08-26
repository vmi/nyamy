## 5. customize {#CUSTOMIZE}

付属のサンプル設定 ([`dot.mayu.rb`](../dot.mayu.rb)) を利用すれば、エディットコントロールで Emacs 風の操作ができるようになりますが、設定ファイルをカスタマイズすることによって、Windows を自分の好きなキーバインディングで利用することができるようになります。

NYamy の設定ファイルには 2 つの形式があります。

- **`.mayu.rb` 形式** — mruby による Ruby DSL。NYamy の標準の形式で、付属のサンプル設定もこの形式です。[Ruby DSL](#RUBYDSL) の章を参照してください。
- **`.mayu` 形式** — 「窓使いの憂鬱」以来の伝統的なテキスト形式。`.mayu.rb` から `load` で取り込むこともできます。

この章では、キー・モディファイヤ・キーマップなどの概念と各機能を、`.mayu` 形式の文法で説明します。ここで説明する概念はどちらの形式でも共通で、Ruby DSL での対応する書き方は [Ruby DSL](#RUBYDSL) の章にまとめてあります。

設定ファイルは[設定フォルダ](#HOME)から検索されます。

`.mayu` は上から下へ読まれていき、重複する記述があれば、より下に書かれているものが有効になります。コメントは `#` ではじめます。アルファベットの大文字と小文字は区別されません。詳しい文法は [`syntax.txt`](syntax.txt) を参照してください。

### i. キー割り当ての変更 {#key}

キー割り当てを変更するには、以下のように記述をします。

```mayu
key ⟨KEY⟩ = ⟨KEY⟩ や ⟨FUNCTION⟩ …
```

`=` より左の `⟨KEY⟩` をキーボードで押すと、Windows へは `=` より右の `⟨KEY⟩` が順番に入力されます。また、右に `⟨FUNCTION⟩` が書かれている場合はウィンドウの最大化や移動などの機能が実行されます。

`⟨KEY⟩` は[キーボード定義](#keyboard)で定義されるもので、デフォルトでは [`109.mayu.rb`](../109.mayu.rb) 又は [`104.mayu.rb`](../104.mayu.rb) で定義されている `⟨KEY⟩` が使用できます。

#### モディファイヤの指定 {#modifier}

`⟨KEY⟩` の前に以下のような記号を付けることによって、コントロールキーなどの状態を表現できます。また、これらをモディファイヤと呼ぶことにします。

- `C-` は、[[Control]] が押されていることを表します。
- `M-` か `A-` は、[[Alt]] が押されていることを表します。
- `S-` は、[[Shift]] が押されていることを表します。
- `NL-` は、[[NumLock]] がロック状態であることを表します。
- `CL-` は、[[CapsLock]] がロック状態であることを表します。
- `SL-` は、[[ScrollLock]] がロック状態であることを表します。
- `KL-` は、[[カナ]] がロック状態であることを表します。  
  ([オプション (`KL-`)](#def_option_KL)をよく読んでください)  
  109 キーボードなら、[[Control]] + [[Shift]] + [[ひらがな]]。  
  104 キーボードなら、[[Control]] + [[Shift]] + [[CapsLock]]。
- `IL-` は、IME が on になっていることを表します
- `IC-` は、IME で変換中であることを表します
- `MAX-` は、ウィンドウが最大化されていることを表します。
- `MIN-` は、ウィンドウが最小化されていることを表します。
- `MMAX-` は、MDI 子ウィンドウが最大化されていることを表します。
- `MMIN-` は、MDI 子ウィンドウが最小化されていることを表します。

以下のように記述すると、[[Control]] + [[A]] を押した時に、Windows へは [[HOME]] キーが入力されます。

```mayu
key C-A = HOME
```

#### モディファイヤキーの無視 {#ignoreModifier}

上記の例では左側に `C-A` と記述していますが、この記述では、ロックキーなどは押されていても押されてなくても良いと記述していることになります。たとえば、[[CapsLock]] を押したあとで [[Control]] + [[A]] を押しても、押さずに [[Control]] + [[A]] を押しても、Windows へは [[Home]] が入力されます。

特定のモディファイヤの状態を無視したい場合は、モディファイヤに "`*`" をつけます。逆にモディファイヤが必ず押されていなければならない場合は付けません。またモディファイヤが必ず離されていなければならない場合は "`~`" を付けます。たとえば、

```mayu
key *S-F9 = &WindowMinimize
```

このように記述すると、[[F9]] 又は [[Shift]] + [[F9]] でウィンドウを最小化することができますが、例えば、[[Control]] + [[F9]] ではできません。

デフォルトでは、`C-` `M-` `S-` `W-` と `M0-`〜`M9-` には "`~`" が、`NL-` `CL-` `SL-` `KL-` `IL-` `IC-` `MAX-` `MIN-` `MMAX-` `MMIN-` と `L0-`〜`L9-` には "`*`" が暗黙に指定されていることになっています。`U-` `D-` については[キーを押す/離す](#keyUpDown)、`R-` については[キーリピートした](#keyRepeat)を参照してください。

また、[[Shift]] は必ず押されていてほしいがほかのモディファイヤはどうでもいいという場合は、

```mayu
key S-*F9 = &WindowMinimize
```

というように "`*`" をキーの直前に記述します。"`~`" についても同様です。

#### 入力されたキーと同じモディファイヤの指定 {#inputModifier}

`=` より右側でのモディファイヤの指定の方法です。

```mayu
key *S-A = C-*S-B
```

例えばこのように記述した場合、[[Shift]] + [[A]] を押すと、Windows へは [[Shift]] + [[Control]] + [[B]] が入力されます。[[A]] を押すと、Windows へは [[Control]] + [[B]] が入力されます。

つまり、`=` の右側で `*` で指定されたモディファイヤは、キーボードで実際に入力したモディファイヤと同じになるように設定されます。

したがって、[[A]] を [[B]] と入れ替えたい場合は、

```mayu
key *A = *B

key *B = *A
```

となります。

#### キーを押す/離す {#keyUpDown}

`⟨KEY⟩` の前にモディファイヤと同じように `D-` と `U-` を付けることができます。これは、それぞれキーの押すことと離すことに対応しています。デフォルトでは `*D-*U-` が指定されています。例えば、

```mayu
key A = B C
```

という記述は、

```mayu
key *U-*D-A = D-B U-B D-C U-C
```

と同じであり、さらに次のものとも同じになります。キーリピートが起こった場合は、`~U-D-A` が何度も実行され、キーを離したときに `U-~D-A` が実行されます。

```mayu
key ~U-D-A = D-B U-B D-C

key U-~D-A = U-C
```

#### キーリピートした {#keyRepeat}

`=` より左の `⟨KEY⟩` の前にモディファイヤと同じように `R-` を付けることができます。これは、キーリピートが発生したことを表します。デフォルトでは `*R-` が指定されています。例えば、

```mayu
key A = B

key R-A = C
```

という記述をすると、[[A]] を押しつづけると、

```mayu
BCCCCCCCCCCCCCCCCCCCCC
```

と入力されます。とてもややこしいのであまり使わないようにしましょう。

### ii. キーマップ定義 {#keymap}

NYamyには、キーマップという概念があります。キーマップにカスタマイズしたいキー情報を書き込んでゆき、ウィンドウごとにキーマップを使い分けます。キーマップを定義するには、以下のどれかの文を書いてからキーを設定します。

```mayu
keymap ⟨キーマップ名⟩

keymap2 ⟨キーマップ名⟩

window ⟨キーマップ名⟩ ⟨ウィンドウクラス名⟩

window ⟨キーマップ名⟩ ( ⟨ウィンドウクラス名⟩ && ⟨ウィンドウタイトル名⟩ )

window ⟨キーマップ名⟩ ( ⟨ウィンドウクラス名⟩ || ⟨ウィンドウタイトル名⟩ )
```

例えば、メモ帳で [[Control]] + [[Z]] を押すと最小化されるが、メモ帳以外のエディットコントロールで [[Control]] + [[Z]] を押すと単なる [[Z]] キーと同じになるという指定がしたい場合は、

```mayu
window EditControl /:Edit$/ : Global

key C-Z = Z

window Notpad /Notepad:Edit$/ : Global

key C-Z = &WindowMinimize
```

と記述します。ここで `/Notepad:Edit$/` はメモ帳の上にあるエディットコントロールの`⟨ウィンドウクラス名⟩`を表しています。`⟨ウィンドウクラス名⟩`は正規表現で記述します。`: Global` は`⟨親キーマップ⟩`を指定しています。

#### ウィンドウクラス/タイトル名 {#windowClass}

Windows の全てのウィンドウは、何らかのウィンドウクラスに属しています。例えば、メモ帳のウィンドウクラス名は `Notepad` で、エディットコントロールのウィンドウクラス名は `Edit` です。

NYamyは、どのウィンドウでどのキーを押したらどんな動作をするか、ということを区別するために`⟨ウィンドウクラス名⟩`と`⟨ウィンドウタイトル名⟩`を用いています。そのために、NYamyではウィンドウの重なりの状態を "`:`" で繋げて表現します。例えば、メモ帳の上のエディットコントロールの`⟨ウィンドウクラス名⟩`ならば、

```mayu
C:\WINDOWS\system32\notepad.exe:Notepad:Edit
```

と表現します。ただし、`⟨ウィンドウクラス名⟩`の一番最初にはそのアプリケーションのパス名を付けています。

`window` 文には、この`⟨ウィンドウクラス名⟩`と`⟨ウィンドウタイトル名⟩`を記述することができますが、`⟨ウィンドウクラス名⟩`全てを書く必要はなく、正規表現で省略することができます。

例えば、`/:Edit$/` は全てのエディットコントロールの`⟨ウィンドウクラス名⟩`を表しますし、`/:#32770.*:Edit$/` ならば、ダイアログボックス上にある全てのエディットコントロールの`⟨ウィンドウクラス名⟩`を表します (`#32770` はダイアログボックスのウィンドウクラス名)。

個々のウィンドウの`⟨ウィンドウクラス名⟩`と`⟨ウィンドウタイトル名⟩`を調べるには、タスクトレイメニュー[調査(I)...](#menu-i)の「ウィンドウの調査」、または`⟨FUNCTION⟩` [`&WindowIdentify`](#function_WindowIdentify) を利用してください。

`⟨ウィンドウクラス名⟩`と`⟨ウィンドウタイトル名⟩`の両方を記述する場合は、括弧で囲みその間を `&&` か `||` で区切ります。`&&` の場合は、両方にマッチするようなウィンドウを表し、`||` の場合はどちらか一方にマッチするようなウィンドウを表します。

#### 正規表現について {#regexp}

`⟨ウィンドウクラス名⟩`と`⟨ウィンドウタイトル名⟩`には正規表現が使用できます。正規表現は `/.../` で囲むか、`\m@...@`で囲みます (ただし`@`はどんな文字でも良いです)。

正規表現エンジンには C++ 標準ライブラリの `std::wregex` (ECMAScript 文法) を使用しています。マッチの際、大文字と小文字は常に区別されません。よく使いそうなものを挙げておきます。

- "`|`" Alternation
- "`*`" Match 0 or more times
- "`+`" Match 1 or more times
- "`?`" Match 1 or 0 times
- "`.`" Match any character
- "`^`" Match the beginning of the string
- "`$`" Match the end of the string
- "`\b`" Match a word boundary
- "`\B`" Match a non word boundary
- "`\w`" Match a word character (`[0-9a-z_]`)
- "`\W`" Match a non word character
- "`\s`" Match a whitespace character
- "`\S`" Match a non-whitespace character
- "`\d`" Match a digit character
- "`\D`" Match a non-digit character
- "`(`" "`)`" Grouping
- "`[`" "`]`" Character class
- より詳しくは ECMAScript (JavaScript) の正規表現のリファレンスを参照してください。文法はほぼ共通です。

#### 親キーマップ {#parentKeymap}

親キーマップとは、現在のキーマップに適切なキー割り当てが定義されていない場合に、キーを捜しに行くキーマップです。"`:`" の後ろに*親キーマップ名*を書きます。例えば、

```mayu
keymap sub : Global

key	C-A = &WindowMinimize

window EditControl /:Edit$/ : sub

key	C-A = &KeymapParent
```

と記述した場合、エディットコントロールで [[Control]] + [[A]] を入力すると、ウィンドウは最小化されます。つまり、[`&KeymapParent`](#function_KeymapParent) を記述することで親キーマップで定義されたキーを利用することができるのです。もし、*親キーマップ名*が指定されていなければ [`&Default`](#function_Default) 扱いとなり、ウィンドウへキーがそのまま入力されます。

#### デフォルトキー {#defaultKey}

`keymap`、`window`、`keymap2` には、最後にキーを羅列することによってデフォルトキーを定義することができます。例えば、

```mayu
window EditControl /:Edit$/ : Global = A

key *B = *C
```

と記述すると、[[B]] を入力すると [[C]] を入力したことになるが、[[B]] 以外のキーを入力すると、[[A]] を入力したことになります。また、デフォルトキーを指定しなかった場合のデフォルトキーは、`keymap` と `window` の場合は [`&KeymapParent`](#function_KeymapParent) で、`keymap2` の場合は [`&Undefined`](#function_Undefined) になります。

#### 二段階キーマップ {#keymap2}

`keymap2` はデフォルトキーが [`&Undefined`](#function_Undefined) になってるようなキーマップで、主に [`&Prefix`](#function_Prefix) を利用して 2 ストロークキーを記述する時に使用します。

#### 初期キーマップ {#initialKeymap}

`.mayu` の一番最初の行には、

```mayu
window Global ( // || // ) = &OtherWindowClass
```

という行が隠れていると考えて下さい。つまり、`.mayu` で何もキーマップを指定せずに書き始めると、*キーマップ名* `Global` のキーマップに対するキー定義になるということです。そして、`Global` キーマップのデフォルトキーは [`&OtherWindowClass`](#function_OtherWindowClass) が設定されています。

#### 矛盾したキーマップの指定 {#conflictKeymap}

同じキーマップに対する `keymap` や `window` や `keymap2` は何度でも指定できますが、矛盾する指定をしてはいけません。例えば、

```mayu
keymap Amap : Global

...

keymap Bmap : Amap

...

keymap Amap : Global

...
```

という指定は問題ありませんが、

```mayu
keymap Amap : Global

...

keymap Bmap : Amap

...

keymap Amap : Bmap	# 矛盾

...
```

という指定はしてはいけません。この場合、`keymap Amap : Bmap` のかわりに `keymap Amap : Global` が指定されたものとみなされます。エラーは出ません。

#### `window` に複数該当する場合 {#matchManyClasses}

例えば、

```mayu
window EditControl /:Edit$/ : Global

key A = A space E D I T enter

key B = B space E D I T enter

window Notepad /:Notepad/ : Global

key A = A space N O T E P A D enter

key C = C space N O T E P A D enter
```

という記述をしたとします。ここで、「メモ帳」を立ち上げると、メモ帳の`⟨ウィンドウクラス名⟩`は

```mayu
C:\WINDOWS\system32\notepad.exe:Notepad:Edit
```

となっているので、`/:Edit$/` と `/:Notepad/` は両方共もメモ帳の`⟨ウィンドウクラス名⟩`に該当します。この時、[[A]] を入力すると、メモ帳には「`a notepad`」と表示されます。これは、重複する記述があれば、より下に書かれているものが有効になるからです。しかし、[[B]] を入力した場合は、重複していないので、メモ帳には「`b edit`」と表示されることになります。

[[B]] を入力した場合に、内部で行われる処理は以下のようになります。

1. まず`⟨ウィンドウクラス名⟩`は `/:Notepad/` に該当しますが、キー割り当てがないので、`window` のデフォルトキーである `&KeymapParent` が採用されます。
2. `&KeymapParent` は*親キーマップ*の参照なので、`Global` キーマップを参照します。
3. そうすると、`Global` キーマップでも [[B]] の割り当てがないので、`Global` キーマップのデフォルトキーである、`&OtherWindowClass` が採用されます。
4. `&OtherWindowClass` が採用されると、まず、他に該当する`⟨ウィンドウクラス名⟩`がないかどうか探します。もしなければ、`&Default` 扱いとなります。この場合は `/:Edit$/` に該当します。
5. `/:Edit$/` に該当したので、`/:Edit$/` の [[B]] が採用されます。したがって、「b edit」と表示されることとなります。

#### キーマップが影響する定義 {#perKeymapDefinition}

以下の単語で始まる定義は、キーマップ毎に定義できます。

- `key ...` [キー割り当ての変更](#key)
- `event ...` [イベント定義](#event)
- `mod ...` [モディファイヤキー割り当ての変更](#mod)

以下の単語で始まる定義は、キーマップ毎に定義することはできません。

- `def ...` [キーボード定義](#keyboard)
- `keyseq ...` [キーシーケンス定義](#keyseq)

### iii. モディファイヤキー割り当ての変更 {#mod}

```mayu
mod ⟨モディファイヤキー名⟩ = ⟨キー名⟩ …

mod ⟨モディファイヤキー名⟩ += ⟨キー名⟩ …

mod ⟨モディファイヤキー名⟩ -= ⟨キー名⟩ …
```

最初の 3 つは、*キー名*で指定したキーをモディファイヤキーにしたり (`=`) 追加したり (`+=`) 削除したり (`-=`) します。各キーマップ毎に割り当てます。明示的に割り当てない場合は、親キーマップから引き継がれます。例えば、

```mayu
mod shift += 無変換
```

は、[[無変換]] キーを shift モディファイヤキーにします。従って、

```mayu
key S-A = X
```

という記述があった場合に、[[無変換]] + [[A]] を押すと [[X]] を入力したことになります。正確には、[[無変換押す]] [[X押す]] [[X離す]] [[無変換離す]] というキーが Windows へ入力されます。これでは都合が悪いということは多いと思われるので、

```mayu
key *無変換 = *LShift
```

として [[無変換]] キーを押すと [[LShift]] が入力されるように割り当てます。そうすれば、Windows へは [[LShift押す]] [[LShift離す]] [[X押す]] [[X離す]] というキーが入力されます。

*モディファイヤキー名*には、`shift`, `alt` (`meta`, `menu`), `control` (`ctrl`), `windows` (`win`), `mod0`〜`mod9` が記述できます。括弧の中の名前も使用できます。

`mod0`〜`mod9` はNYamyの中でのみ有効なモディファイヤで、例えば以下のように使用します。

```mayu
mod mod0 = Up

key M0-Left = Left Up
```

このように割り当てると、[[↑]] を押しながら [[←]] を押すとカーソルが左斜め上へ移動することになります。

#### 真のモディファイヤ {#trueModifier}

モディファイヤにしたいキーの前に "`!`" を付けると、*真のモディファイヤ*になります。例えば、

```mayu
mod shift += !無変換

key 無変換 = Y

key S-A = X
```

と記述した場合、[[無変換]] + [[A]] を押すと [[X押す]] [[X離す]] というキーが Windows へ入力されます。Windows からは、[[無変換]] キーが押されたということは分かりませんし、[[Y]] も Windows へ入力されることはありません。つまり、真のモディファイヤに定義されているキーや `⟨FUNCTION⟩` などは実行されません。

以下のような行を記述すると、

```mayu
mod !⟨モディファイヤキー名⟩
```

その*モディファイヤキー名*に割り当てられているモディファイヤを全て*真のモディファイヤ*に変更します。

#### One Shot モディファイヤ (SandS) {#oneShotModifier}

(一般的には SandS と呼ばれている機能です。Space and Shift の略です。)

モディファイヤにしたいキーの前に "`!!`" を付けると、*One Shot モディファイヤ*になります。たとえば、

```mayu
mod shift = !!LShift

key S-A = X

key S-LShift = Y
```

と記述した場合、[[LShift]] を押してすぐ離した場合は、Windows へは、[[Y]] が入力されますが、[[LShift]] + [[A]] を入力した場合は、[[X]] のみが Windows へ入力されます。

以下のような行を記述すると、

```mayu
mod !!⟨モディファイヤキー名⟩
```

その*モディファイヤキー名*に割り当てられているモディファイヤを全て*One Shot モディファイヤ*に変更します。

#### One Shot (キーリピート有) {#oneShotRepeatableModifier}

*One Shot モディファイヤ*は通常キーリピートしませんが、"`!!!`" を付けると、キーリピートをするようになります。たとえば、

```mayu
mod shift = !!!Up
```

とすると、[[↑]] を押しながら何か別のキー (例えば [[A]]) を押すと [[Shift]] + [[A]] と同じことになりますが、[[↑]] を押しっぱなしにすると [[↑]] がキーリピートして、カーソルが上へ動くということになります。

キーリピートが開始するまでの時間を[オプション (`delay-of !!!`)](#def_option_delay_of_oneShotRepeatableModifier) で設定できます。

#### ロックキー {#lock}

NYamyには、NYamyの中でのみ有効なロックキーが存在します。これらはキーのモディファイヤとして `L0-`〜`L9-` を書くことができ、[`&Toggle`](#function_Toggle) を使うことによりトグルさせることができます。例えば、

```mayu
key ひらがな = &Toggle(Lock0)

key L0-A = B
```

と記述すると、[[ひらがな]] キーがトグル状態になっているときに [[A]] を押すと Windows へは [[B]] が入力されます。

### iv. キーシーケンス定義 {#keyseq}

```mayu
keyseq $⟨キーシーケンス名⟩ = ⟨KEY⟩ や ⟨FUNCTION⟩ …
```

`keyseq` を使うことで、一連のキー入力に対して名前を付けることができます。例えば、

```mayu
keyseq $Right2Times = Right Right

key C-F = $Right2Times
```

とすると、[[Control]] + [[F]] で右に二つカーソルを進めることができます。又、

```mayu
key C-F = Right Right
```

は、`$Right2Times` という名前が定義されないこと以外は、先の例と同じになります。

### v. イベント定義 {#event}

```mayu
event ⟨EVENT⟩ = ⟨KEY⟩ や ⟨FUNCTION⟩ …
```

あるイベントが起こったときに `⟨KEY⟩` や `⟨FUNCTION⟩` を実行します。イベントはキーマップ毎に定義され、親キーマップにイベントが定義されていてもそれは無視されます。

`⟨EVENT⟩` には以下のものが指定できます。

- `prefixed`: [`&Prefix`](#function_Prefix) によってキーマップが指定された時。
- `before-key-down`: キーが押さた時。
- `after-key-up`: キーが離された後。

### vi. キーボード定義 {#keyboard}

デフォルトのキーボード定義は [`109.mayu.rb`](../109.mayu.rb) 又は [`104.mayu.rb`](../104.mayu.rb) に書かれています。

#### キー定義 {#def_key}

キーボードの物理的なキーを定義します。

```mayu
def key ⟨キー名⟩… = ⟨スキャンコード⟩…
```

キーが発生する`⟨スキャンコード⟩`を記述していきます。`⟨スキャンコード⟩`は数字で書き、`E0-` や `E1-` という拡張キーフラグをつけることができます。又、

```mayu
def key Pause = E1-0x1d 0x45
```

このように一連の*スキャンコード*を発生させるキーにはスキャンコードを書き並べます。

#### モディファイヤ定義 {#def_mod}

キーボードの物理的なモディファイヤキーを定義します。

```mayu
def mod ⟨モディファイヤ名⟩ = ⟨キー名⟩…
```

`⟨モディファイヤ名⟩`には、`shift`, `alt` (`meta`, `menu`), `control` (`ctrl`), `windows` (`win`) が記述できます。括弧の中の名前も使用できます。

#### 同期定義 {#def_sync}

`&Sync` に使用する`⟨スキャンコード⟩`を定義します。

```mayu
def sync = ⟨スキャンコード⟩…
```

`&Sync` が実行されるとき、NYamyはこの`⟨スキャンコード⟩`を Windows に送ります。そして、各ウィンドウがこのキーが入力されたことをNYamyへ連絡してくるまで処理を中断します。このようにして同期をとるので、この`⟨スキャンコード⟩`が不正に設定されていると、同期がとれずNYamyが 5 秒ほど固まります (つまり 5 秒ほど何も入力できなくなります)。

#### 別名定義 {#def_alias}

キーの別名を定義します。

```mayu
def alias ⟨別名⟩ = ⟨キー名⟩
```

別名が既存のキー名と同じだった場合は、別名のほうが優先されます。

#### 代用定義 {#def_subst}

あるキーを別のキーとして代用します。

```mayu
def subst ⟨KEY⟩ = ⟨KEY⟩ や ⟨キーシーケンス⟩ …
```

キーが入力されると、まずこの代用定義によって入力されたキーが置き換えられます。その後、[キー割り当ての変更](#key)に従って変換されます。

```mayu
def subst A = B

key B = C
```

上記の例では、[[A]] を入力すると、まず代用定義で [[B]] が押されたことになって、[[B]] が入力された場合は [[C]] が最終的に Windows へ入力されるので、結局 [[A]] を押すと [[C]] が押されたことになります。

代用定義は、キーマップでキーが変更されるより前に実行されます。例えば、109 キーボード上で 104 キーボードや Dvorak のエミュレートをしたいときに使用します。

`=` の左右は[キー割り当ての変更](#key)のものと同じものが指定でき意味も同じになりますが、右側は先頭が `⟨FUNCTION⟩` ではなく `⟨KEY⟩` でなければならず、先頭の `⟨KEY⟩` しか意味を持ちません。

以下色々な例。

```mayu
def subst A = C-B

key *B = S-*C
```

上記の例では [[A]] を入力すると、最終的に [[Shift]] + [[C]] が Windows へ出力されます。

```mayu
def subst A = B C D $Hoge &Toggle(Lock0)
```

上記の例では [[A]] を入力すると、[[B]] が Windows へ出力されます。`C D $Hoge &Toggle(Lock0)` は無視されます。

```mayu
keyseq $COLON = ~S-*Colon

def subst S-*Semicolon = $COLON
```

上記の例では [[Shift]] + [[;]] を入力すると、[[:]] になり、[[Control]] + [[Shift]] + [[;]] を入力すると、[[Control]] + [[:]] になります。

#### オプション (`KL-`) {#def_option_KL}

カナロック `KL-` を正しく設定するようにします。

```mayu
def option KL- = enable
```

このオプションを設定しない場合、特定の場合にカナロックの状態が正しく取得できません。

このオプションを設定すれば、カナロックの状態は正しく取得できますが、IME で文字列を入力中未確定のまま別のウィンドウへフォーカスを切り替え、元のウィンドウへフォーカスを戻した時に、IME に入力中だった文字列は失われます。

109 キーボードで [[Alt]] + [[ひらがな]] を使用するカナロックにはうまく対応できませんでした。普段 [[Alt]] + [[ひらがな]] を使用している人は、

```mayu
keymap Global

key *IC-*IL-A-ひらがな = C-S-ひらがな
```

という設定をして代わりに[[Control]] + [[Shift]] + [[ひらがな]] が使用されるようにしてください。

また、IME の機能の「日本語入力と連動してカナロックをon/offする」という設定にしている場合もカナロックの状態を正しく取得できないという報告があります。その場合は IME を on/off にするキーに、同時にカナロックもしてくれるように設定すると良いでしょう。

> 1. [ログウインドウ](#menu-l)を表示して、「□詳細(<u>D</u>)」をチェックしておきます。
> 2. 「メモ帳」を 2 つ起動し左右に並べます。
> 3. 左のメモ帳でカナをロックして何文字か入力してください。  
>    109 キーボードなら、[[Control]] + [[Shift]] + [[ひらがな]]。  
>    104 キーボードなら、[[Control]] + [[Shift]] + [[CapsLock]]。  
>    (左のメモ帳には半角カタカナが表示されます)
> 4. 右のメモ帳に何文字が入力してください。  
>    (右のメモ帳には半角カタカナが表示されます)
> 5. IME をオンにしてローマ字入力にして右のメモ帳に何文字か入力してください。
> 6. 左のメモ帳に何文字か入力してください。  
>    (左のメモ帳には半角カタカナが表示されます)
> 7. ここで、[ログウインドウ](#menu-l)を見ると `KL-` が付いていません。
> 
> カナロックの状態は、IME がオンの時とオフの時で、べつべつに記憶されているようです。
> 
> しかし、IME がオンのウィンドウから IME がオフのウィンドウへフォーカスが移ったときに、カナロックの状態は正しく反映されないようです。
> 
> その後 IME をオンオフするタイミングで、カナロックの状態が正しく反映されます。
> 
> そこで、このオプションを設定すると、フォーカスが変化した時に IME オンオフを自動的に行い、カナロック状態を反映します。

#### オプション (`delay-of !!!`) {#def_option_delay_of_oneShotRepeatableModifier}

[キーリピート有 One Shot (`!!!`)](#oneShotRepeatableModifier) のキーリピートが始まるまでの時間を指定します。

```mayu
def option delay-of !!! = ⟨DELAY⟩
```

最初の `⟨DELAY⟩` 回のキーリピートを無視するようにします。

デフォルトでは `⟨DELAY⟩` は 0 です。

#### オプション (`nls-keys`) {#def_option_nls_keys}

キーを離したイベントが NYamy に届かないキーのスキャンコードを指定します。

```mayu
def option nls-keys = "⟨スキャンコード⟩, ⟨スキャンコード⟩, ..."
```

日本語環境では、日本語処理に使われる一部のキー (NLS キー) の解放イベントがキーボードレイアウトドライバに消費され、NYamy まで届きません。NYamy から見るとキーが押されたままになるため、そのキーが押しっぱなしとして扱われ続け、モディファイヤの自動解放も行われなくなります。

このオプションに指定したスキャンコードについては、押下イベントを受け取った直後に解放イベントを NYamy 内部で生成し、この状態を回避します。

スキャンコードは 10 進数、または `0x` を付けた 16 進数で書きます。`E0-` / `E1-` の前置も使えます。スキャンコードの代わりに、[`def key`](#def_key) で定義したキーの名前を書くこともできます。区切りはカンマまたは空白です。

```mayu
def option nls-keys = "半角/全角, 英数, ひらがな, 無変換"
def option nls-keys = "0x29, 0x3a, 0x70, 0x7b"
```

上記はどちらも同じ意味で、kbd106.dll を使用している場合の NLS キーの例です。kbd101.dll の場合は `0x29` と `0x3a` の 2 つです。

キー名を使う場合は、そのキーが**このオプションより前に定義されている**必要があります。複数のスキャンコードを持つキー (`Pause` など) は指定できません。

`E0` で始まる名前のキー (`E0英数` など) はそのまま名前として解釈されます。スキャンコードの前置と紛れることはありません。

デフォルトでは何も指定されておらず、解放イベントの生成は行われません。

指定したキーは**モディファイヤにできません**。押下と同時に解放が生成されるためです。英数キーを [[Control]] にするなど、NLS キーをモディファイヤとして使いたい場合は、このオプションではなく、レジストリの Scancode Map で当該キーに `E0-` を付けて別のキーに逃がしたうえで、そのキーを定義し直す方法を使ってください。

### vii. ファイル読み込み {#include}

```mayu
include ⟨ファイル名⟩
```

と書くことによって、その行に*ファイル名* [文字列](#string)で示されるファイルを挿入することができます。*ファイル名*は[設定フォルダ](#HOME)から検索されます。(`.mayu.rb` からのファイル読み込みには [`load` / `require`](#dsl_load) を使用します)

#### 設定フォルダ {#HOME}

NYamy は以下の 3 つのフォルダを使い分けます。それぞれ環境変数として `nyamy-scripter` にも渡されます。

| 環境変数 | フォルダ | 用途 |
|---|---|---|
| `NYAMY_ROOT` | `nyamy.exe` のあるフォルダ | 配布物 (`dot.mayu.rb` や `109.mayu` など) |
| `NYAMY_CONFIG` | `%LOCALAPPDATA%\NYamy\Config` | 自分用の設定ファイル、`nyamy.ini` |
| `NYAMY_HOME` | `%LOCALAPPDATA%\NYamy` | 自分用のデータ。`Lib` サブフォルダは `.rb` ライブラリ置き場 |

設定ファイルは `NYAMY_CONFIG`、`NYAMY_ROOT` の順に検索されます。<strong>カレントディレクトリは検索されません。</strong>自分用の設定ファイルは `%LOCALAPPDATA%\NYamy\Config` に置いてください。タスクトレイメニューの<span class="menu-item">(ホームディレクトリから)</span>は、このフォルダから `.mayu.rb` を読み込みます。

なお、`.mayu.rb` の `load` / `require` の検索順はこれとは少し異なります。詳しくは [`load` / `require`](#dsl_load) を参照してください。

#### scripter の起動 (`cmdLine`) {#cmdline}

設定ファイルを読み込むのは、NYamy 本体が起動する `nyamy-scripter.exe` という別プロセスです。その起動コマンドラインは `nyamy.ini` の `cmdLine` で変更できます。

```ini
cmdLine="${NYAMY_ROOT}\nyamy-scripter.exe .mayu.rb"
```

実行ファイルから引数まで、コマンドライン**全体**を書きます。`${変数名}` の形で任意の環境変数を展開できます。未定義の変数は空文字列に展開され、ログに警告が出ます。`%変数名%` の形式は使えません。

指定できるオプションは次のとおりです。

| オプション | 意味 |
|---|---|
| `-I ⟨フォルダ⟩` | [`$LOAD_PATH`](#dsl_load) にフォルダを追加する。複数回指定可。`;` で区切って一度に複数指定も可。**絶対パスのみ** |
| `-D ⟨シンボル⟩` | [シンボル](#cond)を定義する。複数回指定可 |
| `--` | これ以降をオプションとして解釈しない |
| `-h`, `--help` | 使い方を表示する |
| `--version` | バージョンを表示する |

```ini
cmdLine="${NYAMY_ROOT}\nyamy-scripter.exe -I ${HOME}\nyamy-lib -D MYFLAG .mayu.rb"
```

`-I` に相対パスを指定するとエラーになります。カレントディレクトリを基準にしてしまうと、NYamy がどこから起動されたかで設定の内容が変わってしまうためです。`${NYAMY_HOME}` などを使って絶対パスで指定してください。

環境変数 `NYAMY_LOAD_PATH` でも `$LOAD_PATH` にフォルダを追加できます (`;` 区切り、絶対パスのみ)。`nyamy.ini` を書き換えずに済ませたい場合に使ってください。こちらは相対パスを含んでいてもエラーにはならず、その要素だけが警告つきで無視されます。

`nyamy-scripter` に渡される環境には、`NYAMY_ROOT` / `NYAMY_CONFIG` / `NYAMY_HOME` のほかに `HOME` が加わります。Windows が `HOME` を定義していない場合は `%USERPROFILE%` と同じ値になります (既に定義されている場合はその値のままです)。この `HOME` は `nyamy-scripter` とそこから起動されるプロセスにのみ渡され、NYamy 本体や [`&ShellExecute`](#function_ShellExecute) で起動したアプリケーションには影響しません。設定ファイルからは [`ENV`](#dsl_env) で読めます。

なお `cmdLine` は、scripter のプロトコルを話す別の実装に差し替えるためのものでもあります。通常は変更する必要はありません。

#### ログの書式 {#logformat}

[ログウインドウ](#menu-l)の各行は、`hh:mm:ss.SSS` の時刻とレベル 1 文字を `|` で区切った 15 文字が先頭に付きます。日付が変わると `[YYYY-MM-DD]` だけの行が挿入されます。この行だけは桁の並びが崩れますが、それがそのまま「日付が変わった」という目印になります。

```
[2026-08-10]
12:34:56.789|D|IN    0x1d  D-LeftControl
12:34:56.789|D|  FN  &OtherWindowClass
12:34:56.790|D|    OUT 0x1d  U-LeftControl
12:34:57.102|W|gave up adding the tasktray icon.
12:34:58.004|E|too deep keymap recursion.  there may be a loop.
```

レベルは次の 4 段階です。`D` は「□詳細」がチェックされているときだけ出ます。

| 文字 | 意味 |
| --- | --- |
| `E` | エラー |
| `W` | 警告 |
| `I` | 情報 |
| `D` | 詳細 (内部の変換処理) |

区切りのあとに空白は入りません。`|` の直後からの空白の数が、そのまま入れ子の深さになります。

「□詳細」をチェックしたときに出る行は、行頭の印で種別が分かります。1 打鍵ごとに空行で区切られます。

| 印 | 意味 |
| --- | --- |
| `IN ` | 実際に入力された物理キー |
| `OUT` | NYamy が生成したキー |
| `FN ` | 実行した `⟨FUNCTION⟩` |
| `*  ` | 注記・状態 (`* Modifier Key` など) |

なお、[調査](#menu-i)や `FocusChanged` が出力する `TITLE:` の中の制御文字は、`<TAB>` / `<LF>` / `<CR>`、それ以外は `<XX>` (文字コードが 0xFF を超える場合は `<U+XXXX>`) と表記されます。この文字列は [`window`](#windowClass) のクラス名/タイトル名の照合対象そのものなので、制御文字にマッチさせている設定がある場合は書き換えが必要です (以前のバージョンではすべて `?` と表記されていました)。

設定ファイルの中からログへ出力する方法は [`log`](#dsl_log) を参照してください。

#### ログの保持文字数 (`logMaxSize`) {#logmaxsize}

ログが保持するテキスト量の上限は、既定で10万文字です。`nyamy.ini` の `logMaxSize` で変更できます。例えば20万文字にする場合は以下のようになります。

```ini
logMaxSize=200000
```

未指定・0 以下・数値として解釈できない値の場合は既定値が使われます。この設定は NYamy の起動時に読み込まれます (タスクトレイメニューの<span class="menu-item">Reload</span>では反映されません)。

この値は**起動時に一度だけ確保されるバッファの大きさ**です。NYamy は常駐するので、ログのためにメモリを断続的に確保・解放して断片化させることがないよう、領域は最初に確保したまま最後まで使い回します。1 文字あたり 2 バイトなので、既定の10万文字なら約 200KB です。

<span class="menu-item">ログ</span>ダイアログの編集欄は、このバッファの内容を**表示しているだけ**です。したがって上限を大きくしてもログ 1 行あたりのコストはほとんど変わらず、増えるのは確保されるメモリだけです。スクロールバックを増やしたい場合は遠慮なく大きくして構いません。

上限を超えた分は**古い行から順に、行単位で**捨てられます。先頭に切れかけの行が残ることはありません。

#### 設定ファイルの文字コード {#encoding}

設定ファイルの文字コードは UTF-8 (BOM の有無は不問) を推奨します。`.mayu` 形式については、UTF-16 および CP932 (Shift_JIS) のファイルも読み込むことができます。

### viii. 条件分岐 {#cond}

シンボルを定義して、そのシンボルによって条件分岐させることができます。

```mayu
define ⟨シンボル⟩

if ( ⟨シンボル⟩ )

〜

else

〜

endif
```

例えば次のように記述すると、

```mayu
if ( SwapAB )

key *A = *B

key *B = *A

endif
```

`SwapAB` というシンボルが `define` されている場合に、[[A]] と [[B]] を入れ替えます。

「[設定(<u>S</u>)...](#menu-s)」で `-Dシンボル名` を書くことでシンボルを定義することができます。

### ix. `⟨FUNCTION⟩` リファレンス {#function}

#### ピクセル数の単位 {#function_pixel_unit}

`⟨FUNCTION⟩` の引数に書くピクセル数は、拡大縮小 100% (96dpi) を基準とした値です。実際に動かす際には、対象のウィンドウ (カーソルを動かす場合はカーソル) が乗っているモニタの拡大率に応じて換算されます。

たとえば `&MouseMove(16, 0)` は、100% のモニタ上では 16 ピクセル、125% のモニタ上では 20 ピクセル動きます。どのモニタでも見た目の移動量が同じになるため、拡大率の異なるモニタを併用していても設定を書き分ける必要はありません。

#### `&CancelPrefix` {#function_CancelPrefix}

[`&Prefix`](#function_Prefix) によるプレフィックス状態を取り消します。2 ストロークキーの 1 ストローク目を押した後に、キャンセル用のキーに割り当てて使用します。

#### `&ClipboardChangeCase(⟨do_upcase⟩)` {#function_ClipboardChangeCase}

クリップボードの中身の文字を大文字化又は小文字化します。`⟨do_upcase⟩` に `true` を指定すると大文字化、`false` を指定すると小文字化します。[`&ClipboardUpcaseWord`](#function_ClipboardUpcaseDowncaseWord) / [`&ClipboardDowncaseWord`](#function_ClipboardUpcaseDowncaseWord) はそれぞれ `&ClipboardChangeCase(true)` / `&ClipboardChangeCase(false)` と同じです。

#### `&ClipboardCopy(⟨text⟩)` {#function_ClipboardCopy}

`⟨text⟩` [文字列](#string)をクリップボードへコピーします。

#### `&ClipboardUpcaseWord`, `&ClipboardDowncaseWord` {#function_ClipboardUpcaseDowncaseWord}

それぞれ、クリップボードの中身の文字を大文字化又は小文字化します。

#### `&Default` {#function_Default}

入力されたキーをそのまま Windows へ入力します。そのため、NYamyを起動してない時と同じ動作が期待できます。

#### `&DescribeBindings` {#function_DescribeBindings}

`&DescribeBindings` は、現在のキーマップでどのようなキー操作をするとどのような動作が起こるかを表示します。が、表示形式がいまいち分かりにくいのでどうしようか思案中。

#### `&DirectSSTP(/⟨name⟩/, ⟨protocol⟩ ⟨[⟩, ⟨header ...]⟩)` {#function_DirectSSTP}

[Direct](http://sakura.mikage.to/directsstp.html) [SSTP](http://sakura.mikage.to/sstp.html) プロトコルをしゃべります。

`⟨/name/⟩` にマッチする名前のゴーストへリクエストを Direct SSTP を使用して送ります。

`⟨protocol⟩` [文字列](#string) を省略すると `NOTIFY SSTP/1.1` になります。

`⟨header⟩` [文字列](#string) にカンマで区切ってヘッダを書き並べます。`Sender` ヘッダを省略するとNYamyの名前が挿入されます。`HWnd` ヘッダと `Charset` ヘッダはNYamyが適切に指定するので引数として指定してはいけません。

選択肢などを表示しても答えを受け取ることはできませんが、NYamyはゴーストから返事を 5 秒間待ちます。

例:

```mayu
key F12 = \

&DirectSSTP(/カレン/, \

"SEND SSTP/1.2", \

"Script: " \

"\\1こんにちわ" \

"\\_w[1000]\\0\\s3カレンのこと呼んだ？" \

"\\_w[1000]\\1＞みんな" \

"\\_w[1000]\\0\\s4\\n\\n……。" \

"\\e" ) \

&DirectSSTP(/双葉/, \

"SEND SSTP/1.2", \

"Sender: まゆ", \

"Script: " \

"\\_w[1000]\\0よばれてますよただきちさん。" \

"\\_w[1000]\\1きにするな。" \

"\\e" )
```

#### `&EditNextModifier(⟨モディファイヤ⟩)` {#function_EditNextModifier}

次にユーザーがキーを入力した時に、`⟨モディファイヤ⟩` が押されていることにします。例えば、

```mayu
key ESC = &EditNextModifier(M-)
```

とすると、[[Alt]] + [[X]] などを [[ESCAPE]] [[X]] などで代用することが可能になります。

#### `&EmacsEditKillLinePred`, `&EmacsEditKillLineFunc` {#function_EmacsEditKillLine}

エディットコントロールで emacs の kill-line のような機能を実現します。使い方は [`emacsedit.mayu.rb`](../emacsedit.mayu.rb) を参照のこと。

kill-line は非常にややこしい処理をしています。

まず、`C-k` の期待される動作は、

**(C-k-1)** カーソルが行末にある場合、クリップボードに改行を追加してテキストからは改行を削除する。

**(C-k-2)** カーソルが行末以外の場合、行末までをクリップボードに追加して行末までのテキストを削除。

です。NYamy での定義は、`.mayu` 形式で書くと以下のようになります。([`emacsedit.mayu.rb`](../emacsedit.mayu.rb) では同じ内容を Ruby DSL で記述しています)

```mayu
keyseq $EmacsEdit/kill-line = \

&EmacsEditKillLineFunc S-End C-X &Sync \

&EmacsEditKillLinePred((Delete), (Return Left))
```

こうなってるはずです。

[`&EmacsEditKillLineFunc`](#function_EmacsEditKillLine) は初回だけ、クリップボードの中身をクリアします。初回でない場合は、クリップボードの中身をNYamy内部に保存 **(※)** します。

その後 `S-End C-X` で行末までを選択し「切り取り」ます。ここで、クリップボードに行末までがコピーされたわけですが、クリップボードの中身には幾つか可能性があります。

EDIT コントロールの場合

**(EDIT-1)** カーソルが行末にあると、「」(からっぽ)

**(EDIT-2)** カーソルが行末以外だと、「行末までの文字列」

です。IE の中のエディットボックスの場合、

**(IE-1)** カーソルが行末にあると、「改行」

**(IE-2)** カーソルが行末以外だと、「行末までの文字列＋改行」

です。

[`&EmacsEditKillLinePred`](#function_EmacsEditKillLine) は、クリップボードの中身を調べて、

**(EDIT-1)** の場合は、<strong>※</strong>で保存したデータに「改行」を追加してクリップボードへ書き戻します。その後、第一引数、つまり `Delete` を実行します。

**(EDIT-2)** の場合は、<strong>※</strong>で保存したデータに「行末までの文字列」を追加してクリップボードへ書き戻します。

**(IE-1)** の場合は、<strong>※</strong>で保存したデータに「改行」を追加してクリップボードへ書き戻します。

**(IE-2)** の場合は、<strong>※</strong>で保存したデータに「行末までの文字列(改行は除く)」を追加してクリップボードへ書き戻します。その後、第二引数、つまり `Return Left` を実行します。

このように動作することで **(EDIT-1)** と **(IE-1)** は **(C-k-1)** 相当、**(EDIT-2)** と **(IE-2)** は **(C-k-2)** 相当になります。

#### `&ExecUserFunc(⟨関数名⟩, ⟨引数⟩ …)` {#function_ExecUserFunc}

[Ruby DSL の `deffunc`](#dsl_deffunc) で登録したユーザー定義関数を実行します。`⟨引数⟩` はそのまま Ruby のブロックへ渡されます。

```mayu
key C-F1 = &ExecUserFunc(NotifyTime)

key C-F2 = &ExecUserFunc(ShowMsg, "hello")
```

トリガーとなったキーやフォーカス中のウィンドウの情報も関数側へ引き継がれます。詳しくは[ユーザー定義関数](#dsl_deffunc)を参照してください。

#### `&HelpMessage(⟨title⟩, ⟨message⟩)` {#function_HelpMessage}

タスクトレイ付近にメッセージを表示します。`⟨title⟩` [文字列](#string) と `⟨message⟩` [文字列](#string) を省略すると、表示されているメッセージを消します。

#### `&HelpVariable(⟨title⟩)` {#function_HelpVariable}

タスクトレイ付近に [`&Variable`](#function_Variable) で設定された値が `⟨title⟩` [文字列](#string) と共に表示されます。

#### `&Ignore` {#function_Ignore}

なにも起こりません。

#### `&InvestigateCommand` {#function_InvestigateCommand}

ウィンドウへ送られてくる `WM_COMMAND` と `WM_SYSCOMMAND` を調べログに出力します。トグルになっていますので、調査が終わったらもう一度この `⟨FUNCTION⟩` を実行してください。さもないとアプリケーションの実行速度が遅くなる可能性があります。ログの出力は [`&PostMessage`](#function_PostMessage) で使用することが出来ます。

#### `&Keymap(⟨キーマップ名⟩)` {#function_Keymap}

別のキーマップのキーを指定します。例えば、

```mayu
keymap sub : Global

key	C-A = &WindowMinimize

window EditControl /:Edit$/ : Global

key	C-A = &Keymap(sub)
```

というように利用します。この場合、エディットコントロールで [[Control]] + [[A]] を入力すると、最小化されます。あまり実用的な機能はないかもしれません。ループしないように気をつけて利用してください。

#### `&KeymapParent` {#function_KeymapParent}

[親キーマップ](#parentKeymap)参照。

#### `&KeymapPrevPrefix` {#function_KeymapPrevPrefix}

現在のキーマップ (仮に `CURRENT` という名前とする) が二段階キーマップの場合、[`&Prefix`](#function_Prefix)`(CURRENT)` を実行したキーマップで定義されているキーを指定します。引数が無いと 1 段階前のキーマップになりますが、引数に数字を書くとその段階数前のキーマップになります。たとえば、

```mayu
keymap E

key A = &KeymapPrevPrefix(2)

keymap D

key X = &Prefix(E)

key A = D

keymap C

key X = &Prefix(D)

key A = C

key Y = &Prefix(E)

keymap B

key X = &Prefix(C)

key A = B

keymap Global

key X = &Prefix(B)

key A = A
```

ここで [[X]] [[X]] [[X]] [[X]] [[A]] と入力すると [[C]] が、[[X]] [[X]] [[Y]] [[A]] と入力すると [[B]] が入力されます。

#### `&KeymapWindow` {#function_KeymapWindow}

現在のウィンドウに定義されたキーマップのキーを入力します。プレフィックスキーの入力中に使用すると便利です。例えば

```mayu
keymap2 NotepadC-X

key	A = &KeymapWindow

window Notepad /:Notepad:Edit$/ : Global

key	C-X = &Prefix(NotepadC-X)

key	A = T E S T
```

この場合、メモ帳で [[Control]] + [[X]] を押した後に [[A]] を入力すると、`&KeymapWindow` は `Nodepad` キーマップに定義されているキーを入力しようとします。従って、`TEST` が入力されます。

#### `&LoadSetting(⟨設定名⟩)` {#function_LoadSetting}

設定ファイルを再読み込みします。`⟨設定名⟩` [文字列](#string) は「[設定(<u>S</u>)...](#menu-s)」で設定した「名前」で、再読み込みする設定を指定します。`⟨設定名⟩`を省略すると現在の設定を再読み込みします。

#### `&LogClear` {#function_LogClear}

[ログウインドウ](#menu-l)の内容を消去します。

#### `&MayuDialog(⟨dialog⟩, ⟨show_command⟩)` {#function_MayuDialog}

NYamyのダイアログボックスを表示したり隠したりします。`⟨dialog⟩` には `Investigate` と `Log` が指定できます。それぞれ「調査」ダイアログと「ログ」ダイアログです。`⟨show_command⟩` には、`HIDE`, `SHOW`, `SHOWNA` などが指定できます。

#### `&MouseHook(⟨type⟩, ⟨parameter⟩)` {#function_MouseHook}

マウスイベントをフックし `⟨type⟩` で指定されるアクションに変換します。 `⟨parameter⟩` は整数値で `⟨type⟩` 毎に意味が異なります。指定できる `⟨type⟩` は以下の通りです。

- `None` \[初期値\]変換しません。 `⟨parameter⟩` は無視されます。他の `⟨type⟩` から元に戻す場合にはこの値を指定します。
- `Wheel` マウスの垂直方向の移動をホイールの回転に変換します。この間マウスカーソルは動きません。 `⟨parameter⟩` は変換の際の倍率で、正と負では逆回転となります。
- `WindowMove` マウスの移動をウィンドウの移動に変換します。この間マウスカーソルは動きます。 `⟨parameter⟩` は移動するウィンドウの指定で、 `1` はアクティブウィンドウ、 `2` はマウスカーソル位置のウィンドウを意味します。また符号を負にすると対応する MDI 子ウィンドウを移動します。

#### `&MouseMove(⟨dx⟩, ⟨dy⟩)` {#function_MouseMove}

マウスカーソルを水平に `⟨dx⟩`、垂直に `⟨dy⟩` 移動します。単位は[ピクセル数の単位](#function_pixel_unit)を参照してください。

#### `&MouseWheel(⟨delta⟩)` {#function_MouseWheel}

ホイールを回します。`⟨delta⟩` を `-120` にするとホイールを手前に 1 単位まわしたことになります。逆に `120` にするとホイールを奥へ 1 単位まわしたことになります。

#### `&OtherWindowClass` {#function_OtherWindowClass}

[`window` に複数該当する場合](#matchManyClasses)参照。

#### `&PlugIn(⟨DLLNAME⟩, ⟨FUNCNAME⟩, ⟨FUNCPARAM⟩, ⟨runAsThread⟩)` {#function_PlugIn}

プラグインを実行します。`nyamy.exe` のあるディレクトリの中の `Plugins` というディレクトリにプラグイン DLL を置いておくとそのプラグインの中の関数を NYamy から直接呼ぶことが出来ます。(64bit 版の DLL が必要です。従来の 32bit 用プラグインはそのままでは動作しません)

`⟨DLLNAME⟩` はプラグイン DLL 名です。`Plugins\⟨DLLNAME⟩.dll` が使用されます。

`⟨FUNCNAME⟩` は DLL 中の関数名です。DLL は、以下の関数のうちのどれか実装していなければなりません。この引数は省略することができます。省略すると空文字列になります。

```mayu
void WINAPI mayu⟨FUNCNAME⟩W(const wchar_t *⟨FUNCPARAM⟩);

void WINAPI mayu⟨FUNCNAME⟩A(const char *⟨FUNCPARAM⟩);

void WINAPI mayu⟨FUNCNAME⟩(const char *⟨FUNCPARAM⟩);

void WINAPI ⟨FUNCNAME⟩(const char *⟨FUNCPARAM⟩);
```

`⟨FUNCPARAM⟩` は DLL の関数を呼び出すときに渡される引数です。省略すると空文字列 (`NULL` ではない) になります。

`⟨runAsThread⟩` に `true` を指定すると指定の関数をスレッドの中で実行します。省略すると `false` が指定されたことになります。

#### `&PostMessage(⟨window⟩, ⟨message⟩, ⟨wParam⟩, ⟨lParam⟩)` {#function_PostMessage}

ウィンドウへメッセージを送ることができます。高度な機能なので完全に理解してから利用してください。

```mayu
keyseq $WM_CUT = &PostMessage(ToItself, 0x0300, 0, 0)

window EditControl /:Edit$/ : Global

key C-W = $WM_CUT
```

と書くと、一部のウィンドウで [[Control]] + [[W]] でカットできるようになります。

`⟨window⟩` には、メッセージを送る先のウィンドウを指定します。以下の種類があります。

- `ToItself` はそのウィンドウ自身へ。
- `ToMainWindow` は最も親のウィンドウへ。
- `ToOverlappedWindow` は子でない最初のウィンドウへ。
- `ToParentWindow` は親ウィンドウへ。
- `⟨正の数⟩` は `1`:親ウィンドウ、`2`:親の親、`3`:親の親の親…

どのようなメッセージを送ればよいかは Spy++ などで調べられますが、`WM_COMMAND` と `WM_SYSCOMMAND` については [`&InvestigateCommand`](#function_InvestigateCommand) で調べることもできます。

#### `&Prefix(⟨キーマップ名⟩, ⟨ignore_modifiers⟩)` {#function_Prefix}

プレフィックスキーを指定します。例えば、

```mayu
keymap2 NotepadC-X

key	C-C = &WindowClose

window Notepad /:Notepad:Edit$/ : Global

key	C-X = &Prefix(NotepadC-X)
```

というように記述しておくと、メモ帳で [[Control]] + [[X]] [[Control]] + [[C]] と続けて入力するとメモ帳を終了することができます。

`⟨ignore_modifiers⟩` は省略可能な引数で `true` か `false` を指定します。省略すると `true` が指定されたとみなされます。`true` が指定された場合、`⟨キーマップ名⟩` で指示されるキーマップは

```mayu
mod !shift !alt !control !windows \

!mod0 !mod1 !mod2 !mod3 !mod4 \

!mod5 !mod6 !mod7 !mod8 !mod9
```

が指定されたものとして扱われます。つまり、全てのモディファイヤが[真のモディファイヤ](#trueModifier)として扱われるようになります。

[`keymap2`](#keymap2) を利用しているときには、デフォルトキーが [`&Undefined`](#function_Undefined) になっているので、モディファイヤを入力した時にもベルが鳴るはずですが、このように `true` を指定しておけば鳴らなくなります。([`mod`](#mod) を参照)

`false` を指定すれば、2 ストローク目にモディファイヤキーそのものを使用することができる可能性がありますが、通常はそのような使用方法はしないと思われます。また、さまざまな問題により `false` の指定にはバグがありますので使用はオススメしません。

> 1. キーの押す・離す、が順番に来ない場合 例えば [[Control]] + [[X]] [[Control]] + [[L]] という入力をユーザーがした場合、`D-C-X U-C-X D-C-L U-C-L` という順序で入力されるのが正しいのですが、[[X]] は左手、[[L]] は右手で入力するため、 `D-C-X D-C-L U-C-X U-C-L` という順序で入力されてしまうことがしばしばあります。 ですから、現在の実装では [`&Prefix`](#function_Prefix) はキーダウン (`D-`) 部分でしか正しく動作しないようになっています。入力されるキーの順序が入れ替わるため、`U-` 部分で [`&Prefix`](#function_Prefix) が動作してしまうとおかしなことになるからです。(現在は中途半端に動作しているので、バグかもしれません。要調査)
> 2. キーリピート キーリピートは、キーダウン (`D-`) がたくさん入力されたあとに、キーアップ (`U-`) が一度だけ入力されます。この場合に [`&Prefix`](#function_Prefix) がどのように動作すべきかは自明ではありません。
> 3. モディファイヤキーのキーマップ `false` を指定した時に [[Control]] + [[X]] [[F]] と入力したとします。`D-C-Control D-C-X U-C-X U-Control D-F U-F` このような順序でキーが入力されますが、`U-Control` はどのキーマップで解釈されるべきでしょうか？ 現在は、[`&Prefix`](#function_Prefix) 先のキーマップで解釈されていますが、モディファイヤを真のモディファイヤへ自動的に変換するため、`U-Control` は何の機能ももたないので、うまく動作しているように見えます。ですが、本来ならば元のキーマップで解釈されるべきなのでこれはバグなのですが、修正する予定はありません。

#### `&Recenter` {#function_Recenter}

エディットコントロールかリッチエディットコントロールでのみ動作し、カレットの位置を縦方向の中央に移動させます。

#### `&Repeat(⟨キーシーケンス⟩, ⟨最大回数⟩)` {#function_Repeat}

[`&Variable`](#function_Variable) で設定した回数だけ `⟨キーシーケンス⟩` を実行します。ただし、実行しすぎると危険なので*最大回数*を指定できます。*最大回数*は省略することができ、その場合 10 回が最大になります。

```mayu
key A = &Variable(0, 10) &Repeat((X))
```

上の例では、[[A]] を押すと、[[X]] が 10 回入力されます

#### `&SetForegroundWindow(⟨ウィンドウクラス名⟩, ⟨op⟩, ⟨ウィンドウタイトル名⟩)` {#function_SetForegroundWindow}

`⟨ウィンドウクラス名⟩` にマッチするウィンドウを前面に移動しフォーカスします。特定のアプリケーションをキー一発で呼び出す用途に使用します。

`⟨op⟩` と `⟨ウィンドウタイトル名⟩` は省略可能です。`⟨op⟩` に `&&` を指定するとクラス名とタイトル名の両方にマッチするウィンドウ、`||` を指定するとどちらか一方にマッチするウィンドウが対象になります。省略時は `&&` かつタイトル名 `/.*/` (任意) です。

```mayu
key C-S-N = &SetForegroundWindow(/notepad\.exe/)
```

#### `&SetImeStatus(⟨status⟩)` {#function_SetImeStatus}

IME の ON/OFF を切り替えます。`⟨status⟩` は `on`, `off`, `toggle` のいずれかで、省略時は `toggle` として扱われます。

#### `&SetImeString(⟨text⟩)` {#function_SetImeString}

IME を経由して `⟨text⟩` [文字列](#string) を入力します。

#### `&ShellExecute(⟨operation⟩, ⟨file⟩, ⟨parameters⟩, ⟨directory⟩, ⟨show_command⟩)` {#function_ShellExecute}

プログラムを実行します。

- `⟨operation⟩` [文字列](#string) にはファイルに対してどのような操作をするかを指示し、通常 `open` を指定します。
- `⟨file⟩` [文字列](#string) にはファイルか実行ファイルを書きます。
- `⟨parameters⟩` [文字列](#string) には `⟨file⟩` に実行ファイルを書いたときにオプションを記述します。
- `⟨directory⟩` [文字列](#string) は作業ディレクトリを指定します。
- `⟨show_command⟩` には `ShowNormal` を指定します。

コントロールパネルを開く例:

```mayu
key M-B = &ShellExecute("open", "C:\\WINDOWS\\system32\\Control.exe",,, ShowNormal)
```

システムのプロパティを開く例:

```mayu
key M-C = &ShellExecute("open", "C:/WINDOWS/system32/Control.exe", "sysdm.cpl",, ShowNormal)
```

既定のブラウザで Web ページを開く例:

```mayu
key M-H = &ShellExecute("open", "https://www.example.com/",,, ShowNormal)
```

##### キーリピートとの関係 {#function_ShellExecute_repeat}

キーを押しっぱなしにしても実行は 1 回だけです。オートリピートによる発火は無視されるので、アプリケーションがいくつも起動することはありません。

このため、[`R-`](#keyRepeat) を明示的に指定した `key R-M-B = &ShellExecute(...)` のような割り当ては一度も実行されません。

##### 引数の評価タイミング {#function_ShellExecute_eval}

`$Clipboard` などの[引数置換](#arg_subst)は、**キーを押した時点**で評価されます。プログラムの起動には時間がかかることがありますが、その間にクリップボードの内容やフォーカスが変わっても、キーを押した時点の値が使われます。

```mayu
key M-C-O = &ShellExecute("open", $Clipboard,,, ShowNormal)
```

#### `&Sync` {#function_Sync}

それまでに Windows へ送ったキー入力がアプリケーションに届くまで、NYamyのキー処理を中断します。

NYamyがキー入力を送る経路と、`⟨FUNCTION⟩` がアプリケーションを操作する経路は別なので、`⟨キーシーケンス⟩` に書いた順番どおりにアプリケーションが処理するとは限りません。`&Sync` を挟むと、`&Sync` より前の並びがアプリケーションに届いてから、後ろの並びが実行されます。

##### 動作 {#function_Sync_behavior}

`&Sync` は、キーを押したとき (`D-`) に次の順序で動作します。キーを離したとき (`U-`) は何もしません。

1. `&Sync` に指定された[モディファイヤ](#modifier)の状態になるように、モディファイヤキーの押し下げ / 解放を Windows へ送ります。
2. [`def sync`](#def_sync) で定義した`⟨スキャンコード⟩`を Windows へ送ります。
3. そのキーがフォーカスのあるウィンドウに届いたという連絡が返ってくるまで待ちます。それまでに送ったキー入力はこのキーより前に並んでいるので、待ち合わせが完了した時点でアプリケーションが処理し終えていることが保証されます。

待ち合わせは最大 5 秒で打ち切られます。`⟨スキャンコード⟩`の設定が不正だと連絡が返らず、この 5 秒間は何も入力できなくなります ([`def sync`](#def_sync) 参照)。また、フォーカスがコンソールウィンドウにある場合は待ち合わせを行わず、1. のモディファイヤキーの調整だけを行います。

##### 記述による効果の違い {#function_Sync_variations}

上記 1. のモディファイヤの調整があるため、`&Sync` は書き方によって効果が変わります。

- `&Sync` — モディファイヤを省略した場合は「すべて離されている」という指定になるため、[[Control]]、[[Alt]]、[[Shift]]、[[Windows]] をすべて離してから待ち合わせます。後続の `⟨FUNCTION⟩` は、どのモディファイヤも押されていない状態で実行されます。
- `C-&Sync` — 指定したモディファイヤ ([[Control]]) を押し、指定していないモディファイヤを離してから待ち合わせます。後続の `⟨FUNCTION⟩` は、[[Control]] だけが押された状態で実行されます。
- `*&Sync` — すべてのモディファイヤを `*` で無視すると、モディファイヤキーには一切手を触れず、待ち合わせだけを行います。順序の保証だけが欲しい場合はこの書き方を使います。
- `&Sync` を書かない — 待ち合わせもモディファイヤの調整も行われません。`⟨FUNCTION⟩` が実行される時点でアプリケーションがどこまでキー入力を処理し終えているか、モディファイヤキーをどう認識しているかは不定です。

##### 例: 順序を保証する {#function_Sync_example_order}

```mayu
key C-A = A &WindowMinimize
```

このように記述して [[Control]] + [[A]] を入力すると、Windows へ `A` が入力されるのが先か、`&WindowMinimize` が実行されるのが先かは不定です。順序だけを保証したい場合は `*&Sync` を挟みます。

```mayu
key C-A = A *&Sync &WindowMinimize
```

`*` を付けずに `&Sync` と書くと、順序の保証に加えて、押されている [[Control]] を離してから最小化することになります。

##### 例: モディファイヤの状態を確定させる {#function_Sync_example_modifier}

アプリケーションによっては、同じ操作でもモディファイヤキーが押されているかどうかで動作が変わります。`&Sync` のモディファイヤ指定を使うと、後続の `⟨FUNCTION⟩` が実行される瞬間の状態を選べます。

```mayu
window Explorer /Explorer\.exe/ : Global

key C-S-Z = &Sync &WindowMaximize # モディファイヤなしで最大化

key C-A-Z = C-&Sync &WindowMaximize # Control を押した状態で最大化
```

前者では、[[Control]] と [[Shift]] を離してから最大化のコマンドを送ります。後者では、[[Control]] を押したまま (かつ [[Alt]] を離して) 最大化のコマンドを送るので、アプリケーションが別の動作をします。

なお、`&Sync` 以外の `⟨FUNCTION⟩` にもモディファイヤを指定できますが、`D-` と `U-` ([キーを押す/離す](#keyUpDown)) を除いて効果はありません。モディファイヤの状態を制御するのは `&Sync` の役目です。

#### `&Toggle(Lock⟨N⟩)` {#function_Toggle}

ロックキーをトグルします。`&Toggle(Lock0)`〜`&Toggle(Lock9)` が利用できます。引数の最後に `on`, `off` を追加すると、ロックキーを強制的にオンにしたりオフにしたりできます。

#### `&Undefined` {#function_Undefined}

キーに何も割り当てられていないことにします。もしそのキーが押されると、ベルが鳴ります。

#### `&VK(⟨virtual_key⟩)` {#function_VK}

仮想キーを Windows へ入力します。仮想キーには、物理的なキーボードから入力できないキーも存在しますのでそのようなキーの入力に使用します。仮想キーを調べるには、タスクトレイメニュー[調査(I)...](#menu-i)の「仮想キーの調査」を利用します。例えば、

```mayu
key 変換 = &VK(F13)
```

と記述すると [[変換]] キーを押すと [[F13]] を入力できます。又、`E-` を付けると拡張キーを表し、`D-` はキーを押す、`U-` はキーを離すことを表します。

`⟨virtual_key⟩` に `LButton`、`MButton`、`RButton`、`XButton1`、`XButton2` を指定することによって、マウスのボタンをシミュレートすることができます。

この `⟨FUNCTION⟩` を利用するときは、必ず最後にキーを離していることを確認してください (つまり最後に `&VK(U-F13)` などを書いておく)。さもないと、そのキーが押されっぱなしになります。

#### `&Variable(⟨mag⟩, ⟨inc⟩)` {#function_Variable}

内部変数を `⟨mag⟩` 倍してから `⟨inc⟩` を加えます。この変数は、[`&Repeat`](#function_Repeat) と [`&HelpVariable`](#function_HelpVariable) で使用されます。

#### `&Wait(⟨milli_second⟩)` {#function_Wait}

`⟨milli_second⟩` ミリ秒だけ実行を中断します。その間はキーを入力することはできません。最大 5 秒待つことができます。

#### `&WindowClingToLeft`, `&WindowClingToRight`, `&WindowClingToTop`, `&WindowClingToBottom`, {#function_WindowCling}

ウィンドウを、それぞれの辺が画面のそれぞれの辺にくっつくように移動させます。引数に `MDI` を指定すると、MDI 子ウィンドウを操作します。

- `&WindowClingToTop` は [`&WindowMoveTo(N, 0, 0)`](#function_WindowMoveTo)と同じ。
- `&WindowClingToRight` は [`&WindowMoveTo(E, 0, 0)`](#function_WindowMoveTo) と同じ。
- `&WindowClingToLeft` は [`&WindowMoveTo(W, 0, 0)`](#function_WindowMoveTo) と同じ。
- `&WindowClingToBottom` は [`&WindowMoveTo(S, 0, 0)`](#function_WindowMoveTo) と同じ。

#### `&WindowClose` {#function_WindowClose}

ウィンドウを閉じます。引数に `MDI` を指定すると、MDI 子ウィンドウを操作します。

#### `&WindowIdentify` {#function_WindowIdentify}

ウィンドウのウィンドウクラス名とタイトルを調べログに出力します。又、各種ウィンドウの位置と大きさも出力します。

#### `&WindowMinimize`, `&WindowMaximize`, `&WindowHMaximize`, `&WindowVMaximize` {#function_WindowMaxMinHVMax}

それぞれ、ウィンドウを最小化、最大化、横方向に最大化、縦方向に最大化します。引数に `MDI` を指定すると、MDI 子ウィンドウを操作します。

#### `&WindowHVMaximize(⟨is_horizontal⟩)` {#function_WindowHVMaximize}

引数で方向を指定する形の横/縦最大化です。`⟨is_horizontal⟩` に `true` を指定すると横方向、`false` を指定すると縦方向に最大化します。[`&WindowHMaximize`](#function_WindowMaxMinHVMax) / [`&WindowVMaximize`](#function_WindowMaxMinHVMax) はそれぞれ `&WindowHVMaximize(true)` / `&WindowHVMaximize(false)` と同じです。横方向に最大化した状態で縦方向に最大化する (またはその逆) と、両方向に最大化された状態になります。引数の最後に `MDI` を追加指定すると、MDI 子ウィンドウを操作します。

#### `&WindowMonitor(⟨monitor⟩, ⟨adjust_position⟩, ⟨adjust_size⟩)` {#function_WindowMonitor}

ウィンドウをモニタ `⟨monitor⟩` へ移動します。[`&WindowMonitorTo(primary, ⟨monitor⟩, ⟨adjust_position⟩, ⟨adjust_size⟩)`](#function_WindowMonitorTo) と同じ動作をします。

#### `&WindowMonitorTo(⟨from⟩, ⟨monitor⟩, ⟨adjust_position⟩, ⟨adjust_size⟩)` {#function_WindowMonitorTo}

ウィンドウを `⟨from⟩` を基準としてモニタ `⟨monitor⟩` へ移動します。`⟨from⟩` には、次のものが指定できます。

- `primary` : プライマリモニタを基準とします
- `current` : 現在ウィンドウがあるモニタを基準とします

`⟨monitor⟩` には、数字が指定できます。`0` が基準となるモニタ、正の数は `1`: 次のモニタ、`2`: 次の次のモニタ…、負の数は `-1`: 前のモニタ、`-2`: 前の前のモニタ…、を意味します。

`⟨adjust_position⟩` は省略可能な引数で `true` か `false` を指定します。省略すると `true` が指定されたとみなされます。`true` が指定された場合、移動先がモニタからはみ出すときにできる限りモニタ内におさまる位置へ移動します。

`⟨adjust_size⟩` は省略可能な引数で `true` か `false` を指定します。省略すると `false` が指定されたとみなされます。`⟨adjust_position⟩` が `true` の場合のみ有効です。`true` が指定された場合、移動先がモニタからはみ出すときにモニタ内におさまるようウィンドウの大きさを調整します。

#### `&WindowMove(⟨dx⟩, ⟨dy⟩)` {#function_WindowMove}

ウインドウを水平方向に `⟨dx⟩`、垂直方向に `⟨dy⟩` 移動します。`MDI` を引数の最後に追加指定すると、MDI 子ウィンドウを操作します。`&WindowMoveTo(C, ⟨dx⟩, ⟨dy⟩)` と同じ動作をします。単位は[ピクセル数の単位](#function_pixel_unit)を参照してください。

#### `&WindowMoveTo(⟨gravity⟩, ⟨dx⟩, ⟨dy⟩)` {#function_WindowMoveTo}

基準位置から相対的にウインドウを水平方向に `⟨dx⟩`、垂直方向に `⟨dy⟩` 移動します。単位は[ピクセル数の単位](#function_pixel_unit)を参照してください。`MDI` を引数の最後に追加指定すると、MDI 子ウィンドウを操作します。`⟨gravity⟩` には、次のものが指定できます。

- `C` : 現在位置からの相対位置に移動します。
- `N` : 上下方向はデスクトップの上からの相対位置、左右方向は現在位置からの相対位置に移動します。
- `E` : 上下方向は現在位置からの相対位置、左右方向はデスクトップの右からの相対位置に移動します。
- `W` : 上下方向は現在位置からの相対位置、左右方向はデスクトップの左からの相対位置に移動します。
- `S` : 上下方向はデスクトップの下からの相対位置、左右方向は現在位置からの相対位置に移動します。
- `NE` : デスクトップ右上からの相対位置に移動。
- `NW` : デスクトップ左上からの相対位置に移動。
- `SE` : デスクトップ右下からの相対位置に移動。
- `SW` : デスクトップ左下からの相対位置に移動。

また、他の移動 `⟨FUNCTION⟩` とは以下のような関係があります。

- `&WindowMoveTo(C, ⟨dx⟩, ⟨dy⟩)` は [`&WindowMove(⟨dx⟩, ⟨dy⟩)`](#function_WindowMove) と同じ。
- `&WindowMoveTo(N, 0, 0)` は [`&WindowClingToTop`](#function_WindowCling) と同じ。
- `&WindowMoveTo(E, 0, 0)` は [`&WindowClingToRight`](#function_WindowCling) と同じ。
- `&WindowMoveTo(W, 0, 0)` は [`&WindowClingToLeft`](#function_WindowCling) と同じ。
- `&WindowMoveTo(S, 0, 0)` は [`&WindowClingToBottom`](#function_WindowCling) と同じ。

#### `&WindowMoveVisibly` {#function_WindowMoveVisibly}

ウィンドウ全体が画面に表示されるような位置へウィンドウを移動します。引数に `MDI` を指定すると、MDI 子ウィンドウを操作します。

#### `&WindowRedraw` {#function_WindowRedraw}

ウィンドウを強制的に再描画させます。

#### `&WindowResizeTo(⟨width⟩, ⟨height⟩)` {#function_WindowResizeTo}

ウインドウの大きさを幅 `⟨width⟩`、高さ `⟨height⟩` に変更します。`0` を指定すると現在の大きさに、負の値を指定するとデスクトップの大きさより指定したピクセル数だけ小さい大きさになります。単位は[ピクセル数の単位](#function_pixel_unit)を参照してください。`MDI` を引数の最後に追加指定すると、MDI 子ウィンドウを操作します。

#### `&WindowRaise`, `&WindowLower` {#function_WindowRiseLower}

それぞれ、ウィンドウを一番上、一番下へ移動します。引数に `MDI` を指定すると、MDI 子ウィンドウを操作します。

#### `&WindowSetAlpha(⟨alpha⟩)` {#function_WindowSetAlpha}

ウィンドウを半透明化、又は半透明化解除します。トグルになっています。`⟨alpha⟩` は半透明の度合いを表し、`0` で透明、`100` で不透明になります。`-1` を指定すると、この `⟨FUNCTION⟩` で半透明化されたウィンドウを全て不透明状態に戻します。

#### `&WindowToggleTopMost` {#function_WindowToggleTopMost}

ウィンドウの最前面フラグをトグルします。

#### 引数置換 {#arg_subst}

引数として `$` で始まるキーワードを指定することにより `⟨FUNCTION⟩` に以下の内容を渡すことができます。\[\]内はそのキーワードを指定できる引数の型です。値の取り出しは `⟨FUNCTION⟩` の実行時に行われます。ここでいう実行時とは、その `⟨FUNCTION⟩` が動きはじめる時点のことで、キーを押した時点と考えて構いません。時間のかかる処理を行う `⟨FUNCTION⟩` でも、処理中に値が変わったかどうかは結果に影響しません ([`&ShellExecute`](#function_ShellExecute_eval) の例を参照)。

- `$Clipboard`: \[文字列\] クリップボードの中身
- `$WindowClassName`: \[文字列\] フォーカスされているウィンドウのクラス名
- `$WindowTitleName`: \[文字列\] フォーカスされているウィンドウのタイトル名

`$` で始まる書き方は[キーシーケンス](#keyseq)の参照にも使いますが、どちらとして扱われるかは**その引数の型**で決まります。`⟨キーシーケンス⟩` を受け取る引数 ([`&Repeat`](#function_Repeat)、[`&EmacsEditKillLinePred`](#function_EmacsEditKillLine)) ではキーシーケンス名、[文字列](#string)を受け取る引数では上記のキーワードとして解釈されます。したがって `&Repeat($Clipboard, 3)` は `$Clipboard` という名前のキーシーケンスを探しますし、文字列の引数に上記以外の名前を書くとエラーになります。

クリップボード内の文字列を URL としてブラウザで開く例:

```mayu
key M-C-O = &ShellExecute("open", $Clipboard,,, ShowNormal)
```

フォーカスされているウィンドウのクラス名やタイトル名をクリップボードにコピーする例:

```mayu
key M-C-C = &ClipboardCopy($WindowClassName)

key M-C-T = &ClipboardCopy($WindowTitleName)
```

#### 文字列 {#string}

文字列が記述できる箇所には `"文字列"` と記述することができますが、`\` という文字は、その次にくる文字と組み合わせて特殊な文字を表します。

- `\a` (U+0007) ベル文字
- `\e` (U+001b) ESC 文字
- `\f` (U+000c) 改頁文字
- `\n` (U+000a) 改行文字
- `\r` (U+000d) 復帰文字
- `\t` (U+0009) タブ文字
- `\v` (U+000b) 垂直タブ文字
- `\'` 「`'`」
- `\"` 「`"`」
- `\\` 「`\`」
- `\c⟨X⟩` コントロール文字一般。`^⟨X⟩`
- `\x⟨XXXX⟩` (U+*XXXX*) 16 進数で表現した UNICODE 文字。*X* は 0〜9 と a〜f。
- `\0⟨XXXX⟩` 8 進数で表現した UNICODE 文字。*X* は 0〜7。
- 上記に当てはまらない `\⟨X⟩` は *X* という文字そのもの。

### x. ビルド {#compile}

#### 必要なもの {#compile_tool}

- **Visual Studio 2026** — 「C++ によるデスクトップ開発」ワークロードをインストールしてください。無償の Community エディションでビルドできます。
- **Ruby** — mruby のビルドに必要です。ネイティブ Windows 版 (RubyInstaller など) を PATH の通った場所にインストールしてください。
- **git** — ソースの取得とサブモジュールの展開に必要です。
- **Python 3** (任意) — マニュアル (この HTML) を Markdown ソースから再生成する場合のみ必要です。`pip install markdown` で Python-Markdown をインストールしてください。

#### ソースの取得 {#compile_source}

mruby を git サブモジュールとして参照しているため、`--recursive` を付けて clone します。

```mayu
git clone --recursive ⟨リポジトリURL⟩
```

既に clone 済みの場合は以下でサブモジュールを展開します。

```mayu
git submodule update --init
```

#### ビルド {#compile_build}

まず mruby をビルドします。リポジトリのルートで以下を実行すると、`scripter/mruby-dist/` にライブラリとヘッダが配置されます。

```mayu
powershell -ExecutionPolicy Bypass -File tools\build_mruby.ps1 Release
```

次に本体をビルドします。Visual Studio で `proj\nyamy.sln` を開いてビルドするか、開発者コマンドプロンプトで以下を実行します。

```mayu
msbuild proj\nyamy.sln -p:Configuration=Release
```

`Release\` ディレクトリに `nyamy.exe` などの実行ファイルが作成されます。Debug 構成の場合は `Debug\` に作成されます。

配布用 zip を作成するには以下を実行します。

```mayu
powershell -ExecutionPolicy Bypass -File tools\makedistrib.ps1
```

---

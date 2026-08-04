## 7. related work {#RELATEDWORK}

キー入力を横取り・変換するソフトウェアを NYamy と同時に使うと、互いの変換が干渉してわけのわからないことになりがちです。同種のソフトとの併用は避けてください。

現行の同種のソフトウェアとしては、以下のようなものがあります。

### [AutoHotkey](https://www.autohotkey.com/)

スクリプトを書くことでキーボードなどにさまざまな動作を割り当てることができます。キー変換にとどまらない自動化機能を持っています。

### [Microsoft PowerToys](https://learn.microsoft.com/ja-jp/windows/powertoys/) (Keyboard Manager)

Microsoft が提供するユーティリティ集で、GUI でキーの再マップとショートカットの再割り当てができます。

### レジストリ Scancode Map

Windows 標準の機能で、レジストリの Scancode Map によりキーを恒久的に入れ替えることができます (適用には再起動またはサインインし直しが必要)。ウィンドウ毎の切り替えなどはできませんが、ログイン画面にも効くのが利点です。NYamy は [`ScancodeMap`](#dsl_scancodemap) で設定内容を参照し、二重変換を避けることができます。

かつて「窓使いの憂鬱」と同時期には AltIME・猫まねき・Keylay など数多くのキーカスタマイズソフトが存在しましたが、その多くは開発・配布を終了しています。

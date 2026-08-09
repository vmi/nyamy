# dot.mayu.rb -- Ruby (mruby) version of dot.mayu
# Top-level configuration: select the keyboard layout from USE* symbols,
# then optionally load the default Emacs-like settings.

if symbol_defined?("USE104")
  load "104.mayu.rb"                          # 104 keyboard
  load "109on104.mayu.rb" if symbol_defined?("USE109on104")  # 104 keyboard as 109
else
  load "109.mayu.rb"                          # 109 keyboard
  load "104on109.mayu.rb" if symbol_defined?("USE104on109")  # 109 keyboard as 104
end

if symbol_defined?("USEdefault")
  load "default.mayu.rb"
end

keymap "Global"

# このファイルをホームディレクトリにコピーしてから、
# 以下に自分の好みのキーバインディングを設定するとよい。
# このファイル自体を変更しないこと。

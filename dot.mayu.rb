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

# このファイルを %LOCALAPPDATA%\NYamy\Config にコピーしてから、
# 以下に自分の好みのキーバインディングを設定するとよい。
# 必要に応じて任意の keymap/window ブロックを追加する。
# このファイル自体は変更しないこと。
keymap "Global" do
  # 例: key["C-A-S-L"] = "&MayuDialog(Log, SHOW)"
end

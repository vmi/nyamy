# workaround.mayu.rb -- Ruby (mruby) version of workaround.mayu
# Workaround for environments (e.g. remote desktop) where some keys
# arrive with an E0-prefixed scan code.

defkey "E0RightShift", "E0RShift", scan: "E0-0x36"
mod[:shift] += "E0RShift"
key["*E0RShift"] = "*LShift"

if symbol_defined?("KBD109")
  defkey "E0半角/全角", "E0漢字", "E0Kanji", scan: "E0-0x29"  # 半角/全角 漢字
  defkey "E0英数", "E0Eisuu",                scan: "E0-0x3a"  # 英数 CapsLock 漢字番号
  defkey "E0ひらがな", "E0Hiragana",         scan: "E0-0x70"  # ひらがな カタカナ ローマ字

  if symbol_defined?("KBD104on109")
    defsubst "~S-*E0半角/全角", to: "$GRAVE_ACCENT"
    defsubst "S-*E0半角/全角",  to: "$TILDE"
    defsubst "A-E0半角/全角",   to: "$ToggleIME"
    defsubst "*E0ひらがな",     to: "*Space"
    defsubst "*E0英数",         to: "S-*英数"
  end
end

keymap "Global" do
  if symbol_defined?("KBD109") && !symbol_defined?("KBD104on109")
    key["*E0半角/全角"] = "*Esc"  # Esc と半角/全角の入れ替え
  end
  if symbol_defined?("KBD109")
    mod[:control] += "E0英数"     # 英数を Control に
    key["*E0英数"] = "*LControl"  #	〃
  end
end

keymap "KeymapDefault", default: "&Default" do
  if symbol_defined?("KBD109")
    mod[:control] += "E0英数"     # 英数を Control に
    key["*E0英数"] = "*LControl"  #	〃
  end
end

keymap "Emacsen", parent: "Global" do
  if symbol_defined?("MAP-ESCAPE-TO-META")  # ESC が M- になるのを阻止する
    if symbol_defined?("KBD109") && !symbol_defined?("KBD104on109")
      key["*E0半角/全角"] = "*Esc"
    end
  end
end

keymap2 "EmacsMark" do
  if symbol_defined?("MAP-ESCAPE-TO-META")
    if symbol_defined?("KBD109") && !symbol_defined?("KBD104on109")
      key["E0半角/全角"] = "&Prefix(EmacsMarkEscape) &EditNextModifier(M-)"
    end
  end
end

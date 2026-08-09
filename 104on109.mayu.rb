# 104on109.mayu.rb -- Ruby (mruby) version of 104on109.mayu
# Map a 109 keyboard to behave like a 104 keyboard.

define "KBD104on109"

if symbol_defined? "SunType4"
  if !symbol_defined? "SCM-REMAP-ESC"
    defsubst "*Esc",         to: "*半角/全角"
    defsubst "*半角/全角",   to: "*Esc"
  end
else
  defsubst "~S-*半角/全角",    to: "$GRAVE_ACCENT"
  defsubst  "S-*半角/全角",    to: "$TILDE"
  defsubst  "A-半角/全角",     to: "$ToggleIME"
  defsubst "~S-*E0半角/全角",  to: "$GRAVE_ACCENT"
  defsubst  "S-*E0半角/全角",  to: "$TILDE"
  defsubst  "A-E0半角/全角",   to: "$ToggleIME"
end
defsubst "S-*_2",           to: "$COMMERCIAL_AT"
defsubst "S-*_6",           to: "$CIRCUMFLEX_ACCENT"
defsubst "S-*_7",           to: "$AMPERSAND"
defsubst "S-*_8",           to: "$ASTERISK"
defsubst "S-*_9",           to: "$LEFT_PARENTHESIS"
defsubst "S-*_0",           to: "$RIGHT_PARENTHESIS"
defsubst "S-*Hyphen",       to: "$LOW_LINE"
defsubst "~S-*Caret",       to: "$EQUALS_SIGN"
defsubst "S-*Caret",        to: "$PLUS_SIGN"
defsubst "~S-*Atmark",      to: "$LEFT_SQUARE_BRACKET"
defsubst "S-*Atmark",       to: "$LEFT_CURLY_BRACKET"
defsubst "~S-*OpenBracket", to: "$RIGHT_SQUARE_BRACKET"
defsubst "S-*OpenBracket",  to: "$RIGHT_CURLY_BRACKET"
if symbol_defined? "SunType4"
  defsubst "~S-*CloseBracket", to: "$GRAVE_ACCENT"
  defsubst "S-*CloseBracket",  to: "$TILDE"
else
  defsubst "~S-*CloseBracket", to: "$REVERSE_SOLIDUS"
  defsubst "S-*CloseBracket",  to: "$VERTICAL_LINE"
end
defsubst "S-*Semicolon",    to: "$COLON"
defsubst "~S-*Colon",       to: "$APOSTROPHE"
defsubst "S-*Colon",        to: "$QUOTATION_MARK"
defsubst "*ReverseSolidus", to: "*RightShift"

keymap "Global" do
    mod[:shift] += "ReverseSolidus"
end

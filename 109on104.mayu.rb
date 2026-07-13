# 109on104.mayu.rb -- Ruby (mruby) version of 109on104.mayu
# Map a 104 keyboard to behave like a 109 keyboard.

define "KBD109on104"

defsubst "S-*_2",           to: "$QUOTATION_MARK"
defsubst "S-*_6",           to: "$AMPERSAND"
defsubst "S-*_7",           to: "$APOSTROPHE"
defsubst "S-*_8",           to: "$LEFT_PARENTHESIS"
defsubst "S-*_9",           to: "$RIGHT_PARENTHESIS"
defsubst "S-*_0",           to: "$LOW_LINE"          # for lack of key
defsubst "S-*Hyphen",       to: "$EQUALS_SIGN"
defsubst "~S-*Equal",       to: "$CIRCUMFLEX_ACCENT"
defsubst "S-*Equal",        to: "$TILDE"
defsubst "~S-*OpenBracket", to: "$COMMERCIAL_AT"
defsubst "S-*OpenBracket",  to: "$GRAVE_ACCENT"
defsubst "*CloseBracket",   to: "$LEFT_SQUARE_BRACKET"
defsubst "S-*Semicolon",    to: "$PLUS_SIGN"
defsubst "~S-*Quote",       to: "$COLON"
defsubst "S-*Quote",        to: "$ASTERISK"

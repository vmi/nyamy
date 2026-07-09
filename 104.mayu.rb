# 104.mayu.rb -- Ruby (mruby) version of 104.mayu
# Auto-converted from 104.mayu following doc/scripter-design/mruby-binding.md.
# 101/102/104/105/107/108 US keyboard definition.

define "KBD101"
define "KBD102"
define "KBD104"
define "KBD105"
define "KBD107"
define "KBD108"

# key definitions
defkey "Drag",                          scan: "E1-0x00"
defkey "LButton",                       scan: "E1-0x01"
defkey "RButton",                       scan: "E1-0x02"
defkey "MButton",                       scan: "E1-0x03"
defkey "WheelForward",                  scan: "E1-0x04"
defkey "WheelBackward",                 scan: "E1-0x05"
defkey "XButton1",                      scan: "E1-0x06"
defkey "XButton2",                      scan: "E1-0x07"
defkey "TiltRight",                     scan: "E1-0x08"
defkey "TiltLeft",                      scan: "E1-0x09"
defkey "Esc", "Escape",                 scan: "0x01"
defkey "_1",                            scan: "0x02"
defkey "_2",                            scan: "0x03"
defkey "_3",                            scan: "0x04"
defkey "_4",                            scan: "0x05"
defkey "_5",                            scan: "0x06"
defkey "_6",                            scan: "0x07"
defkey "_7",                            scan: "0x08"
defkey "_8",                            scan: "0x09"
defkey "_9",                            scan: "0x0a"
defkey "_0",                            scan: "0x0b"
defkey "HyphenMinus", "Hyphen", "Minus", scan: "0x0c"
defkey "EqualsSign", "Equal",           scan: "0x0d"
defkey "BackSpace", "BS", "Back",       scan: "0x0e"
defkey "Tab",                           scan: "0x0f"
defkey "Q",                             scan: "0x10"
defkey "ScanPreviousTrack", "PreviousTrack", scan: "E0-0x10"
defkey "W",                             scan: "0x11"
defkey "E",                             scan: "0x12"
defkey "R",                             scan: "0x13"
defkey "T",                             scan: "0x14"
defkey "Y",                             scan: "0x15"
defkey "U",                             scan: "0x16"
defkey "I",                             scan: "0x17"
defkey "O",                             scan: "0x18"
defkey "P",                             scan: "0x19"
defkey "ScanNextTrack", "NextTrack",    scan: "E0-0x19"
defkey "LeftSquareBracket", "OpenBracket", scan: "0x1a"
defkey "RightSquareBracket", "CloseBracket", scan: "0x1b"
defkey "Enter", "Return",               scan: "0x1c"
defkey "NumEnter", "NumReturn",         scan: "E0-0x1c"
defkey "LeftControl", "LControl", "LCtrl", scan: "0x1d"
defkey "RightControl", "RControl", "RCtrl", scan: "E0-0x1d"
defkey "Pause",                         scan: ["E1-0x1d", "0x45"]
defkey "A",                             scan: "0x1e"
defkey "S",                             scan: "0x1f"
defkey "D",                             scan: "0x20"
defkey "Mute",                          scan: "E0-0x20"
defkey "F",                             scan: "0x21"
defkey "ALCalculator",                  scan: "E0-0x21"
defkey "G",                             scan: "0x22"
defkey "Play/Pause",                    scan: "E0-0x22"
defkey "H",                             scan: "0x23"
defkey "J",                             scan: "0x24"
defkey "Stop",                          scan: "E0-0x24"
defkey "K",                             scan: "0x25"
defkey "L",                             scan: "0x26"
defkey "Semicolon",                     scan: "0x27"
defkey "Apostrophe", "Quote",           scan: "0x28"
defkey "GraveAccent", "BackQuote",      scan: "0x29"
defkey "E0GraveAccent", "E0BackQuote",  scan: "E0-0x29"
defkey "LeftShift", "LShift",           scan: "0x2a"
defkey "ReverseSolidus", "BackSlash",   scan: "0x2b"
defkey "Z",                             scan: "0x2c"
defkey "X",                             scan: "0x2d"
defkey "C",                             scan: "0x2e"
defkey "VolumeDecrement", "VolumeDown", scan: "E0-0x2e"
defkey "V",                             scan: "0x2f"
defkey "B",                             scan: "0x30"
defkey "VolumeIncrement", "VolumeUp",   scan: "E0-0x30"
defkey "N",                             scan: "0x31"
defkey "M",                             scan: "0x32"
defkey "ACHome", "Internet",            scan: "E0-0x32"
defkey "Comma",                         scan: "0x33"
defkey "FullStop", "Period",            scan: "0x34"
defkey "Solidus", "Slash",              scan: "0x35"
defkey "NumSolidus", "NumSlash",        scan: "E0-0x35"
defkey "RightShift", "RShift",          scan: "0x36"
defkey "E0RightShift", "E0RShift",      scan: "E0-0x36"
defkey "NumAsterisk", "NumMultiply",    scan: "0x37"
defkey "PrintScreen", "Snapshot",       scan: "E0-0x37"
defkey "LeftAlt", "LAlt", "LMenu",      scan: "0x38"
defkey "RightAlt", "RAlt", "RMenu",     scan: "E0-0x38"
defkey "Space",                         scan: "0x39"
defkey "CapsLock", "Capital", "Caps",   scan: "0x3a"
defkey "E0CapsLock", "E0Capital", "E0Caps", scan: "E0-0x3a"
defkey "F1",                            scan: "0x3b"
defkey "F2",                            scan: "0x3c"
defkey "F3",                            scan: "0x3d"
defkey "F4",                            scan: "0x3e"
defkey "F5",                            scan: "0x3f"
defkey "F6",                            scan: "0x40"
defkey "F7",                            scan: "0x41"
defkey "F8",                            scan: "0x42"
defkey "F9",                            scan: "0x43"
defkey "F10",                           scan: "0x44"
defkey "NumLock",                       scan: "0x45"
defkey "ScrollLock", "Scroll",          scan: "0x46"
defkey "Break",                         scan: "E0-0x46"
defkey "Num7",                          scan: "0x47"
defkey "Home",                          scan: "E0-0x47"
defkey "Num8",                          scan: "0x48"
defkey "Up",                            scan: "E0-0x48"
defkey "Num9",                          scan: "0x49"
defkey "PageUp", "Prior",               scan: "E0-0x49"
defkey "NumHyphenMinus", "NumMinus",    scan: "0x4a"
defkey "Num4",                          scan: "0x4b"
defkey "Left",                          scan: "E0-0x4b"
defkey "Num5",                          scan: "0x4c"
defkey "Num6",                          scan: "0x4d"
defkey "Right",                         scan: "E0-0x4d"
defkey "NumPlusSign", "NumPlus",        scan: "0x4e"
defkey "Num1",                          scan: "0x4f"
defkey "End",                           scan: "E0-0x4f"
defkey "Num2",                          scan: "0x50"
defkey "Down",                          scan: "E0-0x50"
defkey "Num3",                          scan: "0x51"
defkey "PageDown", "Next",              scan: "E0-0x51"
defkey "Num0",                          scan: "0x52"
defkey "Insert",                        scan: "E0-0x52"
defkey "NumFullStop", "NumPeriod",      scan: "0x53"
defkey "Delete", "Del",                 scan: "E0-0x53"
defkey "SysRq",                         scan: "0x54"
defkey "Less",                          scan: "0x56"
defkey "F11",                           scan: "0x57"
defkey "F12",                           scan: "0x58"
defkey "LeftWindows", "LWindows", "LWin", scan: "E0-0x5b"
defkey "RightWindows", "RWindows", "RWin", scan: "E0-0x5c"
defkey "Applications", "Apps",          scan: "E0-0x5d"
defkey "PowerOff",                      scan: "E0-0x5e"
defkey "Sleep",                         scan: "E0-0x5f"
defkey "WakeUp",                        scan: "E0-0x63"
defkey "ACSearch",                      scan: "E0-0x65"
defkey "ACBookmarks",                   scan: "E0-0x66"
defkey "ACRefresh",                     scan: "E0-0x67"
defkey "ACStop",                        scan: "E0-0x68"
defkey "ACForward",                     scan: "E0-0x69"
defkey "ACBack",                        scan: "E0-0x6a"
defkey "ALLocalBrowser",                scan: "E0-0x6b"
defkey "ALEmailReader", "Email",        scan: "E0-0x6c"
defkey "ALConsumerControlConfiguration", scan: "E0-0x6d"

defsync "0x7e"

defmod "Shift",   keys: ["LShift", "RShift"]
defmod "Alt",     keys: ["LAlt", "RAlt"]
defmod "Control", keys: ["LControl", "RControl"]
defmod "Windows", keys: ["LWindows", "RWindows"]
mod[:shift] += "E0RShift"
key["*E0RShift"]      = "*LShift"
key["*E0CapsLock"]    = "*CapsLock"
key["*E0GraveAccent"] = "*GraveAccent"

# key sequence definitions
keyseq "ToggleIME",             "A-BackQuote"
keyseq "CapsLock",              "CapsLock"

keyseq "SPACE",                 "~S-*Space"               #
keyseq "EXCLAMATION_MARK",      "S-*_1"                   # !
keyseq "QUOTATION_MARK",        "S-*Apostrophe"           # "
keyseq "NUMBER_SIGN",           "S-*_3"                   # #
keyseq "DOLLAR_SIGN",           "S-*_4"                   # $
keyseq "PERCENT_SIGN",          "S-*_5"                   # %
keyseq "AMPERSAND",             "S-*_7"                   # &
keyseq "APOSTROPHE",            "~S-*Apostrophe"          # '
keyseq "LEFT_PARENTHESIS",      "S-*_9"                   # (
keyseq "RIGHT_PARENTHESIS",     "S-*_0"                   # )
keyseq "ASTERISK",              "S-*_8"                   # *
keyseq "PLUS_SIGN",             "S-*EqualsSign"           # +
keyseq "COMMA",                 "~S-*Comma"               # ,
keyseq "HYPHEN-MINUS",          "~S-*HyphenMinus"         # -
keyseq "FULL_STOP",             "~S-*FullStop"            # .
keyseq "SOLIDUS",               "~S-*Solidus"             # /
keyseq "DIGIT_ZERO",            "~S-*_0"                  # 0
keyseq "DIGIT_ONE",             "~S-*_1"                  # 1
keyseq "DIGIT_TWO",             "~S-*_2"                  # 2
keyseq "DIGIT_THREE",           "~S-*_3"                  # 3
keyseq "DIGIT_FOUR",            "~S-*_4"                  # 4
keyseq "DIGIT_FIVE",            "~S-*_5"                  # 5
keyseq "DIGIT_SIX",             "~S-*_6"                  # 6
keyseq "DIGIT_SEVEN",           "~S-*_7"                  # 7
keyseq "DIGIT_EIGHT",           "~S-*_8"                  # 8
keyseq "DIGIT_NINE",            "~S-*_9"                  # 9
keyseq "COLON",                 "S-*Semicolon"            # :
keyseq "SEMICOLON",             "~S-*Semicolon"           # ;
keyseq "LESS-THAN_SIGN",        "S-*Comma"                # <
keyseq "EQUALS_SIGN",           "~S-*EqualsSign"          # =
keyseq "GREATER-THAN_SIGN",     "S-*FullStop"             # >
keyseq "QUESTION_MARK",         "S-*Solidus"              # ?
keyseq "COMMERCIAL_AT",         "S-*_2"                   # @
keyseq "LATIN_CAPITAL_LETTER_A", "S-*A"
keyseq "LATIN_CAPITAL_LETTER_B", "S-*B"
keyseq "LATIN_CAPITAL_LETTER_C", "S-*C"
keyseq "LATIN_CAPITAL_LETTER_D", "S-*D"
keyseq "LATIN_CAPITAL_LETTER_E", "S-*E"
keyseq "LATIN_CAPITAL_LETTER_F", "S-*F"
keyseq "LATIN_CAPITAL_LETTER_G", "S-*G"
keyseq "LATIN_CAPITAL_LETTER_H", "S-*H"
keyseq "LATIN_CAPITAL_LETTER_I", "S-*I"
keyseq "LATIN_CAPITAL_LETTER_J", "S-*J"
keyseq "LATIN_CAPITAL_LETTER_K", "S-*K"
keyseq "LATIN_CAPITAL_LETTER_L", "S-*L"
keyseq "LATIN_CAPITAL_LETTER_M", "S-*M"
keyseq "LATIN_CAPITAL_LETTER_N", "S-*N"
keyseq "LATIN_CAPITAL_LETTER_O", "S-*O"
keyseq "LATIN_CAPITAL_LETTER_P", "S-*P"
keyseq "LATIN_CAPITAL_LETTER_Q", "S-*Q"
keyseq "LATIN_CAPITAL_LETTER_R", "S-*R"
keyseq "LATIN_CAPITAL_LETTER_S", "S-*S"
keyseq "LATIN_CAPITAL_LETTER_T", "S-*T"
keyseq "LATIN_CAPITAL_LETTER_U", "S-*U"
keyseq "LATIN_CAPITAL_LETTER_V", "S-*V"
keyseq "LATIN_CAPITAL_LETTER_W", "S-*W"
keyseq "LATIN_CAPITAL_LETTER_X", "S-*X"
keyseq "LATIN_CAPITAL_LETTER_Y", "S-*Y"
keyseq "LATIN_CAPITAL_LETTER_Z", "S-*Z"
keyseq "LEFT_SQUARE_BRACKET",   "~S-*LeftSquareBracket"   # [
keyseq "REVERSE_SOLIDUS",       "~S-*ReverseSolidus"      # \
keyseq "RIGHT_SQUARE_BRACKET",  "~S-*RightSquareBracket"  # ]
keyseq "CIRCUMFLEX_ACCENT",     "S-*_6"                   # ^
keyseq "LOW_LINE",              "S-*HyphenMinus"          # _
keyseq "GRAVE_ACCENT",          "~S-*GraveAccent"         # `
keyseq "LATIN_SMALL_LETTER_A",  "~S-*A"
keyseq "LATIN_SMALL_LETTER_B",  "~S-*B"
keyseq "LATIN_SMALL_LETTER_C",  "~S-*C"
keyseq "LATIN_SMALL_LETTER_D",  "~S-*D"
keyseq "LATIN_SMALL_LETTER_E",  "~S-*E"
keyseq "LATIN_SMALL_LETTER_F",  "~S-*F"
keyseq "LATIN_SMALL_LETTER_G",  "~S-*G"
keyseq "LATIN_SMALL_LETTER_H",  "~S-*H"
keyseq "LATIN_SMALL_LETTER_I",  "~S-*I"
keyseq "LATIN_SMALL_LETTER_J",  "~S-*J"
keyseq "LATIN_SMALL_LETTER_K",  "~S-*K"
keyseq "LATIN_SMALL_LETTER_L",  "~S-*L"
keyseq "LATIN_SMALL_LETTER_M",  "~S-*M"
keyseq "LATIN_SMALL_LETTER_N",  "~S-*N"
keyseq "LATIN_SMALL_LETTER_O",  "~S-*O"
keyseq "LATIN_SMALL_LETTER_P",  "~S-*P"
keyseq "LATIN_SMALL_LETTER_Q",  "~S-*Q"
keyseq "LATIN_SMALL_LETTER_R",  "~S-*R"
keyseq "LATIN_SMALL_LETTER_S",  "~S-*S"
keyseq "LATIN_SMALL_LETTER_T",  "~S-*T"
keyseq "LATIN_SMALL_LETTER_U",  "~S-*U"
keyseq "LATIN_SMALL_LETTER_V",  "~S-*V"
keyseq "LATIN_SMALL_LETTER_W",  "~S-*W"
keyseq "LATIN_SMALL_LETTER_X",  "~S-*X"
keyseq "LATIN_SMALL_LETTER_Y",  "~S-*Y"
keyseq "LATIN_SMALL_LETTER_Z",  "~S-*Z"
keyseq "LEFT_CURLY_BRACKET",    "S-*LeftSquareBracket"    # {
keyseq "VERTICAL_LINE",         "S-*ReverseSolidus"       # |
keyseq "RIGHT_CURLY_BRACKET",   "S-*RightSquareBracket"   # }
keyseq "TILDE",                 "S-*GraveAccent"          # ~

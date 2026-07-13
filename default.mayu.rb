# default.mayu.rb -- Ruby (mruby) version of default.mayu
# Emacs-like general configuration.

load "109.mayu.rb" unless symbol_defined?("KBD109") || symbol_defined?("KBD104")
if symbol_defined?("KBD104")
  defalias "↑",   as: "Up"
  defalias "↓",   as: "Down"
  defalias "←",   as: "Left"
  defalias "→",   as: "Right"
  defalias "Yen", as: "BackSlash"
end

# key sequences
keyseq "$WindowClose", "A-F4"

# Global keymap
keymap "Global" do
  key["*IC-C-Yen"]            = "$ToggleIME"
  key["C-S-M", "C-A-M"]       = "Applications"
  key["C-S-L", "C-A-L"]       = "&WindowLower"
  key["C-S-R", "C-A-R"]       = "&WindowRaise"
  key["C-S-Z", "C-A-Z"]       = "&WindowMaximize"
  key["C-S-I", "C-A-I"]       = "&WindowMinimize"
  key["C-S-X", "C-A-X"]       = "&WindowVMaximize"
  key["C-S-C", "C-A-C"]       = "&WindowHMaximize"
  key["C-S-Left", "C-A-Left"] = "&WindowMove(-16, 0)"
  key["C-S-Right", "C-A-Right"] = "&WindowMove(16, 0)"
  key["C-S-Up", "C-A-Up"]     = "&WindowMove(0, -16)"
  key["C-S-Down", "C-A-Down"] = "&WindowMove(0, 16)"
  key["C-S-A-Left"]           = "&WindowMove(-1, 0)"
  key["C-S-A-Right"]          = "&WindowMove(1, 0)"
  key["C-S-A-Up"]             = "&WindowMove(0, -1)"
  key["C-S-A-Down"]           = "&WindowMove(0, 1)"
  key["W-Left"]               = "&MouseMove(-16, 0)"
  key["W-Right"]              = "&MouseMove(16, 0)"
  key["W-Up"]                 = "&MouseMove(0, -16)"
  key["W-Down"]               = "&MouseMove(0, 16)"
  key["W-A-C-Left"]           = "&MouseMove(-1, 0)"
  key["W-A-C-Right"]          = "&MouseMove(1, 0)"
  key["W-A-C-Up"]             = "&MouseMove(0, -1)"
  key["W-A-C-Down"]           = "&MouseMove(0, 1)"
  key["C-A-A"]                = "&WindowClingToLeft"
  key["C-A-E"]                = "&WindowClingToRight"
  key["C-A-P"]                = "&WindowClingToTop"
  key["C-A-N"]                = "&WindowClingToBottom"
  key["C-A-V"]                = "&WindowMoveVisibly"
  key["C-S-K", "C-A-K"]       = "$WindowClose"
  key["C-S-T"]                = "&WindowToggleTopMost"
  key["C-S-D"]                = "&WindowIdentify &MayuDialog(Log, SHOW)"
  key["C-S-H"]                = "&WindowSetAlpha(70)"
  key["C-S-A-H"]              = "&WindowSetAlpha(-1)"
  key["C-S-U"]                = "&WindowRedraw"
  key["C-S-S"]                = '&LoadSetting &HelpMessage(Mayu, "再読込完了")'
  key["C-S-F1"]               = "&InvestigateCommand"
  unless symbol_defined?("EmacsMove/ShiftSelection")
    key["C-S-A", "C-S-B"] = "&WindowClingToLeft"
    key["C-S-F", "C-S-E"] = "&WindowClingToRight"
    key["C-S-P"]          = "&WindowClingToTop"
    key["C-S-N"]          = "&WindowClingToBottom"
    key["C-S-V"]          = "&WindowMoveVisibly"
  end

  if symbol_defined?("KBD109") && !symbol_defined?("KBD104on109")
    key["*半角/全角"]   = "*Esc"
    key["*E0半角/全角"] = "*Esc"
    key["*Esc"]         = "*半角/全角"
  end

  if symbol_defined?("KBD109")
    mod[:control] += "英数"
    key["*英数"]   = "*LControl"
    mod[:control] += "E0英数"
    key["*E0英数"] = "*LControl"
  else
    mod[:control] += "CapsLock"
    key["*CapsLock"] = "*LControl"
    mod[:control] += "E0CapsLock"
    key["*E0CapsLock"] = "*LControl"
  end

  if symbol_defined?("GANA")
    if symbol_defined?("KBD109")
      mod[:alt] += "!!無変換"
      mod[:alt] += "!!E0無変換"
      key["*無変換"]   = "*無変換"
      key["*E0無変換"] = "*無変換"
      key["A-無変換"]  = "無変換"
      key["A-E0無変換"] = "無変換"
      key["IC-A-K"]    = "無変換"
      key["*IC-変換"]  = "$ToggleIME"
    end
    key["*ScrollLock"] = "$CapsLock"
    key["C-↑"] = "W-↑"
    key["C-↓"] = "W-↓"
    key["C-←"] = "W-←"
    key["C-→"] = "W-→"
  end
end

keymap2 "GlobalEscape", parent: "Global", default: "&KeymapParent" do
  event["prefixed"]        = '&HelpMessage("Global", "ESC-")'
  event["before-key-down"] = "&HelpMessage"
  key["M-C-G"]             = "&Ignore"
end

keymap "Global" do
  if symbol_defined?("MAP-ESCAPE-TO-META")
    key["Escape"] = "&Prefix(GlobalEscape) &EditNextModifier(M-)"
    if symbol_defined?("KBD109") && !symbol_defined?("KBD104on109")
      key["半角/全角"] = "&Prefix(GlobalEscape) &EditNextModifier(M-)"
    end
  end
end

# almost-everything default keymap
keymap "KeymapDefault", default: "&Default" do
  if symbol_defined?("KBD109")
    mod[:control] += "英数"
    key["*英数"]   = "*LControl"
    mod[:control] += "E0英数"
    key["*E0英数"] = "*LControl"
  else
    mod[:control] += "CapsLock"
    key["*CapsLock"] = "*LControl"
    mod[:control] += "E0CapsLock"
    key["*E0CapsLock"] = "*LControl"
  end
  if symbol_defined?("GANA")
    if symbol_defined?("KBD109")
      mod[:alt] += "!!無変換"
      mod[:alt] += "!!E0無変換"
      key["*無変換"]   = "*無変換"
      key["*E0無変換"] = "*無変換"
      key["A-無変換"]  = "無変換"
      key["A-E0無変換"] = "無変換"
      key["IC-A-K"]    = "無変換"
    end
  end
end

# control settings
load "emacsedit.mayu.rb"

window "EditControl",   class: ':(Edit|TEdit|RichEdit(20[AW])?)$', parent: "EmacsEdit"
window "SysListView32", class: ':SysListView32$', parent: "EmacsMove"
window "SysTreeView32", class: ':SysTreeView32$', parent: "EmacsMove"
window "ComboBox",      class: ':ComboBox(:Edit)?$', parent: "EmacsEdit" do
  key["M-N", "M-P"] = "A-Down"
end

# general Windows settings
keyseq "$WM_VSCROLL/SB_PAGEUP",   "&PostMessage(ToItself, 277, 2, 0)"
keyseq "$WM_VSCROLL/SB_PAGEDOWN", "&PostMessage(ToItself, 277, 3, 0)"

keymap2 "GeneralC-X", parent: "EmacsC-X" do
  key["C-S"] = "C-S"
  key["C-W"] = "LAlt F A"
  key["C-F"] = "C-O"
  key["K"]   = "C-N"
  key["C-C"] = "LAlt F X"
end

# dialog box
window "DialogBox", class: ':#32770:', parent: "Global" do
  key["C-G"] = "Escape"
end

# MDI window operation
keymap2 "MDI-WindowOperation", parent: "Global" do
  key["C-S-L", "C-A-L"]       = "&WindowLower(MDI)"
  key["C-S-R", "C-A-R"]       = "&WindowRaise(MDI)"
  key["C-S-Z", "C-A-Z"]       = "&WindowMaximize(MDI)"
  key["C-S-I", "C-A-I"]       = "&WindowMinimize(MDI)"
  key["C-S-X", "C-A-X"]       = "&WindowVMaximize(MDI)"
  key["C-S-C", "C-A-C"]       = "&WindowHMaximize(MDI)"
  key["C-S-Left", "C-A-Left"] = "&WindowMove(-16, 0, MDI)"
  key["C-S-Right", "C-A-Right"] = "&WindowMove(16, 0, MDI)"
  key["C-S-Up", "C-A-Up"]     = "&WindowMove(0, -16, MDI)"
  key["C-S-Down", "C-A-Down"] = "&WindowMove(0, 16, MDI)"
  key["C-S-A-Left"]           = "&WindowMove(-1, 0, MDI)"
  key["C-S-A-Right"]          = "&WindowMove(1, 0, MDI)"
  key["C-S-A-Up"]             = "&WindowMove(0, -1, MDI)"
  key["C-S-A-Down"]           = "&WindowMove(0, 1, MDI)"
  key["C-S-A", "C-S-B", "C-A-A"] = "&WindowClingToLeft(MDI)"
  key["C-S-E", "C-S-F", "C-A-E"] = "&WindowClingToRight(MDI)"
  key["C-S-P", "C-A-P"]       = "&WindowClingToTop(MDI)"
  key["C-S-N", "C-A-N"]       = "&WindowClingToBottom(MDI)"
  key["C-S-V", "C-A-V"]       = "&WindowMoveVisibly(MDI)"
  key["C-S-K", "C-A-K"]       = "C-F4"
end

window "MDI", class: ':MDIClient:', parent: "Global" do
  key["C-S-Q", "C-A-Q"] = "&Prefix(MDI-WindowOperation)"
end

# Mado Tsukai no Yuutsu
window "MayuInvestigate", class: 'mayu\.exe:#32770:mayuFocus$', parent: "KeymapDefault"

window "MayuLog", class: 'mayu\.exe:#32770:Button', title: 'ログ - 窓使いの憂鬱', parent: "Global" do
  key["C-G"] = "$WindowClose"
  key["Esc"] = "$WindowClose"
end

# Console
keyseq "$ConsoleWindowClass/copy",       "&PostMessage(ToItself, 274, 65520, 0)"
keyseq "$ConsoleWindowClass/paste",      "&PostMessage(ToItself, 274, 65521, 0)"
keyseq "$ConsoleWindowClass/region",     "&PostMessage(ToItself, 274, 65522, 0)"
keyseq "$ConsoleWindowClass/scroll",     "&PostMessage(ToItself, 274, 65523, 0)"
keyseq "$ConsoleWindowClass/search",     "&PostMessage(ToItself, 274, 65524, 0)"
keyseq "$ConsoleWindowClass/select-all", "&PostMessage(ToItself, 274, 65525, 0)"

window "ConsoleWindowClass", class: '^ConsoleWindowClass$', parent: "Global" do
  key["C-S-K", "C-A-K"] = "A-Space C"
  key["S-Insert"]       = "$ConsoleWindowClass/paste"
  key["S-Prior"]        = "$WM_VSCROLL/SB_PAGEUP"
  key["S-Next"]         = "$WM_VSCROLL/SB_PAGEDOWN"
  key["S-~NL-Num9"]     = "$WM_VSCROLL/SB_PAGEUP"
  key["S-~NL-Num3"]     = "$WM_VSCROLL/SB_PAGEDOWN"
end

# Explorer, Internet Explorer
keyseq "$Explorer/show-folder-bar", "&PostMessage(ToMainWindow, 273, 41525, 0)"

window "ExplorerList", class: 'EXPLORER.*:SHELLDLL_DefView:.*SysListView32$', parent: "SysListView32" do
  key["S-R"]   = "F2"
  key["C-S-Z"] = "&Sync&WindowMaximize"
  key["C-A-Z"] = "C-&WindowMaximize"
  key["M-E"]   = "$Explorer/show-folder-bar"
end

window "ExplorerTree", class: 'EXPLORER.*:BaseBar:.*SysTreeView32$', parent: "SysTreeView32" do
  key["S-R"]   = "F2"
  key["C-S-Z"] = "&Sync&WindowMaximize"
  key["C-A-Z"] = "C-&WindowMaximize"
  key["M-E"]   = "$Explorer/show-folder-bar"
end

window "InternetExplorer", class: ':Internet Explorer_Server$', parent: "EmacsEdit" do
  key["C-S-Z"] = "&Sync&WindowMaximize"
  key["C-A-Z"] = "C-&WindowMaximize"
end

window "IEFrame", class: ':IEFrame', parent: "EmacsEdit"

window "MicrosoftJava", class: ':Microsoft VM For Java\(TM\) Host Window Class:', parent: "EmacsEdit"

# Emacs
keymap "Emacsen", parent: "Global" do
  key["C-Yen"] = "&Default"
  if symbol_defined?("MAP-ESCAPE-TO-META")
    if symbol_defined?("KBD109") && !symbol_defined?("KBD104on109")
      key["*半角/全角"]   = "*Esc"
      key["*E0半角/全角"] = "*Esc"
      key["*Esc"]         = "*半角/全角"
    else
      key["Escape"] = "&Default"
    end
  end
end

window "Meadow", class: ':Meadow$', parent: "Emacsen" do
  key["IC-M-X"] = "$ToggleIME M-X"
end
window "MULE", class: ':MULE$', parent: "Emacsen"
window "Emacs", class: ':Emacs$', parent: "Emacsen"

# Notepad
keyseq "$Notepad/new",     "&PostMessage(ToParentWindow, 273, 9, 0)"
keyseq "$Notepad/open",    "&PostMessage(ToParentWindow, 273, 10, 0)"
keyseq "$Notepad/save-as", "&PostMessage(ToParentWindow, 273, 1, 0)"

keymap2 "NotepadC-X", parent: "GeneralC-X" do
  event["prefixed"] = '&HelpMessage("メモ帳 C-x-", "C-x C-s\t上書き保存\r\n" "C-x C-f\t開く\t\r\n" "C-x k\t\t新規作成\r\n" "C-x C-c\t終了")'
  event["before-key-down"] = "&HelpMessage"
  key["C-S"] = "$Notepad/save-as"
  key["C-F"] = "$Notepad/open"
  key["K"]   = "$Notepad/new"
  key["C-C"] = "$WindowClose"
end

window "Notepad", class: ':Notepad:Edit$', parent: "EmacsEdit" do
  key["C-X"] = "&Prefix(NotepadC-X)" unless symbol_defined?("ZXCV")
  key["C-S"] = "F3"
  key["M-J"] = "C-G"
end

# ASTEC-X
keyseq "$ASTEC-X/copy-to-x", "&PostMessage(ToItself, 274, 16, 0)"

window "ASTEC-X", class: ':ASTEC-X$', parent: "Global" do
  key["C-Yen"]        = "&Default"
  key["*IC-IL-C-Yen"] = "$ToggleIME"
end

# Becky! Internet Mail
window "BeckyInternetMail", class: 'Rebecca\.exe:BeckyComposeFrameClass:', parent: "EmacsEdit" do
  key["C-X"] = "&Prefix(GeneralC-X)" unless symbol_defined?("ZXCV")
end

window "BeckyInternetMail2", class: 'B2\.exe:Becky2ComposeFrame:', parent: "EmacsEdit" do
  key["C-X"] = "&Prefix(GeneralC-X)" unless symbol_defined?("ZXCV")
end

# Microsoft PowerPoint
keymap2 "PowerPointC-X", parent: "GeneralC-X" do
  key["C-C"] = "$WindowClose"
end

window "PowerPoint", class: 'POWERPNT\.EXE:.*:(paneClassDC|REComboBox20W|RichEdit20W)$', parent: "EmacsEdit" do
  key["C-X"] = "&Prefix(PowerPointC-X)" unless symbol_defined?("ZXCV")
end

window "PowerPoint2", class: 'POWERPNT\.EXE:PP9FrameClass.*', parent: "EmacsEdit" do
  key["C-X"] = "&Prefix(PowerPointC-X)" unless symbol_defined?("ZXCV")
end

# Microsoft Visual Basic 6.0
window "VisualBasic", class: 'vb6\.exe:.*:VbaWindow$', parent: "EmacsEdit" do
  key["C-X"] = "&Prefix(GeneralC-X)" unless symbol_defined?("ZXCV")
end

# Microsoft Word
window "MicrosoftWord", class: 'WINWORD\.EXE:.*:_WwG$', parent: "EmacsEdit" do
  key["C-X"] = "&Prefix(GeneralC-X)" unless symbol_defined?("ZXCV")
end

# Microsoft Excel
window "MicrosoftExcel", class: 'EXCEL\.EXE:XLMAIN:', parent: "EmacsEdit" do
  key["C-X"] = "&Prefix(GeneralC-X)" unless symbol_defined?("ZXCV")
end

# Microsoft Pinball
window "MSPinball", class: 'PINBALL\.EXE:1c7c22a0-9576-11ce-bf80-444553540000$', parent: "Global" do
  key["A-Enter"] = "F4"
end

# Netscape Navigator
window "NetscapeNavigator", class: 'Netscape\.exe:', parent: "Global" do
  key["C-H"] = "BackSpace"
  key["C-S"] = "C-F"
end

# Mozilla
window "Mozilla", class: ':MozillaWindowClass$', parent: "EmacsEdit"

# Personal Dictionary
window "PersonalDictionary", class: 'PDICW32\.EXE:PDICW:ComboBox:Edit', parent: "EmacsEdit" do
  key["C-K"] = "S-End S-Delete"
  key["C-Y"] = "S-Insert"
end

# Real Player
window "RealPlayer", class: 'realplay.exe:PNGUIClass', parent: "Global" do
  key["A-Enter"] = "LAlt V Z F"
  key["C-R"]     = "C-P"
end

# TeraTerm
window "TeraTerm", class: 'TTermPRO\.exe:VTWin32$', parent: "Global" do
  key["C-Slash"]      = "C-S-HyphenMinus"
  key["S-Prior"]      = "C-Prior"
  key["S-Next"]       = "C-Next"
  key["IC-M-X", "IL-M-X"] = "$ToggleIME M-X"
  if symbol_defined?("KBD109")
    key["C-S-ReverseSolidus"] = "C-S-HyphenMinus"
  end
end

# Waffle
keyseq "$WaffleMark/cancel", "Left Right"

window "Waffle", class: 'WITALK2\.EXE:.*:RichEdit(20A)?$', parent: "Global"
keymap2 "WaffleMark", parent: "Waffle", default: "$WaffleMark/cancel &KeymapParent"

keymap "Waffle" do
  key["Home"]      = "&Default"
  key["End"]       = "&Default"
  key["C-Space"]   = "&Prefix(WaffleMark)"
  key["C-A"]       = "&Default"
  key["C-B"]       = "&Default"
  key["C-C"]       = "&Default"
  key["M-B"]       = "&Default"
  key["C-D"]       = "&Default"
  key["M-D"]       = "&Default"
  key["C-E"]       = "&Default"
  key["C-F"]       = "&Default"
  key["M-F"]       = "&Default"
  key["C-G"]       = "&Default"
  key["C-H"]       = "&Default"
  key["C-J"]       = "&Default"
  key["C-K"]       = "&Default"
  key["M-L"]       = "&Default"
  key["C-M"]       = "&Default"
  key["C-N"]       = "&Default"
  key["C-O"]       = "&Default"
  key["C-P"]       = "&Default"
  key["C-Q"]       = "&Prefix(KeymapDefault)"
  key["C-S"]       = "&Default"
  key["C-T"]       = "&Default"
  key["C-V"]       = "Next"
  key["M-V"]       = "&Default"
  key["C-W"]       = "&Default"
  key["M-W"]       = "&Default"
  key["C-Y"]       = "&Default"
  key["M-U"]       = "&Default"
  key["S-Home"]    = "&Default"
  key["S-End"]     = "&Default"
  key["S-M-Comma"] = "&Default"
  key["S-M-Period"] = "&Default"
  key["M-BackSpace"] = "&Default"
  key["C-Slash"]   = "&Default"
end

keymap2 "WaffleMark" do
  key["Home"]      = "S-C-Home  &Prefix(WaffleMark)"
  key["End"]       = "S-C-End   &Prefix(WaffleMark)"
  key["C-A"]       = "S-Home    &Prefix(WaffleMark)"
  key["C-B"]       = "S-Left    &Prefix(WaffleMark)"
  key["M-B"]       = "S-C-Left  &Prefix(WaffleMark)"
  key["C-E"]       = "S-End     &Prefix(WaffleMark)"
  key["C-F"]       = "S-Right   &Prefix(WaffleMark)"
  key["M-F"]       = "S-C-Right &Prefix(WaffleMark)"
  key["C-G"]       = "$WaffleMark/cancel &Undefined"
  key["C-N"]       = "S-Down    &Prefix(WaffleMark)"
  key["C-P"]       = "S-Up      &Prefix(WaffleMark)"
  key["C-V"]       = "S-Next    &Prefix(WaffleMark)"
  key["M-V"]       = "S-Prior   &Prefix(WaffleMark)"
  key["C-W"]       = "C-W Left Right"
  key["M-W"]       = "M-W Left Right"
  key["S-M-Comma"] = "S-C-Home  &Prefix(WaffleMark)"
  key["S-M-Period"] = "S-C-End  &Prefix(WaffleMark)"
  key["Left"]      = "S-Left    &Prefix(WaffleMark)"
  key["Up"]        = "S-Up      &Prefix(WaffleMark)"
  key["Right"]     = "S-Right   &Prefix(WaffleMark)"
  key["Down"]      = "S-Down    &Prefix(WaffleMark)"
end

# Xyzzy
window "Xyzzy", class: 'xyzzy\.exe:', parent: "Global" do
  key["C-S-K", "C-A-K"] = "C-X C-C"
end

# Windows Media Player
window "WindowsMediaPlayer", class: 'mplayer2.*:(Media Player 2|VideoRenderer)', parent: "Global" do
  key["C-A"] = "Space"
  key["C-R"] = "Space"
  key["C-P"] = "Space"
  key["C-S"] = "Period"
end

# Windows Mine Sweeper
window "WindowsMineSweeper", class: 'winmine.exe:マインスイーパ$', parent: "Global" do
  key["D-Z"]  = "&VK(RButton)"
  key["U-Z"]  = "&Ignore"
  key["D-X"]  = "&VK(MButton)"
  key["U-X"]  = "&Ignore"
  key["D-C"]  = "&VK(LButton)"
  key["U-C"]  = "&Ignore"
  key["Q"]    = "F2"
  key["Num1"] = "&MouseMove(-16, 16)"
  key["Num2"] = "&MouseMove(0, 16)"
  key["Num3"] = "&MouseMove(16, 16)"
  key["Num4"] = "&MouseMove(-16, 0)"
  key["Num6"] = "&MouseMove(16, 0)"
  key["Num7"] = "&MouseMove(-16, -16)"
  key["Num8"] = "&MouseMove(0, -16)"
  key["Num9"] = "&MouseMove(16, -16)"
end

# ICQ2000
if symbol_defined?("GANA")
  window "ICQMessageSession", class: 'ICQ\.exe:#32770:Edit$', title: 'Message Session', parent: "EmacsEdit" do
    key["Enter"] = "M-S"
  end
end

# Acrobat Reader
window "AcrobatReader", class: 'AcroRd32.exe:.*:MDIClient:', parent: "EmacsMove" do
  key["Space"] = "PageDown"
  key["BS"]    = "PageUp"
end

# Edmax
window "EdMax-edit", class: 'edmax\.exe:.*Afx:400000:b:0:1900010:0$', parent: "EmacsEdit"

# VisualBasic
window "VBTextBox", class: ':ThunderRT6FormDC:(ThunderRT6TextBox|RichTextWndClass)$', parent: "EmacsEdit"

# StarOffice/StarSuite/OpenOffice
window "StarOffice", class: 'soffice\.exe:SALFRAME$', parent: "EmacsEdit"

# Opera
window "Opera", class: 'Opera\.exe:', parent: "EmacsEdit"

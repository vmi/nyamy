# default.mayu.rb -- Ruby (mruby) version of default.mayu
# Emacs-like general configuration.

if !symbol_defined?("KBD109") && !symbol_defined?("KBD104")
  load "109.mayu.rb"
end

if symbol_defined?("KBD104")
  defalias "↑",  as: "Up"
  defalias "↓",  as: "Down"
  defalias "←",  as: "Left"
  defalias "→",  as: "Right"
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

  if symbol_defined?("KBD109") &&
     !symbol_defined?("KBD104on109") &&
     !symbol_defined?("SCM-REMAP-ESC")
    key["*半角/全角"]   = "*Esc"
    key["*E0半角/全角"] = "*Esc"
    key["*Esc"]         = "*半角/全角"
  end

  if !symbol_defined?("SCM-REMAP-LCTRL")
    if symbol_defined?("KBD109")
      if !nls_key?("英数")
        mod[:control] += "英数"
        key["*英数"]   = "*LControl"
      end
      mod[:control] += "E0英数"
      key["*E0英数"] = "*LControl"
    else
      if !nls_key?("CapsLock")
        mod[:control] += "CapsLock"
        key["*CapsLock"] = "*LControl"
      end
      mod[:control] += "E0CapsLock"
      key["*E0CapsLock"] = "*LControl"
    end
  end
end

keymap2 "GlobalEscape", parent: "Global", default: "&KeymapParent" do
  event["prefixed"]        = '&HelpMessage("Global", "ESC-")'
  event["before-key-down"] = "&HelpMessage"
  key["M-C-G"]             = "&Ignore"
end

keymap "Global" do
  if symbol_defined?("MAP-ESCAPE-TO-META")
    if symbol_defined?("KBD109") &&
       !symbol_defined?("KBD104on109") &&
       !symbol_defined?("SCM-REMAP-ESC")
      key["半角/全角"] = "&Prefix(GlobalEscape) &EditNextModifier(M-)"
    else
      key["Esc"] = "&Prefix(GlobalEscape) &EditNextModifier(M-)"
    end
  end
end

# almost-everything default keymap
keymap "KeymapDefault", default: "&Default" do
  if !symbol_defined?("SCM-REMAP-LCTRL")
    if symbol_defined?("KBD109")
      if !nls_key?("英数")
        mod[:control] += "英数"
        key["*英数"]   = "*LControl"
      end
      mod[:control] += "E0英数"
      key["*E0英数"] = "*LControl"
    else
      if !nls_key?("CapsLock")
        mod[:control] += "CapsLock"
        key["*CapsLock"] = "*LControl"
      end
      mod[:control] += "E0CapsLock"
      key["*E0CapsLock"] = "*LControl"
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

# # Mado Tsukai no Yuutsu
# window "MayuInvestigate", class: 'mayu\.exe:#32770:mayuFocus$', parent: "KeymapDefault"
# 
# window "MayuLog", class: 'mayu\.exe:#32770:Button', title: 'ログ - 窓使いの憂鬱', parent: "Global" do
#   key["C-G"] = "$WindowClose"
#   key["Esc"] = "$WindowClose"
# end

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

# Explorer (Windows11で機能するか要検証)
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

# Emacs
keymap "Emacsen", parent: "Global" do
  key["C-Yen"] = "&Default"
  if symbol_defined?("MAP-ESCAPE-TO-META")
    if symbol_defined?("KBD109") &&
       !symbol_defined?("KBD104on109") &&
       !symbol_defined?("SCM-REMAP-ESC")
      key["*半角/全角"]   = "*Esc"
      key["*E0半角/全角"] = "*Esc"
      key["*Esc"]         = "*半角/全角"
    else
      key["Esc"] = "&Default"
    end
  end
end

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

# Xyzzy
window "Xyzzy", class: 'xyzzy\.exe:', parent: "Global" do
  key["C-S-K", "C-A-K"] = "C-X C-C"
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

# Acrobat Reader
window "AcrobatReader", class: 'AcroRd32.exe:.*:MDIClient:', parent: "EmacsMove" do
  key["Space"] = "PageDown"
  key["BS"]    = "PageUp"
end

# VisualBasic
window "VBTextBox", class: ':ThunderRT6FormDC:(ThunderRT6TextBox|RichTextWndClass)$', parent: "EmacsEdit"

# StarOffice/StarSuite/OpenOffice
window "StarOffice", class: 'soffice\.exe:SALFRAME$', parent: "EmacsEdit"

# Opera
window "Opera", class: 'Opera\.exe:', parent: "EmacsEdit"

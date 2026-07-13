# emacsedit.mayu.rb -- Ruby (mruby) version of emacsedit.mayu
# Emacs-like movement / editing keymaps.

# Emacs-like movement commands
keymap "EmacsMove", parent: "Global" do
  key["Home"]      = "C-Home"
  key["End"]       = "C-End"
  key["C-Space"]   = "&Undefined"
  key["C-A"]       = "Home"
  key["C-B"]       = "Left"
  key["M-B"]       = "C-Left"
  key["C-E"]       = "End"
  key["C-F"]       = "Right"
  key["M-F"]       = "C-Right"
  key["C-G"]       = "Escape"
  key["C-L"]       = "&WindowRedraw &Recenter"
  key["C-N"]       = "Down"
  key["C-P"]       = "Up"
  key["C-Q"]       = "&Prefix(KeymapDefault)"
  key["C-S"]       = "C-F"
  key["C-V"]       = "Next" unless symbol_defined?("ZXCV")
  key["M-V"]       = "Prior"
  key["S-Home"]    = "S-C-Home"
  key["S-End"]     = "S-C-End"
  key["S-M-Comma"] = "C-Home"
  key["S-M-Period"] = "C-End"

  if symbol_defined?("EmacsMove/ShiftSelection")
    key["S-C-A"] = "S-Home"
    key["S-C-B"] = "S-Left"
    key["S-C-E"] = "S-End"
    key["S-C-F"] = "S-Right"
    key["S-C-N"] = "S-Down"
    key["S-C-P"] = "S-Up"
    key["S-C-V"] = "S-Next"
    key["S-M-V"] = "S-Prior"
  end
end

# Emacs-like editing command key sequences
keyseq "EmacsEdit/kill-word",          "S-C-Right C-X"
keyseq "EmacsEdit/backward-kill-word", "S-C-Left C-X"
keyseq "EmacsEdit/transpose-chars",    "S-Right C-X Left C-V Right"
keyseq "EmacsEdit/upcase-word",        "S-C-Right C-C *&Sync &ClipboardUpcaseWord C-V"
keyseq "EmacsEdit/downcase-word",      "S-C-Right C-C *&Sync &ClipboardDowncaseWord C-V"
keyseq "EmacsEdit/kill-line",          "&EmacsEditKillLineFunc S-End C-X &Sync &EmacsEditKillLinePred((Delete), (Return Left))"
keyseq "EmacsMark/cancel",             "Left Right"

keymap "EmacsEdit", parent: "EmacsMove"
keymap2 "EmacsMark", parent: "EmacsEdit", default: "$EmacsMark/cancel &KeymapWindow"
keymap2 "EmacsMarkEscape", parent: "EmacsMark", default: "&KeymapParent"

keymap2 "EmacsC-X", parent: "EmacsEdit" do
  event["prefixed"]        = '&HelpMessage("EmacsEdit C-x-", "C-x u\tundo")'
  event["before-key-down"] = "&HelpMessage"
  key["*U"] = "C-Z"
end

keymap2 "EmacsC-U0_9C-U", parent: "Global",
        default: "&Repeat((&KeymapWindow), 100) &HelpMessage &Variable(0, 0)" do
  key["C-G"] = "&HelpMessage &Variable(0, 0) &Ignore"
end

keymap2 "EmacsC-U0_9", parent: "EmacsC-U0_9C-U" do
  event["prefixed"] = '&HelpVariable("繰り返し")'
  key["_0", "Num0"] = "&Variable(10, 0) &Prefix(EmacsC-U0_9)"
  key["_1", "Num1"] = "&Variable(10, 1) &Prefix(EmacsC-U0_9)"
  key["_2", "Num2"] = "&Variable(10, 2) &Prefix(EmacsC-U0_9)"
  key["_3", "Num3"] = "&Variable(10, 3) &Prefix(EmacsC-U0_9)"
  key["_4", "Num4"] = "&Variable(10, 4) &Prefix(EmacsC-U0_9)"
  key["_5", "Num5"] = "&Variable(10, 5) &Prefix(EmacsC-U0_9)"
  key["_6", "Num6"] = "&Variable(10, 6) &Prefix(EmacsC-U0_9)"
  key["_7", "Num7"] = "&Variable(10, 7) &Prefix(EmacsC-U0_9)"
  key["_8", "Num8"] = "&Variable(10, 8) &Prefix(EmacsC-U0_9)"
  key["_9", "Num9"] = "&Variable(10, 9) &Prefix(EmacsC-U0_9)"
  key["C-U"]        = "&Prefix(EmacsC-U0_9C-U)"
end

keymap2 "EmacsC-U", parent: "EmacsC-U0_9C-U" do
  event["prefixed"] = '&HelpVariable("繰り返し")'
  key["_0", "Num0"] = "&Variable(0, 0) &Prefix(EmacsC-U0_9)"
  key["_1", "Num1"] = "&Variable(0, 1) &Prefix(EmacsC-U0_9)"
  key["_2", "Num2"] = "&Variable(0, 2) &Prefix(EmacsC-U0_9)"
  key["_3", "Num3"] = "&Variable(0, 3) &Prefix(EmacsC-U0_9)"
  key["_4", "Num4"] = "&Variable(0, 4) &Prefix(EmacsC-U0_9)"
  key["_5", "Num5"] = "&Variable(0, 5) &Prefix(EmacsC-U0_9)"
  key["_6", "Num6"] = "&Variable(0, 6) &Prefix(EmacsC-U0_9)"
  key["_7", "Num7"] = "&Variable(0, 7) &Prefix(EmacsC-U0_9)"
  key["_8", "Num8"] = "&Variable(0, 8) &Prefix(EmacsC-U0_9)"
  key["_9", "Num9"] = "&Variable(0, 9) &Prefix(EmacsC-U0_9)"
  key["C-U"]        = "&Variable(4, 0) &Prefix(EmacsC-U)"
end

keymap "EmacsEdit" do
  key["C-Space"]    = "&Prefix(EmacsMark)"
  key["M-BackSpace"] = "$EmacsEdit/backward-kill-word"
  key["C-D"]        = "Delete"
  key["M-D"]        = "$EmacsEdit/kill-word"
  key["C-H"]        = "BackSpace"
  key["C-J"]        = "Return"
  key["C-K"]        = "$EmacsEdit/kill-line"
  key["C-M"]        = "Return"
  key["C-O"]        = "Return Left"
  key["C-T"]        = "$EmacsEdit/transpose-chars" unless symbol_defined?("GANA")
  key["C-W"]        = "C-X"
  key["M-W"]        = "C-C"
  key["C-U"]        = "&Variable(0, 4) &Prefix(EmacsC-U)"
  key["C-X"]        = "&Prefix(EmacsC-X)" unless symbol_defined?("ZXCV")
  key["C-Y"]        = "C-V"
  key["C-Slash"]    = "C-Z"
  key["M-U"]        = "$EmacsEdit/upcase-word"
  key["M-L"]        = "$EmacsEdit/downcase-word"
end

keymap2 "EmacsMark" do
  key["Home"]       = "S-C-Home  &Prefix(EmacsMark)"
  key["End"]        = "S-C-End   &Prefix(EmacsMark)"
  key["C-A"]        = "S-Home    &Prefix(EmacsMark)"
  key["C-B"]        = "S-Left    &Prefix(EmacsMark)"
  key["M-B"]        = "S-C-Left  &Prefix(EmacsMark)"
  key["C-E"]        = "S-End     &Prefix(EmacsMark)"
  key["C-F"]        = "S-Right   &Prefix(EmacsMark)"
  key["M-F"]        = "S-C-Right &Prefix(EmacsMark)"
  key["C-G"]        = "$EmacsMark/cancel &Undefined"
  key["C-N"]        = "S-Down    &Prefix(EmacsMark)"
  key["C-P"]        = "S-Up      &Prefix(EmacsMark)"
  key["C-V"]        = "S-Next    &Prefix(EmacsMark)" unless symbol_defined?("ZXCV")
  key["M-V"]        = "S-Prior   &Prefix(EmacsMark)"
  key["C-W"]        = "C-X Left Right"
  key["M-W"]        = "C-C Left Right"
  key["S-M-Comma"]  = "S-C-Home  &Prefix(EmacsMark)"
  key["S-M-Period"] = "S-C-End   &Prefix(EmacsMark)"
  key["Left"]       = "S-Left    &Prefix(EmacsMark)"
  key["Up"]         = "S-Up      &Prefix(EmacsMark)"
  key["Right"]      = "S-Right   &Prefix(EmacsMark)"
  key["Down"]       = "S-Down    &Prefix(EmacsMark)"
  if symbol_defined?("MAP-ESCAPE-TO-META")
    key["Escape"] = "&Prefix(EmacsMarkEscape) &EditNextModifier(M-)"
    if symbol_defined?("KBD109") && !symbol_defined?("KBD104on109")
      key["半角/全角"]   = "&Prefix(EmacsMarkEscape) &EditNextModifier(M-)"
      key["E0半角/全角"] = "&Prefix(EmacsMarkEscape) &EditNextModifier(M-)"
    end
  end
end

keymap2 "EmacsMarkEscape" do
  event["prefixed"]        = '&HelpMessage("EmacsMark ESC-", " ")'
  event["before-key-down"] = "&HelpMessage"
  key["M-C-G"] = "&Ignore"
end

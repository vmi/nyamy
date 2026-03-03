# 型付き引数システム

> **状態: 未実装 (設計済み)**
> CallFunc / ExecFunc 機能 (protocol.md 参照) とともに実装する。

## 目的

- Setting 構築コマンド (DefKey, KeyAssign 等) と ExecFunc 引数を共通の型で扱う
- **yamy 側での文字列解析を排除**: DLL 側で完全に型変換し、yamy が受け取るのは型付きデータのみ
- FFI (Python ctypes / Ruby ffi / mruby) から使いやすいシンプルな型系

---

## ファイル: `scripter/yamy_arg.h` (将来新規作成)

```c
#pragma once
#include <stdint.h>

typedef enum {
    YAMY_ARG_NONE     = 0,
    YAMY_ARG_STRING   = 1,   // wchar_t* (null-terminated)
    YAMY_ARG_INT32    = 2,   // int32_t
    YAMY_ARG_UINT32   = 3,   // uint32_t
    YAMY_ARG_BOOL     = 4,   // uint8_t (0=false, 1=true)
    YAMY_ARG_UINT64   = 5,   // uint64_t (HWND 等の汎用 64bit 値)
    YAMY_ARG_KEYSEQ   = 6,   // uint32_t (keySeqIdx、yamy_def_key_seq_str の戻り値)
    YAMY_ARG_MODIFIER = 7,   // uint64_t × 2 (modifiers + dontcares)
} YamyArgType;

// 注: YAMY_ARG_HWND は不採用。現状の yamy 組み込み関数はすべて FunctionParam*
// 経由でウィンドウコンテキストを取得し、HWND を明示的な引数に取る関数が存在しない。
// HWND を渡す必要が生じた場合は YAMY_ARG_UINT64 を使用すること。

typedef struct {
    YamyArgType type;
    union {
        const wchar_t* str;
        int32_t        i32;
        uint32_t       u32;
        uint8_t        boolean;
        uint64_t       u64;    // UINT64 または HWND 等の汎用 64bit 値
        uint32_t       keySeqIdx;
        struct {
            uint64_t modifiers;
            uint64_t dontcares;
        } modifier;
    };
} YamyArg;
```

---

## yamy 側組み込み関数シグネチャ DB

### ファイル: `yamy_func_signatures.h` (将来新規作成、yamy + DLL 両方からインクルード)

X マクロ方式で定義。yamy 側の関数登録コードと DLL 側の検証テーブルの
両方を生成するために使用する。

```c
// YAMY_FUNC(id, name, n_args, arg_types...)
// ← arg_types はなし (0 個) の場合は省略

YAMY_FUNC(Windows_Close,    "Windows.Close",    0)
YAMY_FUNC(Windows_Maximize, "Windows.Maximize", 0)
YAMY_FUNC(Windows_Minimize, "Windows.Minimize", 0)
YAMY_FUNC(OSD_Display,      "OSD.Display",      1, YAMY_ARG_STRING)
YAMY_FUNC(Keyboard_Send,    "Keyboard.Send",    1, YAMY_ARG_STRING)
// ... 実際の内容は function.cpp の全登録関数から生成
```

### DLL 側での使用 (検証テーブル生成)

```cpp
struct FuncSig {
    const wchar_t* name;
    size_t n_args;
    YamyArgType arg_types[8];
};

static const FuncSig g_funcSigs[] = {
#define YAMY_FUNC(id, name, n, ...) \
    { L##name, n, { __VA_ARGS__ } },
#include "yamy_func_signatures.h"
#undef YAMY_FUNC
};
```

### yamy 側での使用 (関数登録)

```cpp
#define YAMY_FUNC(id, name, n, ...) \
    registerFunc(name, &FuncImpl_##id, { __VA_ARGS__ });
#include "yamy_func_signatures.h"
#undef YAMY_FUNC
```

---

## ExecFunc での型検証フロー

```
yscr_call_engine_func("OSD.Display", args, 1)
  ↓
1. g_funcSigs から "OSD.Display" を検索
2. n_args == 1、arg_types[0] == YAMY_ARG_STRING を確認
3. args[0] (YscrTypedVal) の型タグを検証し YAMY_ARG_STRING に変換
4. エラーなし → ExecFunc (0x30) としてデータパイプに書き込み
```

---

## FFI での YamyArg 構築例

### Python (ctypes)

```python
import ctypes

class YamyArgModifier(ctypes.Structure):
    _fields_ = [("modifiers", ctypes.c_uint64), ("dontcares", ctypes.c_uint64)]

class YamyArgValue(ctypes.Union):
    _fields_ = [
        ("str",       ctypes.c_wchar_p),
        ("i32",       ctypes.c_int32),
        ("u32",       ctypes.c_uint32),
        ("boolean",   ctypes.c_uint8),
        ("u64",       ctypes.c_uint64),
        ("keySeqIdx", ctypes.c_uint32),
        ("modifier",  YamyArgModifier),
    ]

class YamyArg(ctypes.Structure):
    _fields_ = [("type", ctypes.c_int), ("value", YamyArgValue)]

def make_str_arg(s: str) -> YamyArg:
    arg = YamyArg()
    arg.type = 1  # YAMY_ARG_STRING
    arg.value.str = s
    return arg

def make_uint64_arg(v: int) -> YamyArg:
    arg = YamyArg()
    arg.type = 5  # YAMY_ARG_UINT64
    arg.value.u64 = v
    return arg
```

### Ruby (ffi gem)

```ruby
module YamyScripter
  extend FFI::Library
  ffi_lib "yamy-scripter.dll"

  class YamyArgModifier < FFI::Struct
    layout :modifiers, :uint64, :dontcares, :uint64
  end

  class YamyArgValue < FFI::Union
    layout :str,       :pointer,
           :i32,       :int32,
           :u32,       :uint32,
           :boolean,   :uint8,
           :u64,       :uint64,
           :keySeqIdx, :uint32,
           :modifier,  YamyArgModifier
  end

  class YamyArg < FFI::Struct
    layout :type,  :int,
           :value, YamyArgValue
  end
end
```

---

## 検討事項・未決事項

- `YAMY_ARG_KEYSEQ` は DLL 内部で採番した keySeqIdx を参照する。
  ExecFunc の引数で keySeqIdx を渡す用途 (動的に定義したシーケンスを実行) が
  あるかどうかは要確認。
- 引数型の拡張 (例: YAMY_ARG_RECT、YAMY_ARG_COLOR) が必要になった場合は
  yamy_func_signatures.h の更新とともに追加する。
- `yamy_func_signatures.h` は function.cpp の手動メンテナンスが必要。
  将来的にはコード生成 (PowerShell / CMake スクリプト) の検討が望ましい。

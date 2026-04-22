# 型付き引数システム

## 概要

scripter と Engine 間で関数引数をやり取りする型システム。主な用途は
`on_exec_user_func` での引数読み取りで、Engine 側の `.mayu` で
`&ExecUserFunc("name", arg1, arg2, ...)` と書かれた引数を C API で受け取る。

---

## 型の種類: `YsType`

`YsType` は引数要素の型タグ。`yamy_scripter.h` に定義されている。

```c
typedef enum YsType {
    YsType_Error        = -1,  // エラー / 型取得失敗
    YsType_String       = 0,   // 文字列 (UTF-8)
    YsType_Number       = 1,   // 整数 (int32_t)
    YsType_Regexp       = 2,   // 正規表現 (UTF-8 パターン文字列)
    YsType_KeySeqIdx    = 3,   // キーシーケンスインデックス (uint32_t)
    YsType_ModifierSpec = 4,   // モディファイアビットマスク (uint64_t × 2)
    YsType_TokenSeq     = 5,   // トークン列 (YsStrs*)
} YsType;
```

`ys_func_args_get` / `ys_func_args_push` の value / length 解釈:

| YsType | value の解釈 | length の解釈 |
|--------|-------------|--------------|
| YsType_String | `const char*` (UTF-8, NUL終端) へのポインタ | バイト数 (NUL除く) |
| YsType_Number | `int32_t` を int64_t にキャスト | 未使用 (0) |
| YsType_Regexp | `const char*` (UTF-8, NUL終端) へのポインタ | バイト数 (NUL除く) |
| YsType_KeySeqIdx | `uint32_t` を int64_t にキャスト | 未使用 (0) |
| YsType_ModifierSpec | modifiers ビットマスク (uint64_t) | dontcares ビットマスク (uint64_t) |
| YsType_TokenSeq | `const YsStrs*` をポインタ→int64_t にキャスト | `ys_strs_length()` と同値 |

---

## `YsFuncArgs` / `YsStrs` — 不透明型

```c
// std::vector<YsFuncArg> に対するラッパー (不透明型; 内部型 YsFuncArg は ys_types.h に定義)
typedef struct YsFuncArgs YsFuncArgs;

// std::vector<std::string> (UTF-8) に対するラッパー (不透明型)
typedef struct YsStrs YsStrs;
```

内部構造には直接アクセスせず、必ず `ys_func_args_*` / `ys_strs_*` 関数経由で操作する。

---

## 引数の読み取り (`on_exec_user_func` 内)

Engine 側で `&ExecUserFunc("MyFunc", arg1, arg2, ...)` が実行されると、
CtrlStream 経由で `on_exec_user_func` が呼ばれる。
第 2 引数 `args` に Engine から送られた引数列が格納されている。

```c
void on_exec_user_func(const char* name, const YsFuncArgs* args)
{
    int n = ys_func_args_length(args);
    for (int i = 0; i < n; i++) {
        int64_t value = 0, length = 0;
        YsType t = ys_func_args_get(args, i, &value, &length);
        switch (t) {
        case YsType_String:
            // value = (uintptr_t)(const char*), length = byte count
            printf("string[%d]: %.*s\n", i, (int)length, (const char*)(uintptr_t)value);
            break;
        case YsType_Number:
            printf("number[%d]: %d\n", i, (int32_t)value);
            break;
        case YsType_KeySeqIdx:
            printf("keyseq_idx[%d]: %u\n", i, (uint32_t)value);
            break;
        case YsType_ModifierSpec:
            printf("mod[%d]: modifiers=0x%llx dontcares=0x%llx\n",
                   i, (unsigned long long)value, (unsigned long long)length);
            break;
        case YsType_TokenSeq: {
            const YsStrs* ss = (const YsStrs*)(uintptr_t)value;
            int sn = ys_strs_length(ss);
            for (int j = 0; j < sn; j++) {
                const char* sv; size_t sl;
                ys_strs_get(ss, j, &sv, &sl);
                printf("token[%d][%d]: %.*s\n", i, j, (int)sl, sv);
            }
            break;
        }
        default: break;
        }
    }
    // ys_exec_keyseq で yamy 組み込みアクションを呼び出せる
    ys_exec_keyseq("&SomeAction");
}
```

---

## 引数の構築 (`ys_func_args_push`)

`ys_func_args_push` を使うと C API 経由で `YsFuncArgs` を組み立てられる。
現状の主な用途は FFI/mruby から yamy 側に引数付き関数呼び出しをする場合など。

```c
YsFuncArgs* fas = ys_func_args_new();

// YsType_String: value = (int64_t)(uintptr_t)ptr, length = byte count
const char* s = "hello";
ys_func_args_push(fas, YsType_String, (int64_t)(uintptr_t)s, (int64_t)strlen(s));

// YsType_Number: value = int32_t
ys_func_args_push(fas, YsType_Number, (int64_t)(int32_t)42, 0);
```

---

## FFI からの使用例

`YsFuncArgs` / `YsStrs` は不透明型なので FFI 側で struct 定義は不要。
`ys_func_args_*` / `ys_strs_*` 関数を呼び出すだけでよい。

### Python (ctypes)

```python
import ctypes

lib = ctypes.CDLL("yamy-scripter.dll")

# 型設定
lib.ys_func_args_new.restype     = ctypes.c_void_p
lib.ys_strs_new.restype          = ctypes.c_void_p
lib.ys_func_args_length.restype  = ctypes.c_int
lib.ys_func_args_length.argtypes = [ctypes.c_void_p]
lib.ys_strs_length.restype       = ctypes.c_int
lib.ys_strs_length.argtypes      = [ctypes.c_void_p]
lib.ys_func_args_get.restype     = ctypes.c_int   # YsType
lib.ys_func_args_get.argtypes    = [ctypes.c_void_p, ctypes.c_int,
                                     ctypes.POINTER(ctypes.c_int64),
                                     ctypes.POINTER(ctypes.c_int64)]
lib.ys_strs_get.restype          = ctypes.c_bool
lib.ys_strs_get.argtypes         = [ctypes.c_void_p, ctypes.c_int,
                                     ctypes.POINTER(ctypes.c_char_p),
                                     ctypes.POINTER(ctypes.c_size_t)]
lib.ys_func_args_push.restype    = ctypes.c_bool
lib.ys_func_args_push.argtypes   = [ctypes.c_void_p, ctypes.c_int,
                                     ctypes.c_int64, ctypes.c_int64]
lib.ys_strs_push.restype         = ctypes.c_bool
lib.ys_strs_push.argtypes        = [ctypes.c_void_p, ctypes.c_char_p,
                                     ctypes.c_size_t]
lib.ys_exec_keyseq.restype       = ctypes.c_bool
lib.ys_exec_keyseq.argtypes      = [ctypes.c_char_p]

YsType_String     = 0
YsType_Number     = 1
YsType_Regexp     = 2
YsType_KeySeqIdx  = 3
YsType_ModifierSpec    = 4
YsType_TokenSeq   = 5

# --- 読み取り ---

def read_args(fas) -> list:
    result = []
    n = lib.ys_func_args_length(fas)
    for i in range(n):
        v, l = ctypes.c_int64(0), ctypes.c_int64(0)
        t = lib.ys_func_args_get(fas, i, ctypes.byref(v), ctypes.byref(l))
        if t == YsType_String or t == YsType_Regexp:
            s = ctypes.string_at(v.value, l.value).decode("utf-8")
            result.append({"type": "string" if t == YsType_String else "regexp", "value": s})
        elif t == YsType_Number:
            result.append({"type": "number", "value": ctypes.c_int32(v.value).value})
        elif t == YsType_KeySeqIdx:
            result.append({"type": "keyseq_idx", "value": v.value & 0xFFFFFFFF})
        elif t == YsType_ModifierSpec:
            result.append({"type": "mod",
                            "modifiers": v.value & 0xFFFFFFFFFFFFFFFF,
                            "dontcares": l.value & 0xFFFFFFFFFFFFFFFF})
        elif t == YsType_TokenSeq:
            ss = ctypes.c_void_p(v.value)
            sn = lib.ys_strs_length(ss)
            tokens = []
            for j in range(sn):
                sp = ctypes.c_char_p(); sl = ctypes.c_size_t()
                lib.ys_strs_get(ss, j, ctypes.byref(sp), ctypes.byref(sl))
                tokens.append(ctypes.string_at(sp, sl.value).decode("utf-8"))
            result.append({"type": "token_seq", "value": tokens})
    return result

# --- コールバック ---

# exeCtx は呼び出し元コンテキストポインタ (今回は使わないので無視)
LoadSettingFn  = ctypes.CFUNCTYPE(ctypes.c_bool, ctypes.c_void_p)
ExecUserFuncFn = ctypes.CFUNCTYPE(None,
    ctypes.c_void_p,  # exeCtx
    ctypes.c_char_p,  # func_name
    ctypes.c_void_p)  # args (YsFuncArgs*)

def on_load_setting(ctx):
    return bool(lib.ys_load_mayu())

def on_exec_user_func(ctx, name, fas):
    args = read_args(fas)
    print(f"called: {name.decode()}, args={args}")
    lib.ys_exec_keyseq(b"&SomeAction")

# YsCallbacks 構造体 (on_load_setting + on_quit)
class YsCallbacks(ctypes.Structure):
    _fields_ = [
        ("on_load_setting", LoadSettingFn),
        ("on_quit",         ctypes.c_void_p),  # NULL
    ]

callbacks = YsCallbacks(LoadSettingFn(on_load_setting), None)

lib.ys_start.restype  = ctypes.c_int
lib.ys_start.argtypes = [ctypes.POINTER(YsCallbacks), ctypes.c_void_p]
lib.ys_start(ctypes.byref(callbacks), None)
```

### Ruby (ffi gem)

```ruby
require 'ffi'

module YamyScripter
  extend FFI::Library
  ffi_lib "yamy-scripter.dll"

  YsType_String     = 0
  YsType_Number     = 1
  YsType_Regexp     = 2
  YsType_KeySeqIdx  = 3
  YsType_ModifierSpec    = 4
  YsType_TokenSeq   = 5

  attach_function :ys_func_args_new,    [],                             :pointer
  attach_function :ys_strs_new,         [],                             :pointer
  attach_function :ys_func_args_length, [:pointer],                     :int
  attach_function :ys_strs_length,      [:pointer],                     :int
  attach_function :ys_func_args_get,    [:pointer, :int,
                                         :pointer, :pointer],           :int
  attach_function :ys_strs_get,         [:pointer, :int,
                                         :pointer, :pointer],           :bool
  attach_function :ys_func_args_push,   [:pointer, :int,
                                         :int64, :int64],               :bool
  attach_function :ys_strs_push,        [:pointer, :string, :size_t],   :bool
  attach_function :ys_exec_keyseq,      [:string],                      :bool
  attach_function :ys_load_mayu,        [],                             :bool
  attach_function :ys_start,            [:pointer, :pointer],           :int

  # --- 読み取り ---

  def self.read_args(fas)
    n = ys_func_args_length(fas)
    n.times.map do |i|
      vp = FFI::MemoryPointer.new(:int64)
      lp = FFI::MemoryPointer.new(:int64)
      t  = ys_func_args_get(fas, i, vp, lp)
      v, l = vp.read_int64, lp.read_int64
      case t
      when YsType_String, YsType_Regexp
        { type: t == YsType_String ? :string : :regexp,
          value: FFI::Pointer.new(v).read_bytes(l).force_encoding("UTF-8") }
      when YsType_Number
        { type: :number, value: [v].pack("q<").unpack1("l<") }
      when YsType_KeySeqIdx
        { type: :keyseq_idx, value: v & 0xFFFFFFFF }
      when YsType_ModifierSpec
        { type: :mod, modifiers: v & 0xFFFFFFFF_FFFFFFFF,
                      dontcares: l & 0xFFFFFFFF_FFFFFFFF }
      when YsType_TokenSeq
        ss   = FFI::Pointer.new(v)
        sn   = ys_strs_length(ss)
        toks = sn.times.map do |j|
          sp = FFI::MemoryPointer.new(:pointer)
          sl = FFI::MemoryPointer.new(:size_t)
          ys_strs_get(ss, j, sp, sl)
          sp.read_pointer.read_bytes(sl.read(:size_t)).force_encoding("UTF-8")
        end
        { type: :token_seq, value: toks }
      else
        { type: :unknown, ys_type: t }
      end
    end
  end
end

# YsCallbacks 構造体 (on_load_setting + on_quit)
class YsCallbacks < FFI::Struct
  layout :on_load_setting, :pointer,
         :on_quit,         :pointer
end

on_load = FFI::Function.new(:bool, [:pointer]) { YamyScripter.ys_load_mayu }
callbacks = YsCallbacks.new
callbacks[:on_load_setting] = on_load
callbacks[:on_quit]         = FFI::Pointer::NULL
YamyScripter.ys_start(callbacks, FFI::Pointer::NULL)
```

---

## `ys_exec_keyseq` — yamy 組み込み関数の呼び出し

`on_exec_user_func` 内から yamy 組み込み関数 (`&OSD.Display` 等) を呼び出すには
`ys_exec_keyseq` を使う。アクション文字列は mayu 構文で記述する。

```c
void on_exec_user_func(void* exeCtx, const char* name, const YsFuncArgs* args)
{
    // "&関数名(引数)" の形で mayu 構文アクション文字列として指定
    ys_exec_keyseq("&OSD.Display(\"hello\")");
}
```

制約 (c-api.md 参照):
- `on_exec_user_func` 内でのみ有効 (`on_load_setting` 内では false を返す)
- `&ExecUserFunc` をアクション文字列に含めることは禁止 (無限ループ防止)

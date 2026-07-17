# 型付き引数システム

## 概要

scripter と Engine 間で関数引数をやり取りする型システム。主な用途は
`on_exec_user_func` での引数読み取りで、Engine 側の `.mayu` で
`&ExecUserFunc("name", arg1, arg2, ...)` と書かれた引数を C API で受け取る。

---

## 型の種類: `NYsType`

`NYsType` は引数要素の型タグ。`nyamy_scripter.h` に定義されている。

```c
typedef enum NYsType {
    NYsType_Error        = -1,  // エラー / 型取得失敗
    NYsType_String       = 0,   // 文字列 (UTF-8)
    NYsType_Number       = 1,   // 整数 (int32_t)
    NYsType_Regexp       = 2,   // 正規表現 (UTF-8 パターン文字列)
    NYsType_KeySeqIdx    = 3,   // キーシーケンスインデックス (uint32_t)
    NYsType_ModifierSpec = 4,   // モディファイアビットマスク (uint64_t × 2)
    NYsType_TokenSeq     = 5,   // トークン列 (NYsStrs*)
} NYsType;
```

`nys_func_args_get` / `nys_func_args_push` の value / length 解釈:

| NYsType | value の解釈 | length の解釈 |
|--------|-------------|--------------|
| NYsType_String | `const char*` (UTF-8, NUL終端) へのポインタ | バイト数 (NUL除く) |
| NYsType_Number | `int32_t` を int64_t にキャスト | 未使用 (0) |
| NYsType_Regexp | `const char*` (UTF-8, NUL終端) へのポインタ | バイト数 (NUL除く) |
| NYsType_KeySeqIdx | `uint32_t` を int64_t にキャスト | 未使用 (0) |
| NYsType_ModifierSpec | modifiers ビットマスク (uint64_t) | dontcares ビットマスク (uint64_t) |
| NYsType_TokenSeq | `const NYsStrs*` をポインタ→int64_t にキャスト | `nys_strs_length()` と同値 |

---

## `NYsFuncArgs` / `NYsStrs` — 不透明型

```c
// std::vector<NYsFuncArg> に対するラッパー (不透明型; 内部型 NYsFuncArg は nys_types.h に定義)
typedef struct NYsFuncArgs NYsFuncArgs;

// std::vector<std::string> (UTF-8) に対するラッパー (不透明型)
typedef struct NYsStrs NYsStrs;
```

内部構造には直接アクセスせず、必ず `nys_func_args_*` / `nys_strs_*` 関数経由で操作する。

---

## 引数の読み取り (`on_exec_user_func` 内)

Engine 側で `&ExecUserFunc("MyFunc", arg1, arg2, ...)` が実行されると、
CtrlStream 経由で `on_exec_user_func` が呼ばれる。
第 2 引数 `args` に Engine から送られた引数列が格納されている。

```c
void on_exec_user_func(const char* name, const NYsFuncArgs* args)
{
    int n = nys_func_args_length(args);
    for (int i = 0; i < n; i++) {
        int64_t value = 0, length = 0;
        NYsType t = nys_func_args_get(args, i, &value, &length);
        switch (t) {
        case NYsType_String:
            // value = (uintptr_t)(const char*), length = byte count
            printf("string[%d]: %.*s\n", i, (int)length, (const char*)(uintptr_t)value);
            break;
        case NYsType_Number:
            printf("number[%d]: %d\n", i, (int32_t)value);
            break;
        case NYsType_KeySeqIdx:
            printf("keyseq_idx[%d]: %u\n", i, (uint32_t)value);
            break;
        case NYsType_ModifierSpec:
            printf("mod[%d]: modifiers=0x%llx dontcares=0x%llx\n",
                   i, (unsigned long long)value, (unsigned long long)length);
            break;
        case NYsType_TokenSeq: {
            const NYsStrs* ss = (const NYsStrs*)(uintptr_t)value;
            int sn = nys_strs_length(ss);
            for (int j = 0; j < sn; j++) {
                const char* sv; size_t sl;
                nys_strs_get(ss, j, &sv, &sl);
                printf("token[%d][%d]: %.*s\n", i, j, (int)sl, sv);
            }
            break;
        }
        default: break;
        }
    }
    // nys_exec_keyseq で nyamy 組み込みアクションを呼び出せる
    nys_exec_keyseq("&SomeAction");
}
```

---

## 引数の構築 (`nys_func_args_push`)

`nys_func_args_push` を使うと C API 経由で `NYsFuncArgs` を組み立てられる。
現状の主な用途は FFI/mruby から nyamy 側に引数付き関数呼び出しをする場合など。

```c
NYsFuncArgs* fas = nys_func_args_new();

// NYsType_String: value = (int64_t)(uintptr_t)ptr, length = byte count
const char* s = "hello";
nys_func_args_push(fas, NYsType_String, (int64_t)(uintptr_t)s, (int64_t)strlen(s));

// NYsType_Number: value = int32_t
nys_func_args_push(fas, NYsType_Number, (int64_t)(int32_t)42, 0);
```

---

## FFI からの使用例

`NYsFuncArgs` / `NYsStrs` は不透明型なので FFI 側で struct 定義は不要。
`nys_func_args_*` / `nys_strs_*` 関数を呼び出すだけでよい。

### Python (ctypes)

```python
import ctypes

lib = ctypes.CDLL("nyamy-scripter.dll")

# 型設定
lib.nys_func_args_new.restype     = ctypes.c_void_p
lib.nys_strs_new.restype          = ctypes.c_void_p
lib.nys_func_args_length.restype  = ctypes.c_int
lib.nys_func_args_length.argtypes = [ctypes.c_void_p]
lib.nys_strs_length.restype       = ctypes.c_int
lib.nys_strs_length.argtypes      = [ctypes.c_void_p]
lib.nys_func_args_get.restype     = ctypes.c_int   # NYsType
lib.nys_func_args_get.argtypes    = [ctypes.c_void_p, ctypes.c_int,
                                     ctypes.POINTER(ctypes.c_int64),
                                     ctypes.POINTER(ctypes.c_int64)]
lib.nys_strs_get.restype          = ctypes.c_bool
lib.nys_strs_get.argtypes         = [ctypes.c_void_p, ctypes.c_int,
                                     ctypes.POINTER(ctypes.c_char_p),
                                     ctypes.POINTER(ctypes.c_size_t)]
lib.nys_func_args_push.restype    = ctypes.c_bool
lib.nys_func_args_push.argtypes   = [ctypes.c_void_p, ctypes.c_int,
                                     ctypes.c_int64, ctypes.c_int64]
lib.nys_strs_push.restype         = ctypes.c_bool
lib.nys_strs_push.argtypes        = [ctypes.c_void_p, ctypes.c_char_p,
                                     ctypes.c_size_t]
lib.nys_exec_keyseq.restype       = ctypes.c_bool
lib.nys_exec_keyseq.argtypes      = [ctypes.c_char_p]

NYsType_String     = 0
NYsType_Number     = 1
NYsType_Regexp     = 2
NYsType_KeySeqIdx  = 3
NYsType_ModifierSpec    = 4
NYsType_TokenSeq   = 5

# --- 読み取り ---

def read_args(fas) -> list:
    result = []
    n = lib.nys_func_args_length(fas)
    for i in range(n):
        v, l = ctypes.c_int64(0), ctypes.c_int64(0)
        t = lib.nys_func_args_get(fas, i, ctypes.byref(v), ctypes.byref(l))
        if t == NYsType_String or t == NYsType_Regexp:
            s = ctypes.string_at(v.value, l.value).decode("utf-8")
            result.append({"type": "string" if t == NYsType_String else "regexp", "value": s})
        elif t == NYsType_Number:
            result.append({"type": "number", "value": ctypes.c_int32(v.value).value})
        elif t == NYsType_KeySeqIdx:
            result.append({"type": "keyseq_idx", "value": v.value & 0xFFFFFFFF})
        elif t == NYsType_ModifierSpec:
            result.append({"type": "mod",
                            "modifiers": v.value & 0xFFFFFFFFFFFFFFFF,
                            "dontcares": l.value & 0xFFFFFFFFFFFFFFFF})
        elif t == NYsType_TokenSeq:
            ss = ctypes.c_void_p(v.value)
            sn = lib.nys_strs_length(ss)
            tokens = []
            for j in range(sn):
                sp = ctypes.c_char_p(); sl = ctypes.c_size_t()
                lib.nys_strs_get(ss, j, ctypes.byref(sp), ctypes.byref(sl))
                tokens.append(ctypes.string_at(sp, sl.value).decode("utf-8"))
            result.append({"type": "token_seq", "value": tokens})
    return result

# --- コールバック ---

# exeCtx は呼び出し元コンテキストポインタ (今回は使わないので無視)
LoadSettingFn  = ctypes.CFUNCTYPE(ctypes.c_bool, ctypes.c_void_p)
ExecUserFuncFn = ctypes.CFUNCTYPE(None,
    ctypes.c_void_p,  # exeCtx
    ctypes.c_char_p,  # func_name
    ctypes.c_void_p)  # args (NYsFuncArgs*)

def on_load_setting(ctx):
    return bool(lib.nys_load_mayu())

def on_exec_user_func(ctx, name, fas):
    args = read_args(fas)
    print(f"called: {name.decode()}, args={args}")
    lib.nys_exec_keyseq(b"&SomeAction")

# NYsCallbacks 構造体 (on_load_setting + on_quit)
class NYsCallbacks(ctypes.Structure):
    _fields_ = [
        ("on_load_setting", LoadSettingFn),
        ("on_quit",         ctypes.c_void_p),  # NULL
    ]

callbacks = NYsCallbacks(LoadSettingFn(on_load_setting), None)

lib.nys_start.restype  = ctypes.c_int
lib.nys_start.argtypes = [ctypes.POINTER(NYsCallbacks), ctypes.c_void_p]
lib.nys_start(ctypes.byref(callbacks), None)
```

### Ruby (ffi gem)

```ruby
require 'ffi'

module NYamyScripter
  extend FFI::Library
  ffi_lib "nyamy-scripter.dll"

  NYsType_String     = 0
  NYsType_Number     = 1
  NYsType_Regexp     = 2
  NYsType_KeySeqIdx  = 3
  NYsType_ModifierSpec    = 4
  NYsType_TokenSeq   = 5

  attach_function :nys_func_args_new,    [],                             :pointer
  attach_function :nys_strs_new,         [],                             :pointer
  attach_function :nys_func_args_length, [:pointer],                     :int
  attach_function :nys_strs_length,      [:pointer],                     :int
  attach_function :nys_func_args_get,    [:pointer, :int,
                                         :pointer, :pointer],           :int
  attach_function :nys_strs_get,         [:pointer, :int,
                                         :pointer, :pointer],           :bool
  attach_function :nys_func_args_push,   [:pointer, :int,
                                         :int64, :int64],               :bool
  attach_function :nys_strs_push,        [:pointer, :string, :size_t],   :bool
  attach_function :nys_exec_keyseq,      [:string],                      :bool
  attach_function :nys_load_mayu,        [],                             :bool
  attach_function :nys_start,            [:pointer, :pointer],           :int

  # --- 読み取り ---

  def self.read_args(fas)
    n = nys_func_args_length(fas)
    n.times.map do |i|
      vp = FFI::MemoryPointer.new(:int64)
      lp = FFI::MemoryPointer.new(:int64)
      t  = nys_func_args_get(fas, i, vp, lp)
      v, l = vp.read_int64, lp.read_int64
      case t
      when NYsType_String, NYsType_Regexp
        { type: t == NYsType_String ? :string : :regexp,
          value: FFI::Pointer.new(v).read_bytes(l).force_encoding("UTF-8") }
      when NYsType_Number
        { type: :number, value: [v].pack("q<").unpack1("l<") }
      when NYsType_KeySeqIdx
        { type: :keyseq_idx, value: v & 0xFFFFFFFF }
      when NYsType_ModifierSpec
        { type: :mod, modifiers: v & 0xFFFFFFFF_FFFFFFFF,
                      dontcares: l & 0xFFFFFFFF_FFFFFFFF }
      when NYsType_TokenSeq
        ss   = FFI::Pointer.new(v)
        sn   = nys_strs_length(ss)
        toks = sn.times.map do |j|
          sp = FFI::MemoryPointer.new(:pointer)
          sl = FFI::MemoryPointer.new(:size_t)
          nys_strs_get(ss, j, sp, sl)
          sp.read_pointer.read_bytes(sl.read(:size_t)).force_encoding("UTF-8")
        end
        { type: :token_seq, value: toks }
      else
        { type: :unknown, nys_type: t }
      end
    end
  end
end

# NYsCallbacks 構造体 (on_load_setting + on_quit)
class NYsCallbacks < FFI::Struct
  layout :on_load_setting, :pointer,
         :on_quit,         :pointer
end

on_load = FFI::Function.new(:bool, [:pointer]) { NYamyScripter.nys_load_mayu }
callbacks = NYsCallbacks.new
callbacks[:on_load_setting] = on_load
callbacks[:on_quit]         = FFI::Pointer::NULL
NYamyScripter.nys_start(callbacks, FFI::Pointer::NULL)
```

---

## `nys_exec_keyseq` — nyamy 組み込み関数の呼び出し

`on_exec_user_func` 内から nyamy 組み込み関数 (`&OSD.Display` 等) を呼び出すには
`nys_exec_keyseq` を使う。アクション文字列は mayu 構文で記述する。

```c
void on_exec_user_func(void* exeCtx, const char* name, const NYsFuncArgs* args)
{
    // "&関数名(引数)" の形で mayu 構文アクション文字列として指定
    nys_exec_keyseq("&OSD.Display(\"hello\")");
}
```

制約 (c-api.md 参照):
- `on_exec_user_func` 内でのみ有効 (`on_load_setting` 内では false を返す)
- `&ExecUserFunc` をアクション文字列に含めることは禁止 (無限ループ防止)

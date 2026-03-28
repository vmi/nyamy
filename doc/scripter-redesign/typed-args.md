# 型付き引数システム

## 概要

scripter と Engine 間で関数引数をやり取りする型システム。主な用途は 2 つ:

1. **`ys_reg_user_func` の preset_args**: ユーザー定義関数に事前引数を紐付けて登録し、
   Engine から関数が呼ばれたときに `on_exec_user_func` へ `YsFuncArgs*` として渡す。
2. **`on_exec_user_func` での引数読み取り**: Engine 側の `.mayu` で
   `&ExecUserFunc("name", arg1, arg2, ...)` と書かれた引数を C API で受け取る。

---

## 型の種類: `YsType`

`YsType` は内部 C++ 型 `FuncArg` バリアントに対応する C API の列挙型。

```c
typedef enum YsType {
    YT_ERROR      = -1,  // エラー / 型取得失敗
    YT_STRING     = 0,   // 文字列 (UTF-8)
    YT_NUMBER     = 1,   // 整数 (int32_t)
    YT_REGEXP     = 2,   // 正規表現 (UTF-8)
    YT_KEYSEQ_IDX = 3,   // キーシーケンスインデックス (uint32_t)
    YT_MOD        = 4,   // モディファイアビットマスク (uint64_t × 2)
    YT_TOKEN_SEQ  = 5,   // トークン列 (YsStrs*)
} YsType;
```

`ys_func_args_get` / `ys_func_args_push` の value / length 解釈:

| YsType | value の解釈 | length の解釈 |
|--------|-------------|--------------|
| YT_STRING | `const char*` (UTF-8, NUL終端) へのポインタ | バイト数 (NUL除く) |
| YT_NUMBER | `int32_t` を int64_t にキャスト | 未使用 (0) |
| YT_REGEXP | `const char*` (UTF-8, NUL終端) へのポインタ | バイト数 (NUL除く) |
| YT_KEYSEQ_IDX | `uint32_t` を int64_t にキャスト | 未使用 (0) |
| YT_MOD | modifiers ビットマスク (uint64_t) | dontcares ビットマスク (uint64_t) |
| YT_TOKEN_SEQ | `const YsStrs*` をポインタ→int64_t にキャスト | `ys_strs_length()` と同値 |

---

## `YsFuncArgs` / `YsStrs` — 不透明型

```c
// std::vector<FuncArg> に対するラッパー (不透明型)
typedef struct YsFuncArgs YsFuncArgs;

// std::vector<std::wstring> に対するラッパー (不透明型)
typedef struct YsStrs YsStrs;
```

内部構造には直接アクセスせず、必ず `ys_func_args_*` / `ys_strs_*` 関数経由で操作する。

---

## 引数の読み取り (`on_exec_user_func` 内)

```c
void on_exec_user_func(const char* name,
                        const YsFuncArgs* preset_args,
                        const YsTriggerInfo* trigger_info)
{
    int n = ys_func_args_length(preset_args);
    for (int i = 0; i < n; i++) {
        int64_t value = 0, length = 0;
        YsType t = ys_func_args_get(preset_args, i, &value, &length);
        switch (t) {
        case YT_STRING:
            // value = (uintptr_t)(const char*), length = byte count
            printf("string[%d]: %.*s\n", i, (int)length, (const char*)(uintptr_t)value);
            break;
        case YT_NUMBER:
            printf("number[%d]: %d\n", i, (int32_t)value);
            break;
        case YT_KEYSEQ_IDX:
            printf("keyseq_idx[%d]: %u\n", i, (uint32_t)value);
            break;
        case YT_MOD:
            printf("mod[%d]: modifiers=0x%llx dontcares=0x%llx\n",
                   i, (unsigned long long)value, (unsigned long long)length);
            break;
        case YT_TOKEN_SEQ: {
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
    // trigger_info は ys_exec_keyseq に再利用できる
    ys_exec_keyseq("&SomeAction", trigger_info);
}
```

---

## 引数の構築 (`ys_reg_user_func` の preset_args)

`ys_func_args_new()` で作成し、`ys_func_args_push()` で要素を追加する。
`ys_reg_user_func` 呼び出し後は関数内でコピーされるため、`fas` の寿命は
`on_load_setting` 終了まで保持すれば十分。

```c
static bool on_load_setting(void)
{
    // "MyFunc" を文字列 "hello" と数値 42 の preset_args で登録
    YsFuncArgs* fas = ys_func_args_new();

    // YT_STRING: value = (int64_t)(uintptr_t)ptr, length = byte count
    const char* s = "hello";
    ys_func_args_push(fas, YT_STRING, (int64_t)(uintptr_t)s, (int64_t)strlen(s));

    // YT_NUMBER: value = int32_t
    ys_func_args_push(fas, YT_NUMBER, (int64_t)(int32_t)42, 0);

    ys_reg_user_func("MyFunc", fas);
    // fas は on_load_setting 終了まで生存していれば OK
    // (関数内でコピーされる)

    return ys_load_mayu();
}
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

YT_STRING = 0; YT_NUMBER = 1; YT_REGEXP = 2
YT_KEYSEQ_IDX = 3; YT_MOD = 4; YT_TOKEN_SEQ = 5

# --- 構築 ---

def push_string(fas, s: str) -> bool:
    b = s.encode("utf-8")
    # バッファを保持しておくこと (on_load_setting の終わりまで)
    buf = ctypes.create_string_buffer(b)
    ptr = ctypes.cast(buf, ctypes.c_void_p).value
    return lib.ys_func_args_push(fas, YT_STRING, ptr, len(b))

def push_number(fas, n: int) -> bool:
    return lib.ys_func_args_push(fas, YT_NUMBER, ctypes.c_int64(n).value, 0)

# --- 読み取り ---

def read_args(fas) -> list:
    result = []
    n = lib.ys_func_args_length(fas)
    for i in range(n):
        v, l = ctypes.c_int64(0), ctypes.c_int64(0)
        t = lib.ys_func_args_get(fas, i, ctypes.byref(v), ctypes.byref(l))
        if t == YT_STRING or t == YT_REGEXP:
            s = ctypes.string_at(v.value, l.value).decode("utf-8")
            result.append({"type": "string" if t == YT_STRING else "regexp", "value": s})
        elif t == YT_NUMBER:
            result.append({"type": "number", "value": ctypes.c_int32(v.value).value})
        elif t == YT_KEYSEQ_IDX:
            result.append({"type": "keyseq_idx", "value": v.value & 0xFFFFFFFF})
        elif t == YT_MOD:
            result.append({"type": "mod",
                            "modifiers": v.value & 0xFFFFFFFFFFFFFFFF,
                            "dontcares": l.value & 0xFFFFFFFFFFFFFFFF})
        elif t == YT_TOKEN_SEQ:
            ss = ctypes.c_void_p(v.value)
            sn = lib.ys_strs_length(ss)
            tokens = []
            for j in range(sn):
                sp = ctypes.c_char_p(); sl = ctypes.c_size_t()
                lib.ys_strs_get(ss, j, ctypes.byref(sp), ctypes.byref(sl))
                tokens.append(ctypes.string_at(sp, sl.value).decode("utf-8"))
            result.append({"type": "token_seq", "value": tokens})
    return result
```

### Ruby (ffi gem)

```ruby
require 'ffi'

module YamyScripter
  extend FFI::Library
  ffi_lib "yamy-scripter.dll"

  YT_STRING     = 0
  YT_NUMBER     = 1
  YT_REGEXP     = 2
  YT_KEYSEQ_IDX = 3
  YT_MOD        = 4
  YT_TOKEN_SEQ  = 5

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

  # --- 構築 ---

  def self.push_string(fas, s)
    buf = FFI::MemoryPointer.from_string(s.encode("UTF-8"))
    ys_func_args_push(fas, YT_STRING, buf.address, s.bytesize)
  end

  def self.push_number(fas, n)
    ys_func_args_push(fas, YT_NUMBER, n, 0)
  end

  # --- 読み取り ---

  def self.read_args(fas)
    n = ys_func_args_length(fas)
    n.times.map do |i|
      vp = FFI::MemoryPointer.new(:int64)
      lp = FFI::MemoryPointer.new(:int64)
      t  = ys_func_args_get(fas, i, vp, lp)
      v, l = vp.read_int64, lp.read_int64
      case t
      when YT_STRING, YT_REGEXP
        { type: t == YT_STRING ? :string : :regexp,
          value: FFI::Pointer.new(v).read_bytes(l).force_encoding("UTF-8") }
      when YT_NUMBER
        { type: :number, value: [v].pack("q<").unpack1("l<") }
      when YT_KEYSEQ_IDX
        { type: :keyseq_idx, value: v & 0xFFFFFFFF }
      when YT_MOD
        { type: :mod, modifiers: v & 0xFFFFFFFF_FFFFFFFF,
                      dontcares: l & 0xFFFFFFFF_FFFFFFFF }
      when YT_TOKEN_SEQ
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
```

---

## `ys_exec_keyseq` — yamy 組み込み関数の呼び出し

`on_exec_user_func` 内から yamy 組み込み関数 (`&OSD.Display` 等) を呼び出すには
`ys_exec_keyseq` を使う。アクション文字列は mayu 構文で記述する。

```c
void on_exec_user_func(const char* name,
                        const YsFuncArgs* preset_args,
                        const YsTriggerInfo* trigger_info)
{
    // "&関数名(引数)" の形で mayu 構文アクション文字列として指定
    ys_exec_keyseq("&OSD.Display(\"hello\")", trigger_info);
}
```

制約 (c-api.md 参照):
- `on_exec_user_func` 内でのみ有効 (`on_load_setting` 内では false を返す)
- `&ExecUserFunc` をアクション文字列に含めることは禁止 (無限ループ防止)


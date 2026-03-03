# DLL 公開 C API 仕様案

FFI/mrubyで使用する他言語インターフェースの仕様案。

- FFIとしてなるべく追加コード量が少なくなるようなAPIを規定する。
- 文字列は NUL終端のUTF-8(BOM無し)とする。(ライブラリ内部で wchar_t に変換する)

### ファイル: `scripter/yamy_scripter.h`

Scripter API 一覧を以下に示す。

```c
// ユーザー定義関数呼び出し時の入力コンテキスト
typedef struct YscrInputCtx {
    // 検討中
} YscrInputCtx;

// 型付きデータ。下位3ビットをタグとして扱う(※)。データアクセスにはマクロを使用する
// タグの意味は以下の通り:
// - 0b000: YscrStr*
// - 0b001: int64_t (※61bit符号付き整数として扱う)
// - 0b010: KeySeqのインデックス
// - 0b011-0b111: 予約
// ※MSVCのmallocは、32bit環境では8, 64bit環境では16のアラインメントを取ることが保証されている
typedef struct YscrTypedVal {
    int64_t data;
} YscrTypedVal;

// 文字列型
typedef struct YscrStr {
    int32_t len; // 確保するデータサイズ: offsetof(YscrStr, val) + len + 1 (終端NUL分)
    const char val[1]; // NUL終端のUTF-8
} YscrStr;

// 型付きデータ配列の確保およびリサイズ
// - リサイズ時、ポインタの値は変更される場合がある
// - CmdCommit, CmdAbort, もしくはCmdCallEngineFunc実行後に自動解放される
YSCR_API YscrTypedVal* yscr_new_typed_val_array(int n);
YSCR_API YscrTypedVal* yscr_resize_typed_val_array(YscrTypedVal* tvs, int n);

// 型付きデータ配列の要素へのデータ書き込み
// - svalueが非NULLならばNUL終端UTF-8文字列としてコピーされる
// - svalueがNULLならばそのままNULLが設定され、yscr_to_str()の呼び出しでNULLが返る
// - ivalueは61bitの符号付き整数として扱われる
// - xvalueはKeqSeqのインデックス。KeySeqはEngineとScripter双方で同じ値を保持する
YSCR_API void yscr_tvs_set_str(YscrTypedVal* tvs, int i, const char* svalue);
YSCR_API void yscr_tvs_set_int(YscrTypedVal* tvs, int i, int64_t ivalue);
YSCR_API void yscr_tvs_set_keyseq_idx(YscrTypedVal* tvs, int i, int xvalue);

// 型付きデータ読み込み。書き込みは禁止
#define yscr_is_str(tv) (((tv).data&7)==0x00)
#define yscr_is_num(tv) (((tv).data&7)==0x01)
#define yscr_is_keyseq_idx(tv) (((tv).data&7)==0x02)
#define yscr_to_str(tv) ((YscrStr*)((tv).data))
#define yscr_to_int(tv) ((tv).data>>3)
#define yscr_to_keyseq_idx(tv) ((int)((tv).data>>3))

// 設定情報構築要求。yscr_start()で初期設定が完了したら呼ばれる
typedef bool (*yscr_ctrl_load_setting)();

// ユーザー定義関数実行要求
typedef void (*yscr_ctrl_call_user_func)(YscrInputCtx*, const char* /* user_func_name */,
                                         const YscrTypedVal* /* preset_args */, int /* preset_arg_count */);

// yscr_start で渡すコールバック構造体
typedef struct YscrCallbacks {
    yscr_ctrl_load_setting load_setting;
    yscr_ctrl_call_user_func call_user_func;
} YscrCallbacks;

// scripterのメインループを開始。以下の場合、処理を終了する
// - Engineから終了コマンドを受信した場合 (返り値: 0)
// - yscr_ctrl_load_setting()がfalseを返した場合 (返り値: 1)
YSCR_API int yscr_start(YscrCallbacks *callbacks);

// バージョン確認 (FFI 利用時の互換性検証用)
YSCR_API uint32_t yscr_version(void);

// 各項目設定。callbacks->load_setting内で下記APIを呼び出して設定情報を構築する。
// callbacks->load_setting終了時までキューイングしておき、trueが返却されたらEngineにCmdCommitとともに送信される。
// falseが返却されたら、キューをキャンセルしてCmdAbortが送信される。

// キーシーケンスを登録する。登録が成功した場合、もしくは既存のキーシーケンスが存在した場合は0以上のインデックス値を返す。
// キーシーケンスの登録に失敗した場合は -1 を返す。
// name: キーシーケンス名 (NULL で匿名。空文字列不可)
// actions: キーシーケンスの内容をmayu構文で記述した文字列 (例: "A B C", "&BeginningOfLine", "$otherseq")
YSCR_API int yscr_reg_keyseq(const char* name, const char* actions);

// def key ... 相当
// names: キー名の配列 (複数のエイリアス名を含む)
// scan_codes: スキャンコード文字列の配列 (例: "0x1c", "E0-0x1c", "E1-0x1d")
YSCR_API bool yscr_def_key(const char* const* names, int names_count,
                            const char* const* scan_codes, int scan_codes_count);

// def mod ... 相当
// modifier_name: モディファイア名 (例: "Shift", "Control")
// key_names: このモディファイアに対応するキー名の配列
YSCR_API bool yscr_def_mod(const char* modifier_name,
                            const char* const* key_names, int key_names_count);

// def sync ... 相当
// scan_codes: 同期キーのスキャンコード文字列の配列 (例: "E1-0x1d", "0x45")
YSCR_API bool yscr_def_sync(const char* const* scan_codes, int scan_codes_count);

// def alias ... 相当
// alias_name: エイリアス名, key_name: 元のキー名
YSCR_API bool yscr_def_alias(const char* alias_name, const char* key_name);

// def subst ... 相当
// lhs_mod_keys: 変換元の修飾キー文字列の配列 (例: "*-LButton")
// rhs_keyseq_idx: 変換先キーシーケンスのインデックス (yscr_reg_keyseqで取得)
YSCR_API bool yscr_def_subst(const char* const* lhs_mod_keys, int lhs_count,
                              int rhs_keyseq_idx);

// def option ... 相当
// option_name: オプション名 (例: "KL-", "delay-of !!!", "mouse-event", "drag-threshold")
// value: 値文字列 (例: "true", "500")
YSCR_API bool yscr_def_option(const char* option_name, const char* value);

// define SYMBOL ... 相当
// symbol_name: シンボル名 (例: "KBD104")
YSCR_API bool yscr_def_symbol(const char* symbol_name);

// keymap, keymap2, window 相当。キーマップ依存の命令は、以後このキーマップに紐付けられる
// keyword: "keymap", "keymap2", "window" のいずれか
// name: キーマップ名
// window_class: ウィンドウクラス名の正規表現 (window以外は NULL)
// window_title: ウィンドウタイトルの正規表現 (window以外は NULL)
// window_op: "&&", "||", または NULL (window以外は NULL)
// parent_name: 親キーマップ名 (なければ NULL)
// default_keyseq_idx: デフォルトキーシーケンスのインデックス (-1 はなし)
YSCR_API bool yscr_begin_keymap(const char* keyword, const char* name,
                                 const char* window_class, const char* window_title,
                                 const char* window_op, const char* parent_name,
                                 int default_keyseq_idx);

// key KEY = ... 相当
// lhs_mod_keys: 左辺の修飾キー文字列の配列 (例: "A", "S-A", "~S-A")
// rhs_keyseq_idx: 右辺キーシーケンスのインデックス
YSCR_API bool yscr_assign_key(const char* const* lhs_mod_keys, int lhs_count,
                               int rhs_keyseq_idx);

// event EVENT = ... 相当
// event_name: イベント名 (例: "before-key-down", "after-key-up", "prefixed")
// rhs_keyseq_idx: 右辺キーシーケンスのインデックス
YSCR_API bool yscr_assign_event(const char* event_name, int rhs_keyseq_idx);

// mod MOD = ... 相当
// modifier_name: モディファイア名 (例: "Shift")
// op: 代入演算子 "=", "+=", "-=" のいずれか
// keys: キー名の配列。アサインモード接頭辞付き可 (例: "LShift", "!LShift", "!!RShift", "!!!CapsLock")
YSCR_API bool yscr_assign_mod(const char* modifier_name, const char* op,
                              const char* const* keys, int keys_count);

// mod MOD = ... 相当 (prefix_mods あり)
// prefixes: prefix のアサインモード + 修飾子名 ("!Shift", "!!!Ctrl" など) の配列
// prefix_count: prefix の数
// それ以外の引数は yscr_assign_mod と同じ
YSCR_API bool yscr_assign_mod_ex(const char* const* prefixes, int prefix_count,
                                  const char* modifier_name, const char* op,
                                  const char* const* keys, int keys_count);

// 登録したキーシーケンスのインデックスを取得する。未登録の場合は -1 を返す
YSCR_API int yscr_get_keyseq_idx(const char* name);

// ユーザー定義関数を登録する。Engineから呼び出されると callbacks->call_user_func が呼ばれる
// func_name: 登録する関数名
// preset_args, preset_arg_count: Engineに送信され、呼び出し時に callbacks->call_user_func の引数として返される
YSCR_API bool yscr_reg_user_func(const char* func_name, YscrTypedVal *preset_args, int preset_arg_count);

// キューに登録した設定情報をリセットする。
YSCR_API bool yscr_reset();

// yscr_resetを呼んだ後、*.mayuを読み込む処理を行う。
// これを使用する場合は、callbacks->load_setting内で "return yscr_load_mayu();" のように記述すること。
YSCR_API bool yscr_load_mayu();

// callbacks->call_user_func内で使用する。Engineの関数を実行する。argsはyscr_new_typed_val_array()
// callbacks->load_setting内では使用禁止
YSCR_API void yscr_call_engine_func(const char* engine_func_name, YscrTypedVal *args, int arg_count);
```

### 初期化処理とイベントループ

1. scripterプロセスが起動し、yscr_start()を呼ぶと、内部初期化処理を行なった後に callbacks->load_setting が呼ばれる。
2. callbacks->load_settingがtrueを返すと、キューイングした設定情報およびCmdCommitを送信。以後コマンド要求待ち。
    - falseを返すとキューを破棄してCmdAbortを送信。
3. Engineがユーザー定義関数呼び出しを実行すると、ctrlチャネル経由で callbacks->call_user_func が呼ばれる。
    - ユーザー定義関数内で、Engineの関数を実行したい場合は、yscr_call_engine_func()を呼び出す。
4. Engineが再読み込みもしくは終了を選択すると、yscr_start()が終了する。
    - 再読み込み時はscripterプロセス終了後、再起動される。

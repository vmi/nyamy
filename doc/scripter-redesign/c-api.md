# DLL 公開 C API 仕様案

FFI/mrubyで使用する他言語インターフェースの仕様案。

- FFIとしてなるべく追加コード量が少なくなるようなAPIを規定する。
- 文字列は NUL終端のUTF-8(BOM無し)とする。(ライブラリ内部で wchar_t に変換する)

### ファイル: `scripter/yamy_scripter.h`

Scripter API 一覧を以下に示す。

```c
// std::vector<YsFuncArg> に対するラッパー (不透明型; 内部型 YsFuncArg は ys_types.h に定義)
typedef struct YsFuncArgs YsFuncArgs;

// std::vector<std::string> (UTF-8) に対するラッパー (不透明型)
typedef struct YsStrs YsStrs;

// 引数要素の型タグ
typedef enum YsType {
    YsType_Error        = -1,
	YsType_String       = 0,
	YsType_Number       = 1,
	YsType_Regexp       = 2,
	YsType_KeySeqIdx    = 3,
	YsType_ModifierSpec = 4,
	YsType_TokenSeq     = 5,
} YsType;

// 書き込み用 YsFuncArgs の作成。コールバック関数から戻った段階で全て解放される。
YS_API YsFuncArgs* ys_func_args_new(void);
// 文字列配列の作成。コールバック関数から戻った段階で全て解放される。
YS_API YsStrs* ys_strs_new(void);

// std::vector<...> の長さ
YS_API int ys_func_args_length(const YsFuncArgs* fas);
YS_API int ys_strs_length(const YsStrs *ss);

// YsType_String: *p_value is const char*, *p_length is count of bytes (not include the end of NUL).
// (This lifetime same as YsFuncArgs*)
// YsType_Number: *p_value is number (int32_t)
// YsType_Regexp: *p_value is const char*, *p_length is count of bytes (not include the end of NUL).
// (This lifetime same as YsFuncArgs*)
// YsType_KeySeqIdx: *p_value is number (uint32_t)
// YsType_ModifierSpec: *p_value is modifier bitmask (uint64_t), *p_length is dontcare bitmask (uint64_t).
// YsType_TokenSeq: *p_value is const YsStrs*, *p_length is length of string array (same as ys_strs_length(*p_value)).
YS_API YsType ys_func_args_get(const YsFuncArgs* fas, int idx, int64_t* p_value, int64_t* p_length);
YS_API bool ys_strs_get(const YsStrs* ss, int idx, const char** p_value, size_t* p_length);

// ys_***_get と対称。type に応じた value/length の解釈は ys_***_get のコメントと同じ。
YS_API bool ys_func_args_push(YsFuncArgs* fas, YsType type, int64_t value, int64_t length);
YS_API bool ys_strs_push(YsStrs* ss, const char* value, size_t length);

// 設定ロードが要求されたときに呼び出されるコールバック
typedef bool (*ys_on_load_setting)(void);

// Engine側で &ExecUserFunc() が実行されたときに呼び出されるコールバック
// args: Engine が ExecUserFunc コマンドとともに送った引数列
typedef void (*ys_on_exec_user_func)(const char* /* user_func_name */,
                                     const YsFuncArgs* /* args */);

// scripterのメインループを開始。以下の場合、処理を終了する
// - Engineから終了コマンドを受信した場合 (返り値: 0)
// - on_load_setting()がfalseを返した場合 (返り値: 1)
YS_API int ys_start(const ys_on_load_setting on_load_setting);

// バージョン確認 (FFI 利用時の互換性検証用)
YS_API uint32_t ys_version(void);

// 各項目設定。on_load_setting内で下記APIを呼び出して設定情報を構築する。
// on_load_setting終了時までキューイングしておき、trueが返却されたらEngineにCmdCommitとともに送信される。
// falseが返却されたら、キューをキャンセルしてCmdAbortが送信される。

// キーシーケンスを登録し、割り当てられたインデックスを返す。失敗した場合は -1 を返す。
// name が指定されており、同名のキーシーケンスが既に登録済みの場合は内容を上書きして既存のインデックスを返す。
// 登録されたキーシーケンスはプロセス終了時まで保持される。
// name: キーシーケンス名 (NULL で匿名。空文字列不可)
// actions: キーシーケンスの内容をmayu構文で記述した文字列 (例: "A B C", "&BeginningOfLine", "$otherseq")
YS_API int ys_reg_keyseq(const char* name, const char* actions);

// def key ... 相当
// names: キー名のYsStrs(全ての要素は文字列であること) (複数のエイリアス名を含む)
// scan_codes: スキャンコード文字列のYsStrs(全ての要素は文字列であること) (例: "0x1c", "E0-0x1c", "E1-0x1d")
YS_API bool ys_def_key(const YsStrs* names, const YsStrs* scancodes);

// def mod ... 相当
// modifier_name: モディファイア名 (例: "Shift", "Control")
// key_names: このモディファイアに対応するキー名のYsStrs(全ての要素は文字列であること)
YS_API bool ys_def_mod(const char* modifier_name, const YsStrs* key_names);

// def sync ... 相当
// scan_codes: 同期キーのスキャンコード文字列のYsStrs(全ての要素は文字列であること) (例: "E1-0x1d", "0x45")
YS_API bool ys_def_sync(const YsStrs* scan_codes);

// def alias ... 相当
// alias_name: エイリアス名, key_name: 元のキー名
YS_API bool ys_def_alias(const char* alias_name, const char* key_name);

// def subst ... 相当
// lhs_mod_keys: 変換元の修飾キー文字列のYsStrs(全ての要素は文字列であること) (例: "*-LButton")
// rhs_keyseq_idx: 変換先キーシーケンスのインデックス (ys_reg_keyseqで取得)
YS_API bool ys_def_subst(const YsStrs* lhs_mod_keys, int rhs_keyseq_idx);

// def option ... 相当
// option_name: オプション名 (例: "KL-", "delay-of !!!", "mouse-event", "drag-threshold")
// value: 値文字列 (例: "true", "500")
YS_API bool ys_def_option(const char* option_name, const char* value);


// keymap, keymap2, window 相当。キーマップ依存の命令は、以後このキーマップに紐付けられる
// keyword: "keymap", "keymap2", "window" のいずれか
// name: キーマップ名
// window_class: ウィンドウクラス名の正規表現 (window以外は NULL)
// window_title: ウィンドウタイトルの正規表現 (window以外は NULL)
// window_op: "&&", "||", または NULL (window以外は NULL)
// parent_name: 親キーマップ名 (なければ NULL)
// default_keyseq_idx: デフォルトキーシーケンスのインデックス (-1 はなし)
YS_API bool ys_begin_keymap(const char* keyword, const char* name,
                            const char* window_class, const char* window_title,
                            const char* op, const char* parent_name,
                            int default_keyseq_idx);

// key KEY = ... 相当
// lhs_mod_keys: 左辺の修飾キー文字列のYsStrs(全ての要素は文字列であること) (例: "A", "S-A", "~S-A")
// rhs_keyseq_idx: 右辺キーシーケンスのインデックス
YS_API bool ys_assign_key(const YsStrs* lhs_mod_keys, int rhs_keyseq_idx);

// event EVENT = ... 相当
// event_name: イベント名 (例: "before-key-down", "after-key-up", "prefixed")
// rhs_keyseq_idx: 右辺キーシーケンスのインデックス
YS_API bool ys_assign_event(const char* event_name, int rhs_keyseq_idx);

// mod MOD = ... 相当
// prefixes: prefix のアサインモード + 修飾子名 ("!Shift", "!!!Ctrl" など) のYsStrs(全ての要素は文字列であること)
//           (なければ NULL)
// modifier_name: モディファイア名 (例: "Shift")
// op: 代入演算子 "=", "+=", "-=" のいずれか
// keys: キー名のYsStrs(全ての要素は文字列であること)。
//       アサインモード接頭辞付き可 (例: "LShift", "!LShift", "!!RShift", "!!!CapsLock")
YS_API bool ys_assign_mod(const YsStrs* prefixes, const char* modifier_name, const char* op, const YsStrs* keys);

// 登録したキーシーケンスのインデックスを取得する。未登録の場合は -1 を返す
YS_API int ys_get_keyseq_idx(const char* name);

// ユーザー定義関数を登録する。Engine から &ExecUserFunc(func_name) が実行されると
// 登録したハンドラが呼ばれる。Engine から送られた引数は on_exec_user_func の args に入る。
// func_name: 登録する関数名
// on_exec_user_func: 関数のハンドラ
YS_API bool ys_reg_user_func(const char* func_name, ys_on_exec_user_func on_exec_user_func);

// キューに登録した設定情報をリセットする。
YS_API bool ys_reset_setting(void);

// ys_reset_settingを呼んだ後、*.mayuを読み込む処理を行う。
// これを使用する場合は、on_load_setting内で "return ys_load_mayu();" のように記述すること。
YS_API bool ys_load_mayu(void);

// adhocなキーシーケンスの実行をEngine側に要求する
// - キーシーケンスでの &ExecUserFunc は使用禁止 (無限ループ防止のため実装内でガード、falseを返す)
// - on_exec_user_func内で使用する (そのコールバックのトリガーコンテキストで実行される)
// - on_load_setting内では使用禁止 (falseを返す)
YS_API bool ys_exec_keyseq(const char* actions);

// 指定パスの .mayu ファイルをコンパイルしてキューに積む
// path: UTF-8 ファイルパス (相対パスは設定ファイルと同ディレクトリから解決)
// on_load_setting 内でのみ有効
YS_API bool ys_include_mayu(const char* path);

// ホームディレクトリ一覧を返す (設定ファイル探索の基準パス群, UTF-8 YsStrs)
// 実行ファイルのディレクトリ, %LOCALAPPDATA%\Programs\Yamy, 同\conf を含む
// 返り値: コールバックセッションのライフタイムで管理。手動解放不要
// on_load_setting / on_exec_user_func 内で有効
YS_API YsStrs* ys_get_home_directories(void);

// Windows レジストリから現在の設定プロファイル情報を取得する
// out_name:    プロファイル名 (NULL 可) — 次回本関数呼び出しまで有効
// out_path:    ファイルパス   (NULL 可) — 次回本関数呼び出しまで有効
// out_symbols: シンボル名の YsStrs (NULL 可) — セッションライフタイム
// 返り値: レジストリに有効な設定が存在すれば true
// on_load_setting / on_exec_user_func 内で有効
YS_API bool ys_get_registry_config(const char** out_name,
                                    const char** out_path,
                                    YsStrs**     out_symbols);

// 設定ファイル名を絶対パスに解決する (コンパイルなし)
// name: ファイル名 (NULL または "" でデフォルト .mayu を探索)
//       空の場合: レジストリ → ホームディレクトリの .mayu の順で探索
//       指定の場合: ホームディレクトリ内で検索
// out_path: 解決した絶対パス (NULL 可) — 次回本関数呼び出しまで有効
// 返り値: ファイルが見つかれば true (見つからない場合は ys_last_error() にエラーが入る)
// on_load_setting / on_exec_user_func 内で有効
YS_API bool ys_resolve_config_path(const char*  name,
                                    const char** out_path);

// 最後のエラーメッセージを返す (UTF-8 NUL 終端)
// エラーなし / 未発生の場合は NULL を返す
YS_API const char* ys_last_error(void);
```

### 初期化処理とイベントループ

1. scripterプロセスが起動し、ys_start()を呼ぶと、内部初期化処理を行なった後に on_load_setting が呼ばれる。
2. on_load_settingがtrueを返すと、キューイングした設定情報およびCmdCommitを送信。以後コマンド要求待ち。
    - falseを返すとキューを破棄してCmdAbortを送信。
3. Engineがユーザー定義関数呼び出し (`&ExecUserFunc` / `@func_name`) を実行すると、ctrlチャネル経由でExecUserFuncコマンドと引数・トリガーコンテキストが届く。
    scripterは関数名でys_reg_user_funcに登録したハンドラをルックアップし、受信した引数を `args` として呼び出す。
    - ユーザー定義関数内で、Engineのキーシーケンスを実行したい場合は、ys_exec_keyseq()を呼び出す。
      トリガーコンテキストは内部で自動的に引き継がれる。
4. Engineが再読み込みもしくは終了を選択すると、ys_start()が終了する。
    - 再読み込み時はscripterプロセス終了後、再起動される。

### CmdCommit 後のメモリ管理

CmdCommit 送信後、以下の情報はプロセス終了時まで**恒久的に保持**される。

| 情報 | 理由 |
|------|------|
| **登録済み KeySeq**（`ys_reg_keyseq`）| インデックス値は Engine との間で共有される識別子であり、プロセスライフタイム中は安定している必要がある。名前→インデックスのマッピングを保持することで、重複登録を避けつつ同一インデックスを返せる。KeySeq の実データ（actions 文字列）は Engine が保持するため scripter 側では不要。 |
| **シンボル**（プロセス起動時に渡されたシンボルセット）| on_load_setting 呼び出し時に参照するため、少なくとも on_load_setting の実行期間中は保持が必要。プロセスライフタイム中に変化しないため、解放せず保持する方が実装上簡潔。 |
| **登録済みユーザー関数**（`ys_reg_user_func`）| CmdCommit 後も Engine から ExecUserFunc が随時届くため、関数名→ハンドラのマッピングを常に参照できる必要がある。 |

上記以外の設定情報構築に使用したメモリ（キーマップ定義、キー/モディファイア定義、中間コンパイル状態など）は CmdCommit 送信後に解放できる。これらは CmdStream 経由で Engine に転送済みであり、scripter 側では不要となる。

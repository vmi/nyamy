# DLL 公開 C API 仕様

FFI/mrubyで使用する他言語インターフェースの仕様。

- FFIとしてなるべく追加コード量が少なくなるようなAPIを規定する。
- 文字列は NUL終端のUTF-8(BOM無し)とする。(ライブラリ内部で wchar_t に変換する)

### ファイル: `scripter/nyamy_scripter.h`

Scripter API 一覧を以下に示す。

```c
// std::vector<NYsFuncArg> に対するラッパー (不透明型; 内部型 NYsFuncArg は nys_types.h に定義)
typedef struct NYsFuncArgs NYsFuncArgs;

// std::vector<std::string> (UTF-8) に対するラッパー (不透明型)
typedef struct NYsStrs NYsStrs;

// 引数要素の型タグ
typedef enum NYsType {
    NYsType_Error        = -1,
	NYsType_String       = 0,
	NYsType_Number       = 1,
	NYsType_Regexp       = 2,
	NYsType_KeySeqIdx    = 3,
	NYsType_ModifierSpec = 4,
	NYsType_TokenSeq     = 5,
} NYsType;

// 書き込み用 NYsFuncArgs の作成。コールバック関数から戻った段階で全て解放される。
NYS_API NYsFuncArgs* nys_func_args_new(void);
// 文字列配列の作成。コールバック関数から戻った段階で全て解放される。
NYS_API NYsStrs* nys_strs_new(void);

// std::vector<...> の長さ
NYS_API int nys_func_args_length(const NYsFuncArgs* fas);
NYS_API int nys_strs_length(const NYsStrs *ss);

// NYsType_String: *p_value is const char*, *p_length is count of bytes (not include the end of NUL).
// (This lifetime same as NYsFuncArgs*)
// NYsType_Number: *p_value is number (int32_t)
// NYsType_Regexp: *p_value is const char*, *p_length is count of bytes (not include the end of NUL).
// (This lifetime same as NYsFuncArgs*)
// NYsType_KeySeqIdx: *p_value is number (uint32_t)
// NYsType_ModifierSpec: *p_value is modifier bitmask (uint64_t), *p_length is dontcare bitmask (uint64_t).
// NYsType_TokenSeq: *p_value is const NYsStrs*, *p_length is length of string array (same as nys_strs_length(*p_value)).
NYS_API NYsType nys_func_args_get(const NYsFuncArgs* fas, int idx, int64_t* p_value, int64_t* p_length);
NYS_API bool nys_strs_get(const NYsStrs* ss, int idx, const char** p_value, size_t* p_length);

// nys_***_get と対称。type に応じた value/length の解釈は nys_***_get のコメントと同じ。
NYS_API bool nys_func_args_push(NYsFuncArgs* fas, NYsType type, int64_t value, int64_t length);
NYS_API bool nys_strs_push(NYsStrs* ss, const char* value, size_t length);

// 設定ロードが要求されたときに呼び出されるコールバック
// exeCtx: nys_start() に渡した呼び出し元コンテキストポインタ
typedef bool (*nys_on_load_setting)(void* exeCtx);

// Engine側で &ExecUserFunc() が実行されたときに呼び出されるコールバック
// exeCtx: nys_start() に渡した呼び出し元コンテキストポインタ
// args: Engine が ExecUserFunc コマンドとともに送った引数列
typedef void (*nys_on_exec_user_func)(void*             /* exeCtx */,
                                     const char*       /* user_func_name */,
                                     const NYsFuncArgs* /* args */);

// コールバックテーブル。on_quit は省略可 (NULL 可)
typedef struct NYsCallbacks {
    bool (*on_load_setting)(void* exeCtx);
    void (*on_quit)(void* exeCtx);          // Quit / Reload 直前に呼ばれる (省略可)
} NYsCallbacks;

// scripterのメインループを開始。以下の場合、処理を終了する
// - Engineから終了コマンドを受信した場合 (返り値: 0)
// - on_load_setting()がfalseを返した場合 (返り値: 1)
// callbacks: コールバックテーブル (on_load_setting は必須)
// exeCtx:   各コールバックに透過的に渡される呼び出し元コンテキストポインタ
// コールバックは呼び出し元スレッドで実行され、ctrl ストリームの読み出しは
// 専用スレッドが行う (実行中でも Quit を観測できるようにするため)
NYS_API int nys_start(const NYsCallbacks* callbacks, void* exeCtx);

// Quit (または ctrl パイプの EOF) 観測後、実行中のコールバックの完了を待つ時間 [ms]。
// タイムアウトするとプロセスを強制終了する (走り続けるスクリプトは中断できず、
// nyamy 側の reader スレッドは本プロセスが書き込み端を閉じるまで解放されないため)。
// 既定は 0 = 無期限に待つ。プロセス内ホストが道連れにされないようにするための既定値で、
// nyamy が起動する scripter は kScripterQuitTimeoutMillisec (ctrl_stream.h) を渡す。
// nys_start() の前に呼ぶこと。
NYS_API void nys_set_quit_timeout(uint32_t millisec);

// バージョン確認 (FFI 利用時の互換性検証用)
// NYamy が 0.9.x の間は NYamy 本体と同じ値。1.0.0 リリース時に 1.0.0 で固定し、
// 以後は本 C API が変わったときだけ動かす。
NYS_API uint32_t nys_version(void);

// 各項目設定。on_load_setting内で下記APIを呼び出して設定情報を構築する。
// on_load_setting終了時までキューイングしておき、trueが返却されたらEngineにCmdCommitとともに送信される。
// falseが返却されたら、キューをキャンセルしてCmdAbortが送信される。

// キーシーケンスを登録し、割り当てられたインデックスを返す。失敗した場合は -1 を返す。
// name が指定されており、同名のキーシーケンスが既に登録済みの場合は内容を上書きして既存のインデックスを返す。
// 登録されたキーシーケンスはプロセス終了時まで保持される。
// name: キーシーケンス名 (NULL で匿名。空文字列不可)
// actions: キーシーケンスの内容をmayu構文で記述した文字列 (例: "A B C", "&BeginningOfLine", "$otherseq")
NYS_API int nys_reg_keyseq(const char* name, const char* actions);

// def key ... 相当
// names: キー名のNYsStrs(全ての要素は文字列であること) (複数のエイリアス名を含む)
// scan_codes: スキャンコード文字列のNYsStrs(全ての要素は文字列であること) (例: "0x1c", "E0-0x1c", "E1-0x1d")
NYS_API bool nys_def_key(const NYsStrs* names, const NYsStrs* scancodes);

// def mod ... 相当
// modifier_name: モディファイア名 (例: "Shift", "Control")
// key_names: このモディファイアに対応するキー名のNYsStrs(全ての要素は文字列であること)
NYS_API bool nys_def_mod(const char* modifier_name, const NYsStrs* key_names);

// def sync ... 相当
// scan_codes: 同期キーのスキャンコード文字列のNYsStrs(全ての要素は文字列であること) (例: "E1-0x1d", "0x45")
NYS_API bool nys_def_sync(const NYsStrs* scan_codes);

// def alias ... 相当
// alias_name: エイリアス名, key_name: 元のキー名
NYS_API bool nys_def_alias(const char* alias_name, const char* key_name);

// def subst ... 相当
// lhs_mod_keys: 変換元の修飾キー文字列のNYsStrs(全ての要素は文字列であること) (例: "*-LButton")
// rhs_keyseq_idx: 変換先キーシーケンスのインデックス (nys_reg_keyseqで取得)
NYS_API bool nys_def_subst(const NYsStrs* lhs_mod_keys, int rhs_keyseq_idx);

// def option ... 相当
// option_name: オプション名 (例: "KL-", "delay-of !!!", "mouse-event", "drag-threshold")
// value: 値文字列 (例: "true", "500")
NYS_API bool nys_def_option(const char* option_name, const char* value);


// keymap, keymap2, window 相当。キーマップ依存の命令は、以後このキーマップに紐付けられる
// keyword: "keymap", "keymap2", "window" のいずれか
// name: キーマップ名
// window_class: ウィンドウクラス名の正規表現 (window以外は NULL)
// window_title: ウィンドウタイトルの正規表現 (window以外は NULL)
// window_op: "&&", "||", または NULL (window以外は NULL)
// parent_name: 親キーマップ名 (なければ NULL)
// default_keyseq_idx: デフォルトキーシーケンスのインデックス (-1 はなし)
NYS_API bool nys_begin_keymap(const char* keyword, const char* name,
                            const char* window_class, const char* window_title,
                            const char* op, const char* parent_name,
                            int default_keyseq_idx);

// key KEY = ... 相当
// lhs_mod_keys: 左辺の修飾キー文字列のNYsStrs(全ての要素は文字列であること) (例: "A", "S-A", "~S-A")
// rhs_keyseq_idx: 右辺キーシーケンスのインデックス
NYS_API bool nys_assign_key(const NYsStrs* lhs_mod_keys, int rhs_keyseq_idx);

// event EVENT = ... 相当
// event_name: イベント名 (例: "before-key-down", "after-key-up", "prefixed")
// rhs_keyseq_idx: 右辺キーシーケンスのインデックス
NYS_API bool nys_assign_event(const char* event_name, int rhs_keyseq_idx);

// mod MOD = ... 相当
// prefixes: prefix のアサインモード + 修飾子名 ("!Shift", "!!!Ctrl" など) のNYsStrs(全ての要素は文字列であること)
//           (なければ NULL)
// modifier_name: モディファイア名 (例: "Shift")
// op: 代入演算子 "=", "+=", "-=" のいずれか
// keys: キー名のNYsStrs(全ての要素は文字列であること)。
//       アサインモード接頭辞付き可 (例: "LShift", "!LShift", "!!RShift", "!!!CapsLock")
NYS_API bool nys_assign_mod(const NYsStrs* prefixes, const char* modifier_name, const char* op, const NYsStrs* keys);

// 登録したキーシーケンスのインデックスを取得する。未登録の場合は -1 を返す
NYS_API int nys_get_keyseq_idx(const char* name);

// ユーザー定義関数を登録する。Engine から &ExecUserFunc(func_name) が実行されると
// 登録したハンドラが呼ばれる。Engine から送られた引数は on_exec_user_func の args に入る。
// func_name: 登録する関数名
// on_exec_user_func: 関数のハンドラ
NYS_API bool nys_reg_user_func(const char* func_name, nys_on_exec_user_func on_exec_user_func);

// キューに登録した設定情報をリセットする。
NYS_API bool nys_reset_setting(void);

// nys_reset_settingを呼んだ後、*.mayuを読み込む処理を行う。
// これを使用する場合は、on_load_setting内で "return nys_load_mayu();" のように記述すること。
NYS_API bool nys_load_mayu(void);

// adhocなキーシーケンスの実行をEngine側に要求する
// - キーシーケンスでの &ExecUserFunc は使用禁止 (無限ループ防止のため実装内でガード、falseを返す)
// - on_exec_user_func内で使用する (そのコールバックのトリガーコンテキストで実行される)
// - on_load_setting内では使用禁止 (falseを返す)
NYS_API bool nys_exec_keyseq(const char* actions);

// 指定パスの .mayu ファイルをコンパイルしてキューに積む
// path: UTF-8 ファイルパス (相対パスは設定ファイルと同ディレクトリから解決)
// on_load_setting 内でのみ有効
NYS_API bool nys_include_mayu(const char* path);

// ホームディレクトリ一覧を返す (設定ファイル探索の基準パス群, UTF-8 NYsStrs)
// %USERPROFILE%\.config\nyamy, %LOCALAPPDATA%\NYamy\Config, 実行ファイルのディレクトリ
// をこの探索順で含む
// 返り値: コールバックセッションのライフタイムで管理。手動解放不要
// on_load_setting / on_exec_user_func 内で有効
NYS_API NYsStrs* nys_get_home_directories(void);

// 設定ファイル名を絶対パスに解決する (コンパイルなし)
// name: ファイル名 (NULL または "" でデフォルト .mayu を探索)
//       空の場合: レジストリ → ホームディレクトリの .mayu の順で探索
//       指定の場合: ホームディレクトリ内で検索
// out_path: 解決した絶対パス (NULL 可) — 次回本関数呼び出しまで有効
// 返り値: ファイルが見つかれば true (見つからない場合は nys_last_error() にエラーが入る)
// on_load_setting / on_exec_user_func 内で有効
NYS_API bool nys_resolve_config_path(const char*  name,
                                    const char** out_path);

// キー名またはスキャンコード文字列をスキャンコード WORD 値に解決する
// WORD は (prefix<<8)|code。prefix は 0x00(通常)/0xE0/0xE1 で、
// レジストリ Scancode Map と同じエンコード。
// str: 先行する nys_def_key で定義済みのキー名 (大文字小文字非区別、エイリアス可)、
//      またはスキャンコードリテラル ("0x1c", "E0-0x1c", "E1-0x0f", 十進 "28")。
//      キー名を優先し、未定義の場合はリテラルとして解釈する。
// 返り値: WORD 値 (0..0xE1FF)。解決不能なら -1
// on_load_setting 内でのみ有効 (キー名解決は nys_def_key 実行後に有効)
NYS_API int nys_sc_resolve(const char* str);

// キャッシュ済みレジストリ Scancode Map のエントリ数を返す
// マップは初回アクセス時に遅延読み込みされ、新しい設定ロードでクリアされる。
// Scancode Map 未設定 / 値が不正な場合は 0 を返す
NYS_API int nys_scancode_map_length(void);

// Scancode Map の 1 エントリを WORD 値で読み出す (エンコードは nys_sc_resolve 参照)
// from_word: 変換元スキャンコード, to_word: 変換先スキャンコード (0 はキー無効化)
// 両 out ポインタは NULL 可。idx が範囲外なら false を返す
NYS_API bool nys_scancode_map_entry(int idx, unsigned* from_word, unsigned* to_word);

// 最後のエラーメッセージを返す (UTF-8 NUL 終端)
// エラーなし / 未発生の場合は NULL を返す
NYS_API const char* nys_last_error(void);
```

> **C++ 専用ヘルパー (`extern "C"` 外)**
> `parseScancodeMapBlob(const unsigned char* data, size_t len, std::vector<std::pair<uint16_t,uint16_t>>& out)`
> — レジストリ "Scancode Map" の生バイナリ blob を (変換元, 変換先) WORD ペア列に
> パースする。単体テストから直接呼べるよう `NYS_API` で export している。
> blob レイアウト: header1(4) + header2(4) + count(4) + count 個の DWORD エントリ
> (末尾は null 終端なのでマッピング数は count-1)。各 DWORD は HIWORD=変換元・
> LOWORD=変換先。不正な blob では false を返し out を空にする。

### 初期化処理とイベントループ

1. scripterプロセスが起動し、`nys_start(callbacks, exeCtx)` を呼ぶと、
   環境変数 `NYS_CTRL` / `NYS_CMD` からパイプハンドルを取得して初期化処理を行なった後、
   `callbacks->on_load_setting(exeCtx)` が呼ばれる。
2. `on_load_setting` が `true` を返すと、キューイングした設定情報および CmdCommit を送信。以後コマンド要求待ち。
    - `false` を返すとキューを破棄して CmdAbort を送信。
3. Engineがユーザー定義関数呼び出し (`&ExecUserFunc` / `@func_name`) を実行すると、ctrlチャネル経由でExecUserFuncコマンドと引数・トリガーコンテキストが届く。
    scripterは関数名で `nys_reg_user_func` に登録したハンドラをルックアップし、`(exeCtx, func_name, args)` として呼び出す。
    - ユーザー定義関数内で、Engineのキーシーケンスを実行したい場合は、`nys_exec_keyseq()` を呼び出す。
      トリガーコンテキストは内部で自動的に引き継がれる。
4. Engineが終了を選択すると、`callbacks->on_quit(exeCtx)` が呼ばれた後 `nys_start()` が終了する。
    - 再読み込み時はscripterプロセス終了後、再起動される (プロセス再起動方式は未実装)。

`nys_start()` は 2 スレッドで動く。ctrl ストリームの読み出しは専用スレッドが行い、
コールバックは呼び出し元スレッドで実行される。したがって手順 1〜3 のコールバック実行中でも
Quit / EOF は即座に観測される。観測後もキュー済みのジョブは順に実行され、
`nys_set_quit_timeout()` で指定した時間内に終わらなければプロセスごと強制終了される
(詳細は [protocol.md の Quit](protocol.md#quit-0xff))。

ExecUserFunc の保留は 64 件が上限。ctrl パイプを常時 drain するようになった代わりに、
溢れた分はここで捨ててログに出す (以前はパイプが満杯になることで nyamy 側が捨てていた)。

### CmdCommit 後のメモリ管理

CmdCommit 送信後、以下の情報はプロセス終了時まで**恒久的に保持**される。

| 情報 | 理由 |
|------|------|
| **登録済み KeySeq**（`nys_reg_keyseq`）| インデックス値は Engine との間で共有される識別子であり、プロセスライフタイム中は安定している必要がある。名前→インデックスのマッピングを保持することで、重複登録を避けつつ同一インデックスを返せる。KeySeq の実データ（actions 文字列）は Engine が保持するため scripter 側では不要。 |
| **シンボル**（プロセス起動時に渡されたシンボルセット）| on_load_setting 呼び出し時に参照するため、少なくとも on_load_setting の実行期間中は保持が必要。プロセスライフタイム中に変化しないため、解放せず保持する方が実装上簡潔。 |
| **登録済みユーザー関数**（`nys_reg_user_func`）| CmdCommit 後も Engine から ExecUserFunc が随時届くため、関数名→ハンドラのマッピングを常に参照できる必要がある。 |

上記以外の設定情報構築に使用したメモリ（キーマップ定義、キー/モディファイア定義、中間コンパイル状態など）は CmdCommit 送信後に解放できる。これらは CmdStream 経由で Engine に転送済みであり、scripter 側では不要となる。

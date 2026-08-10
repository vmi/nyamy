## 2. uninstall {#UNINSTALL}

タスクトレイメニューの[終了(<u>X</u>)](#menu-x)で NYamy を終了してから、インストール先
(`%LOCALAPPDATA%\Programs\NYamy`) の `uninstall.cmd` を実行してください。確認を求められるので
`yes` と入力すると、ファイル一式とスタートアップ用ショートカット (作成していた場合) が削除されます。
確認なしで削除したい場合は `uninstall.cmd --force` を実行してください。

設定を完全に削除したい場合は、あわせて以下も削除してください。

- `%LOCALAPPDATA%\NYamy` ([設定フォルダ](#HOME)。`nyamy.ini` と自分用の設定ファイルが入っています)

NYamy はレジストリへの書き込みを行わないため、レジストリの掃除は不要です。

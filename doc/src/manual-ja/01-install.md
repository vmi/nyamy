## 1. install {#INSTALL}

配布 zip を展開し、同梱の `install.cmd` を実行してインストールします。ドライバ等は未使用なので、再起動は不要です。

1. NYamy 以外のキーボードカスタマイズソフトを利用している場合は、動作の衝突を避けるため終了 (またはアンインストール) しておくことをお勧めします。
2. 以前のバージョンの NYamy を利用している場合は、終了させます。
    - そのまま `install.cmd` を実行して上書きインストールして構いません。アンインストールする必要はありません。
3. 配布 zip をダウンロードフォルダなど任意のフォルダに展開します (一時的な場所で構いません)。
4. 展開したフォルダの `install.cmd` を実行します。
    - ファイル一式が `%LOCALAPPDATA%\Programs\NYamy` にコピーされます (`install.cmd`/`install.ps1` 自身はコピー対象に含まれません)。
    - 途中でスタートアップ (ログイン時の自動起動) 用ショートカットを作成するか尋ねられます。何も入力せず Enter を押すと作成しません。あとから作成/削除したい場合は、配布 zip を再度展開し、その中の `install.cmd --startup-only` / `install.cmd --no-startup-only` を実行してください (`install.cmd --help` でオプション一覧を表示できます)。
5. `%LOCALAPPDATA%\Programs\NYamy\nyamy.exe` を実行すると、タスクトレイに NYamy のアイコンが表示されます。
6. タスクトレイメニューの[選択(<u>C</u>) ►](#menu-c)で、キーボードに合った設定を選びます。

### 設定の保存先 {#install_config}

NYamy の設定 (選択中の設定ファイルなど) は `%LOCALAPPDATA%\NYamy\Config\nyamy.ini` に保存されます。初回起動時に `nyamy.exe` と同じフォルダにある `nyamy.ini` が雛形として取り込まれます。レジストリは使用しません。

### 管理者権限について {#install_admin}

通常の利用に管理者権限は不要です。ただし、管理者権限で実行されているアプリケーションに対してマウスイベント (`def option mouse-event`) を届けるには、NYamy 自体を管理者として実行する必要があります。

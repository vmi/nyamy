# ビルドシステム変更仕様

## 現在の構成

```
proj/
  yamy.sln                      — ソリューション
  yamy.vcxproj                  — yamy.exe (Yamy本体、x64 のみ)
  yamy32dll.vcxproj             — yamy32.dll (32bit フック DLL)
  yamy64dll.vcxproj             — yamy64.dll (64bit フック DLL)
  yamy-scripter.vcxproj         — yamy-scripter.exe (mruby 内蔵 EXE)
  makefunc.vcxproj              — ビルド時コード生成
  distrib.vcxproj               — 配布パッケージ作成
  yamyd32.vcxproj               — 32bit アプリへの yamy32.dll フック注入用
```

---

## CRT に関する注意

現状は EXE ともに `/MD` (MultiThreadedDLL) を使用している。
将来 FFI 経由で外部スクリプトが C API (`ys_*`) を使う場合は CRT 境界に注意が必要。
EXE 内部で確保したメモリは EXE 内部で解放すること。

---

## 将来の変更予定

プロジェクト名・GUID・TargetName は変更しない予定。

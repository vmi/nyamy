# ビルドシステム変更仕様

## 現在の構成

```
proj/
  yamy.sln                      — ソリューション
  yamy.vcxproj                  — yamy.exe (Yamy本体、x64 のみ)
  yamy32dll.vcxproj             — yamy32.dll (32bit フック DLL)
  yamy64dll.vcxproj             — yamy64.dll (64bit フック DLL)
  yamy-scripter-dll.vcxproj     — yamy-scripter.dll (公開 C API + .mayu コンパイラ, DynamicLibrary)
  yamy-scripter.vcxproj         — yamy-scripter.exe (mruby 内蔵 EXE, 上記 DLL を ProjectReference)
  makefunc.vcxproj              — ビルド時コード生成
  distrib.vcxproj               — 配布パッケージ作成
  yamyd32.vcxproj               — 32bit アプリへの yamy32.dll フック注入用
```

---

## DLL / EXE の関係

`yamy-scripter.dll` が公開 C API (`ys_*`) と .mayu コンパイラ本体を持つ。
`yamy-scripter.exe` は `mruby_main.cpp` / `mruby_binding.cpp` のみをコンパイルした
薄いラッパーで、`<ProjectReference>` で DLL を参照して C API を import する。
そのため EXE の実行には `yamy-scripter.dll` が必須
(`tools/makedistrib.ps1` の `$files` に列挙されており配布パッケージに同梱される)。
FFI クライアント (Python/Ruby) は EXE を介さず DLL を直接ロードする。

エクスポート切り替えは `yamy_scripter.cpp` が `#include` 前に `_YAMY_SCRIPTER_IMPL` を
定義することで行う (`YS_API` = `__declspec(dllexport)`)。DLL を import する側 (mruby ラッパーや
FFI) では未定義なので `__declspec(dllimport)` になる。

## CRT に関する注意

現状は DLL / EXE ともに `/MD` (MultiThreadedDLL) を使用している。
将来 FFI 経由で外部スクリプトが C API (`ys_*`) を使う場合は CRT 境界に注意が必要。
DLL 内部で確保したメモリは DLL 内部で解放すること。

---

## 将来の変更予定

プロジェクト名・GUID・TargetName は変更しない予定。

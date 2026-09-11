@echo off

rem 2025 Y.Hanashiro.
rem 本バッチは UnrealVersionSelector を経由せず、
rem Build.bat→UnrealBuildToolのProjectFiles処理を直接呼び出しているため、
rem 右クリック「Generate Visual Studio project files」より起動オーバーヘッドが少なく
rem （別プロセス起動（UnrealVersionSelector）やら
レジストリ検索（インストール済み UE の探索）やらいろいろ）
rem 余計なプロセス起動や待機を行わずに完了する。
rem 生成内容（.sln / .vcxproj）は従来処理と同一であり、機能差はないはず。

cd /d %~dp0
setlocal

rem ---- このフォルダ内の .uproject を 1 件拾う ----
for %%F in (*.uproject) do (
    set "UPROJECT=%%~fF"
    goto :FOUND_PROJECT
)

echo .uproject が見つかりませんでした。
pause
goto :END

:FOUND_PROJECT
echo UPROJECT = "%UPROJECT%"
echo.

rem ---- UE5.8 のルート（必要に応じてパス調整するように）----
set "UE_ROOT=C:\Program Files\Epic Games\UE_5.8"
set "BUILDBAT=%UE_ROOT%\Engine\Build\BatchFiles\Build.bat"

echo 実行:
echo "%BUILDBAT%" -projectfiles -project="%UPROJECT%" -game -progress
echo.

rem ここで Build.bat が終わるまで待つ
call "%BUILDBAT%" -projectfiles -project="%UPROJECT%" -game -progress
set "ERR=%ERRORLEVEL%"

echo.
echo ExitCode=%ERR%

rem ---- 終了後の通知 ----
if %ERR% EQU 0 (
    powershell -NoProfile -Command "Add-Type -AssemblyName PresentationFramework; [System.Windows.MessageBox]::Show('VS プロジェクトファイルの生成が完了しました。','GenerateProjectFiles','OK','Information')" >nul
) else (
    powershell -NoProfile -Command "Add-Type -AssemblyName PresentationFramework; [System.Windows.MessageBox]::Show('VS プロジェクトファイル生成に失敗しました。ExitCode=%ERR%','GenerateProjectFiles','OK','Error')" >nul
)

:END
endlocal

@echo off
rem Worlds Beyond - the Dominion redesign: rebuilds terrain, gameplay, villages, water, flora and ruins in order.
rem CLOSE THE UNREAL EDITOR FIRST. Resume from a step: run_dominion_redesign.bat 16  (or 17, 18, 19, 22, hlod, tests).
rem A pass counts as done when its log says the Python script executed successfully; the commandlet's own exit code
rem is not used, because the project's existing GameFeatureData asset-manager errors make every commandlet return 1.
setlocal
set UE="C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
set PROJ="C:\GAT360\Worlds-Beyond\WorldBeyond\WorldBeyond.uproject"
set S=C:\GAT360\Worlds-Beyond\WorldBeyond\Scripts\Migration\
set SAVED=C:\GAT360\Worlds-Beyond\WorldBeyond\Saved\MigrationBackups
set LOGS=C:\GAT360\Worlds-Beyond\WorldBeyond\Saved\Logs\Redesign
if not exist "%LOGS%" mkdir "%LOGS%"
tasklist /FI "IMAGENAME eq UnrealEditor.exe" | find /I "UnrealEditor.exe" >nul && (echo Close the Unreal Editor first. & exit /b 1)
if not "%~1"=="" goto pass%~1
:pass15
echo ==== pass 15 ====
set "BEYOND_REBUILD_LAYOUT=1" & set "BEYOND_REBUILD_TERRAIN=1" & %UE% %PROJ% -run=pythonscript -script="%S%migrate_pass15.py" -unattended -nosplash -AllowCommandletRendering -abslog="%LOGS%\pass15.log"
set "BEYOND_REBUILD_LAYOUT=" & set "BEYOND_REBUILD_TERRAIN="
findstr /C:"Python script executed successfully" "%LOGS%\pass15.log" >nul || (set "FAILED=15" & goto failed)
findstr /C:"WARNING" "%SAVED%\last_run_pass15.txt"
:pass16
echo ==== pass 16 ====
set "BEYOND_REBUILD_GAMEPLAY=1" & %UE% %PROJ% -run=pythonscript -script="%S%migrate_pass16.py" -unattended -nosplash -AllowCommandletRendering -abslog="%LOGS%\pass16.log"
set "BEYOND_REBUILD_GAMEPLAY="
findstr /C:"Python script executed successfully" "%LOGS%\pass16.log" >nul || (set "FAILED=16" & goto failed)
findstr /C:"WARNING" "%SAVED%\last_run_pass16.txt"
:pass17
echo ==== pass 17 ====
set "BEYOND_REBUILD_VILLAGES=1" & %UE% %PROJ% -run=pythonscript -script="%S%migrate_pass17.py" -unattended -nosplash -AllowCommandletRendering -abslog="%LOGS%\pass17.log"
set "BEYOND_REBUILD_VILLAGES="
findstr /C:"Python script executed successfully" "%LOGS%\pass17.log" >nul || (set "FAILED=17" & goto failed)
findstr /C:"WARNING" "%SAVED%\last_run_pass17.txt"
:pass18
echo ==== pass 18 ====
set "BEYOND_REBUILD_WATER=1" & set "BEYOND_SKIP_PICTURES=1" & %UE% %PROJ% -run=pythonscript -script="%S%migrate_pass18.py" -unattended -nosplash -AllowCommandletRendering -abslog="%LOGS%\pass18.log"
set "BEYOND_REBUILD_WATER=" & set "BEYOND_SKIP_PICTURES="
findstr /C:"Python script executed successfully" "%LOGS%\pass18.log" >nul || (set "FAILED=18" & goto failed)
findstr /C:"WARNING" "%SAVED%\last_run_pass18.txt"
:pass19
echo ==== pass 19 ====
set "BEYOND_REBUILD_FLORA=1" & set "BEYOND_SKIP_PICTURES=1" & %UE% %PROJ% -run=pythonscript -script="%S%migrate_pass19.py" -unattended -nosplash -AllowCommandletRendering -abslog="%LOGS%\pass19.log"
set "BEYOND_REBUILD_FLORA=" & set "BEYOND_SKIP_PICTURES="
findstr /C:"Python script executed successfully" "%LOGS%\pass19.log" >nul || (set "FAILED=19" & goto failed)
findstr /C:"WARNING" "%SAVED%\last_run_pass19.txt"
:pass22
echo ==== pass 22 ====
set "BEYOND_REBUILD_RUINS=1" & %UE% %PROJ% -run=pythonscript -script="%S%migrate_pass22.py" -unattended -nosplash -AllowCommandletRendering -abslog="%LOGS%\pass22.log"
set "BEYOND_REBUILD_RUINS="
findstr /C:"Python script executed successfully" "%LOGS%\pass22.log" >nul || (set "FAILED=22" & goto failed)
findstr /C:"WARNING" "%SAVED%\last_run_pass22.txt"
:passhlod
echo ==== HLODs ====
rem HLOD actors are derived data. Delete the old ones in a separate run first - the builder runs its delete step
rem after its build step - so no HLOD actor survives on an HLOD grid that a rebuilt pass has renamed.
%UE% %PROJ% /Game/WorldsBeyond/Maps/Dominion -run=WorldPartitionBuilderCommandlet -Builder=WorldPartitionHLODsBuilder -DeleteHLODs -AllowCommandletRendering -unattended -nosplash -abslog="%LOGS%\hlod_delete.log"
%UE% %PROJ% /Game/WorldsBeyond/Maps/Dominion -run=WorldPartitionBuilderCommandlet -Builder=WorldPartitionHLODsBuilder -AllowCommandletRendering -unattended -nosplash -abslog="%LOGS%\hlod.log"
echo ==== minimap ====
%UE% %PROJ% /Game/WorldsBeyond/Maps/Dominion -run=WorldPartitionBuilderCommandlet -Builder=WorldPartitionMiniMapBuilder -AllowCommandletRendering -unattended -nosplash -abslog="%LOGS%\minimap.log"
:passtests
echo ==== tests ====
%UE% %PROJ% -ExecCmds="Automation RunTests WorldsBeyond.Prototype.WorldBuilder+WorldsBeyond.Prototype.OpenWorld;Quit" -unattended -nullrhi -nosplash -abslog="%LOGS%\tests.log"
findstr /C:"Test Completed" "%LOGS%\tests.log"
echo Done. Logs: %LOGS%   Pass reports: %SAVED%\last_run_pass*.txt
exit /b 0
:failed
echo Pass %FAILED% did not finish (see %LOGS%\pass%FAILED%.log and %SAVED%\last_run_pass%FAILED%.txt).
echo Fix it, then resume with: run_dominion_redesign.bat %FAILED%
exit /b 1

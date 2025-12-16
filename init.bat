@echo off
if "%~1"=="" (set "target=x64") else (set "target=%~1")
"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" %target%
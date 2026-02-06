@echo off
if "%~1"=="" (set "target=x64") else (set "target=%~1")
"C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvarsall.bat" %target%
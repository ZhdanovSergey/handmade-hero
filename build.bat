@echo off
mkdir build
pushd build
cl -Zi ..\src\main.cpp user32.lib gdi32.lib
popd
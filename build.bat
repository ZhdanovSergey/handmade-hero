@echo off
mkdir build
pushd build
cl -std:c++20 -Wall -external:W0 -Zi ..\src\main.cpp user32.lib gdi32.lib
popd
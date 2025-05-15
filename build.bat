@echo off
mkdir build
pushd build
@REM cl -std:c++20 -Wall -external:W0 -Zi -fsanitize=address -analyze ..\src\main.cpp user32.lib gdi32.lib
cl -std:c++20 -Wall -external:W0 -Zi ..\src\main.cpp user32.lib gdi32.lib
popd
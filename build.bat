@echo off
mkdir build
pushd build
cl -std:c++20 -Zi -Wall -external:W0 -RTC1 -RTCc %1 ..\src\main.cpp user32.lib gdi32.lib
popd
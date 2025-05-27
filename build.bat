@echo off
IF NOT EXIST build mkdir build
pushd build
cl -std:c++20 -Zi -Wall -external:W0 -RTC1 -RTCc %1 ..\src\win32_handmade.cpp user32.lib gdi32.lib
popd
@echo off
IF NOT EXIST build mkdir build
pushd build
@REM TODO: what about custom flags?
cl -DHANDMADE_INTERNAL=1 -DHANDMADE_SLOW=1 -std:c++20 -Zi -Wall -external:W0 -EHsc -RTC1 -RTCc -D_ALLOW_RTCc_IN_STL %1 ..\src\win32_handmade.cpp user32.lib gdi32.lib
popd
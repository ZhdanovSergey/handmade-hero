@echo off
IF NOT EXIST build mkdir build
pushd build
cl -DHANDMADE_DEV=1 -DHANDMADE_SLOW=1 -std:c++20 -GR- -EHa- -wd4577 -wd4820 -Zi -Oi -Wall -external:W0 -RTC1 -RTCc -D_ALLOW_RTCc_IN_STL -nologo %1 ..\src\win32_handmade.cpp user32.lib gdi32.lib -link -opt:ref
popd
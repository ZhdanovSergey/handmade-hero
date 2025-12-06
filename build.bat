@echo off
IF NOT EXIST build mkdir build
pushd build
cl -DHANDMADE_DEV=1 -DHANDMADE_SLOW=1 -std:c++20 -GR- -EHa- -wd4505 -wd4577 -wd4820 -wd5045 -Zi -Oi -Wall -external:W0 -RTC1 -RTCc -D_ALLOW_RTCc_IN_STL -nologo %1 ..\src\win32_handmade.cpp gdi32.lib user32.lib winmm.lib -link -opt:ref
popd
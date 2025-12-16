@echo off

if "%VSCMD_ARG_TGT_ARCH%"=="x86" (set "subsystem=-subsystem:windows,5.1") else (set "subsystem=")

IF NOT EXIST build mkdir build
pushd build

@REM TODO: устанавливать флаги компиляции в зависимости от dev/slow режима
cl -DDEV_MODE=1 -DSLOW_MODE=1 ^
    -std:c++20 -MT -GR- -EHa- -Zi -Oi -RTC1 ^
    -Wall -external:W0 -wd4191 -wd4505 -wd4577 -wd4820 -wd5045 -nologo ^
    ..\src\win32_handmade.cpp gdi32.lib user32.lib winmm.lib -link -opt:ref %subsystem%

popd
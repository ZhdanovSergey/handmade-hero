@echo off

if %VSCMD_ARG_TGT_ARCH%=="x86" (set subsystem="-subsystem:windows,5.1")

@REM TODO: устанавливать флаги компиляции в зависимости от dev/slow режима
set common_flags=-DDEV_MODE=1 -DSLOW_MODE=1^
    -std:c++20 -MT -GR- -EHa- -Zi -Fm -Oi -RTC1 ^
    -Wall -external:W0 -wd4100 -wd4191 -wd4505 -wd4514 -wd4577 -wd4820 -wd5045 -nologo

IF NOT EXIST build mkdir build
pushd build
cl %common_flags% -LD ..\src\win32_dll_main.cpp -link -opt:ref %subsystem% -EXPORT:update_and_render -EXPORT:get_sound_samples
cl %common_flags% ..\src\win32_handmade.cpp gdi32.lib user32.lib winmm.lib -link -opt:ref %subsystem%
popd
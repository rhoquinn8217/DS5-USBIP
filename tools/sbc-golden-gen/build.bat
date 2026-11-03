@echo off
rem Builds sbc-golden-gen.exe into the folder this is run from. Run it from a
rem Visual Studio x64 developer prompt. main.cpp says why this needs FFmpeg 8.0.
rem   set FFMPEG_ROOT=<a checkout's third_party\ffmpeg\x64\release>
rem   tools\sbc-golden-gen\build.bat
if "%FFMPEG_ROOT%"=="" (
    echo Set FFMPEG_ROOT first. main.cpp says where it comes from.
    exit /b 2
)
set REPO=%~dp0..\..
cl /nologo /O2 /EHsc /MT /std:c++17 /utf-8 /W3 ^
    /I "%REPO%\third_party" /I "%REPO%\tests" /I "%~dp0." /I "%FFMPEG_ROOT%\include" ^
    "%~dp0main.cpp" "%~dp0ffmpeg_side.cpp" ^
    "%REPO%\third_party\sbc\sbc.c" "%REPO%\third_party\sbc\sbcdsp.c" "%REPO%\third_party\sbc\sbcenc.c" ^
    /Fe:sbc-golden-gen.exe ^
    /link /LIBPATH:"%FFMPEG_ROOT%\lib" avcodec.lib avutil.lib

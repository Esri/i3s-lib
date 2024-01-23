
@echo off

call "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" 

set CMAKE=cmake.exe
set NINJA=ninja.exe
@rem set CC=cl.exe
@rem set CXX=cl.exe

@echo Building Debug configuration of i3slib...
set BUILD_DIR_DEBUG=%~dp0../build/x64-Debug
%CMAKE% %~dp0.. -B %BUILD_DIR_DEBUG% -G Ninja -DCMAKE_MAKE_PROGRAM=%NINJA% -DCMAKE_BUILD_TYPE="Debug" -DCMAKE_INSTALL_PREFIX=%BUILD_DIR_DEBUG%/install
%CMAKE% --build %BUILD_DIR_DEBUG%

@echo Building Release configuration of i3slib...
set BUILD_DIR_RELEASE=%~dp0../build/x64-Release
%CMAKE% %~dp0.. -B %BUILD_DIR_RELEASE% -G Ninja -DCMAKE_MAKE_PROGRAM=%NINJA% -DCMAKE_BUILD_TYPE="Release" -DCMAKE_INSTALL_PREFIX=%BUILD_DIR_RELEASE%/install
%CMAKE% --build %BUILD_DIR_RELEASE%


@echo off


call "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" 
set PATH=%PATH%;C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Tools\Llvm\bin

set CMAKE=cmake.exe
set NINJA=ninja.exe
@rem set CMAKE="C:/PROGRAM FILES/MICROSOFT VISUAL STUDIO/2022/PROFESSIONAL/COMMON7/IDE/COMMONEXTENSIONS/MICROSOFT/CMAKE/CMake/bin/cmake.exe"
@rem set NINJA="C:/PROGRAM FILES/MICROSOFT VISUAL STUDIO/2022/PROFESSIONAL/COMMON7/IDE/COMMONEXTENSIONS/MICROSOFT/CMAKE/Ninja/ninja.exe"

set CC=clang-cl.exe
set CXX=clang-cl.exe

set CFLAGS=-m64
set CXXFLAGS=-m64

@rem set BUILD_DIR_RELEASE=%~dp0../build/x64-Clang-Release
@rem %CMAKE% --build %BUILD_DIR_RELEASE%
@rem exit

@echo Building Debug configuration of i3slib...
set BUILD_DIR_DEBUG=%~dp0../build/x64-Clang-Debug
%CMAKE% %~dp0.. -B %BUILD_DIR_DEBUG% -G Ninja -DCMAKE_MAKE_PROGRAM=%NINJA% -DCMAKE_BUILD_TYPE="Debug" -DCMAKE_INSTALL_PREFIX=%BUILD_DIR_DEBUG%/install
%CMAKE% --build %BUILD_DIR_DEBUG%

@echo Building Release configuration of i3slib...
set BUILD_DIR_RELEASE=%~dp0../build/x64-Clang-Release
%CMAKE% %~dp0.. -B %BUILD_DIR_RELEASE% -G Ninja -DCMAKE_MAKE_PROGRAM=%NINJA% -DCMAKE_BUILD_TYPE="Release" -DCMAKE_INSTALL_PREFIX=%BUILD_DIR_RELEASE%/install
%CMAKE% --build %BUILD_DIR_RELEASE%

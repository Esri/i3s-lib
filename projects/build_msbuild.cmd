
@echo off

call "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" 

@echo Building Debug configuration of i3slib...
MSBuild.exe i3s.sln /p:Configuration=Debug /p:Platform=x64 /t:Build

@echo Building Release configuration of i3slib...
MSBuild.exe i3s.sln /p:Configuration=Release /p:Platform=x64 /t:Build

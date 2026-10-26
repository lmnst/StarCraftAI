@echo off
setlocal
for %%I in ("%~dp0..") do set "PROJECT_ROOT=%%~fI"
set "MSBUILD=%~dp0VSBuildTools\MSBuild\Current\Bin\MSBuild.exe"
"%MSBUILD%" "%PROJECT_ROOT%\src\STARTcraft\visualstudio\STARTcraft.sln" /m:1 /nr:false /p:Configuration=Release /p:Platform=Win32 /p:PlatformToolset=v143 /p:WindowsTargetPlatformVersion=10.0.22621.0 /verbosity:minimal /nologo
if errorlevel 1 goto failed
"%MSBUILD%" "%PROJECT_ROOT%\src\UAlbertaBot\UAlbertaBot\VisualStudio\UAlbertaBot.sln" /t:UAlbertaBot /m:1 /nr:false /p:Configuration=Release /p:Platform=Win32 /p:PlatformToolset=v143 /p:WindowsTargetPlatformVersion=10.0.22621.0 "/p:BWAPI_DIR=%PROJECT_ROOT%\src\STARTcraft\src\bwapi" /verbosity:minimal /nologo
if errorlevel 1 goto failed
"%MSBUILD%" "%PROJECT_ROOT%\tools\LocalDemoLauncher.vcxproj" /m:1 /nr:false /p:Configuration=Release /p:Platform=Win32 /verbosity:minimal /nologo
if errorlevel 1 goto failed
"%MSBUILD%" "%PROJECT_ROOT%\tools\MatchLauncher.vcxproj" /m:1 /nr:false /p:Configuration=Release /p:Platform=Win32 /verbosity:minimal /nologo
if errorlevel 1 goto failed
echo Both bots and both launchers built successfully.
exit /b 0
:failed
echo Build failed. Review the output above.
exit /b 1
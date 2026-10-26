@echo off
setlocal
for %%I in ("%~dp0..") do set "PROJECT_ROOT=%%~fI"
set "MSBUILD="
if defined MSBUILD_EXE_PATH if exist "%MSBUILD_EXE_PATH%" set "MSBUILD=%MSBUILD_EXE_PATH%"
if not defined MSBUILD if exist "%~dp0VSBuildTools\MSBuild\Current\Bin\MSBuild.exe" set "MSBUILD=%~dp0VSBuildTools\MSBuild\Current\Bin\MSBuild.exe"
if not defined MSBUILD for /f "delims=" %%I in ('where MSBuild.exe 2^>nul') do if not defined MSBUILD set "MSBUILD=%%I"
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not defined MSBUILD if exist "%VSWHERE%" for /f "usebackq delims=" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -find MSBuild\**\Bin\MSBuild.exe`) do if not defined MSBUILD set "MSBUILD=%%I"
if not defined MSBUILD (
    echo Install Visual Studio 2022 Build Tools with Desktop development with C++ and a Windows SDK.
    echo Or set MSBUILD_EXE_PATH to an existing MSBuild.exe.
    exit /b 1
)
if not exist "%PROJECT_ROOT%\src\STARTcraft\visualstudio\STARTcraft.sln" goto sources_missing
if not exist "%PROJECT_ROOT%\src\UAlbertaBot\UAlbertaBot\VisualStudio\UAlbertaBot.sln" goto sources_missing
set "BUILD_OPTIONS=/m:1 /nr:false /p:Configuration=Release /p:Platform=Win32 /p:PlatformToolset=v143 /p:WindowsTargetPlatformVersion=10.0 /verbosity:minimal /nologo"
"%MSBUILD%" "%PROJECT_ROOT%\src\STARTcraft\visualstudio\STARTcraft.sln" %BUILD_OPTIONS%
if errorlevel 1 goto failed
"%MSBUILD%" "%PROJECT_ROOT%\src\UAlbertaBot\UAlbertaBot\VisualStudio\UAlbertaBot.sln" /t:UAlbertaBot %BUILD_OPTIONS% "/p:BWAPI_DIR=%PROJECT_ROOT%\src\STARTcraft\src\bwapi"
if errorlevel 1 goto failed
"%MSBUILD%" "%PROJECT_ROOT%\tools\LocalDemoLauncher.vcxproj" %BUILD_OPTIONS%
if errorlevel 1 goto failed
"%MSBUILD%" "%PROJECT_ROOT%\tools\MatchLauncher.vcxproj" %BUILD_OPTIONS%
if errorlevel 1 goto failed
echo Both bots and both launchers built successfully.
exit /b 0
:sources_missing
echo Run tools\Setup-Sources.ps1 first.
exit /b 1
:failed
echo Build failed. Review the output above.
exit /b 1
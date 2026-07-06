@echo off
setlocal

set "CONFIG=%~1"
if "%CONFIG%"=="" set "CONFIG=Debug"

set "PLATFORM=x64"
set "SCRIPT_DIR=%~dp0"
set "PROJECT=%SCRIPT_DIR%QTDough.vcxproj"
set "SHADER_COMPILE=%SCRIPT_DIR%src\shaders\Compile.bat"

if not exist "%SHADER_COMPILE%" (
    echo Shader compile script not found at "%SHADER_COMPILE%"
    exit /b 1
)

echo === Compiling shaders ===
call "%SHADER_COMPILE%"
if errorlevel 1 (
    echo Shader compile reported an error.
    exit /b 1
)

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo vswhere.exe not found at "%VSWHERE%". Install Visual Studio 2017 or later.
    exit /b 1
)

set "VS_INSTALL="
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -property installationPath`) do (
    set "VS_INSTALL=%%i"
)

if not defined VS_INSTALL (
    echo No Visual Studio installation found by vswhere.
    exit /b 1
)

set "VCVARS=%VS_INSTALL%\VC\Auxiliary\Build\vcvarsall.bat"
if not exist "%VCVARS%" (
    echo vcvarsall.bat not found at "%VCVARS%"
    exit /b 1
)

call "%VCVARS%" %PLATFORM%
if errorlevel 1 (
    echo vcvarsall.bat failed.
    exit /b 1
)

echo === MSBuild (%CONFIG% ^| %PLATFORM%) ===
msbuild "%PROJECT%" /p:Configuration=%CONFIG% /p:Platform=%PLATFORM% /m /v:minimal /nologo
if errorlevel 1 exit /b %errorlevel%

echo === Launching QTDough.exe ===
pushd "%SCRIPT_DIR%"
"%SCRIPT_DIR%QTDough.exe"
set "RUN_EXITCODE=%errorlevel%"
popd
exit /b %RUN_EXITCODE%

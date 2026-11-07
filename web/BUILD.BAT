@echo off
rem the radar in a web page: core\ built for WebAssembly with Emscripten, into twin.js and twin.wasm here
rem   BUILD.BAT      Emscripten from C:\emsdk, or set EMSDK=path
rem what goes in is in emcc.args, the site's workflow builds from the same file
rem then serve this folder with any web server: a page opened from a file can't load the wasm
setlocal
if "%EMSDK%"=="" set EMSDK=C:\emsdk
if not exist "%EMSDK%\upstream\emscripten\emcc.exe" (
  echo Emscripten not found in %EMSDK%
  exit /b 1
)
rem its own python and settings, not whatever else is on this PC
set "PATH=%EMSDK%\upstream\emscripten;%PATH%"
set "EM_CONFIG=%EMSDK%\.emscripten"
for /d %%P in ("%EMSDK%\python\*") do set "EMSDK_PYTHON=%%P\python.exe"
cd /d "%~dp0"
rem the version, as the firmware takes it: the first "## " line of CHANGES.md
set VER=
for /f "tokens=2" %%v in ('findstr /b /c:"## " ..\CHANGES.md') do if not defined VER set VER=%%v
call emcc @emcc.args -DFLIGHT_VERSION=\"%VER%\"
if errorlevel 1 exit /b 1
echo built twin.js and twin.wasm, version %VER%

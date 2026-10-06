@echo off
setlocal
REM Match configure.bat and allow a build directory selected by bootstrap.
if not defined BUILD_DIR set "BUILD_DIR=..\build"
cmake --build "%BUILD_DIR%" --config RelWithDebInfo --parallel --target install %*
exit /b %ERRORLEVEL%

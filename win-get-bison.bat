cmake -P win-get-bison.cmake
@if errorlevel 1 exit /b 1
set PATH=%PATH%;%CD%\..\bison-2.4.1\bin

@echo off
setlocal enabledelayedexpansion

:loop
set /p "searchStr=>"

if /i "!searchStr!"=="exit" (
    goto :end
)

if "!searchStr!"=="" (
    goto :loop
)

findstr /s /i /p /c:"!searchStr!" *.*

if errorlevel 1 (
    echo No matches found.
)

goto :loop

:end
endlocal

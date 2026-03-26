@echo off
setlocal enabledelayedexpansion

set NAME=%1

if /i "%NAME:~-4%"==".ttf" set NAME=%NAME:~0,-4%

for /f "tokens=1 delims=_" %%A in ("%NAME%") do set HEIGHT=%%A

IF EXIST %NAME%_0.png    ( del %NAME%_0.png )
IF EXIST %NAME%.png      ( del %NAME%.png )
IF EXIST %NAME%.fnt      ( del %NAME%.fnt )
IF EXIST %NAME%.fontjson ( del %NAME%.fontjson )

C:\Code\JC\Bin\fontbm.exe --font-file %NAME%.ttf --font-size %HEIGHT% --output %NAME% --monochrome --chars 32-255 --spacing-vert 1 --spacing-horiz 1
ren %NAME%_0.png %NAME%.png
C:\Code\JC\Bin\fnt2fontjson.exe %NAME%.fnt %NAME%.fontjson

IF EXIST %NAME%.fnt      ( del %NAME%.fnt )
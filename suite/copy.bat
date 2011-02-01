REM both xcopy and copy require backward slashes
@echo off
set dst=%2
set dst=%dst:/=\%
copy %1 %dst%

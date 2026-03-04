@echo off
for %%f in (*.fontjson) do (
    fontgrid.exe "%%~nf"
)
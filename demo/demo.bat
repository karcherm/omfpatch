@echo off
REM demo.in isn't called demo.bin to allow "*.bin" in .gitignore
copy demo.in demo1.bin
tasm demo1.asm
..\omfpatch demo1.bin demo.map demo1.obj
type demo1.bin
echo.
type demo1.exp
echo.

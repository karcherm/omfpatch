@echo off
tasm /t/l printno.asm
tlink /x/t printno
printno

tasm /t patchyes.asm
..\omfpatch printno.com printno.map patchyes.obj
printno

tasm /t britain.asm
..\omfpatch printno.com printno.map britain.obj
printno
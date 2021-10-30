#!/bin/bash
jwasm -q -Fodemo1.obj demo1.asm
cp demo.in demo1.bin
../omfpatch demo1.bin demo.map demo1.obj
echo -n "OLD: "
cat demo.in
echo
echo -n "NEW: "
cat demo1.bin
echo
echo -n "EXP: "
cat demo1.exp
cmp demo1.bin demo1.exp

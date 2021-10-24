omfpatch - Use Intel/Microsoft .OBJ files as binary diffs
=========================================================

Overview
--------

This tool makes it possible to use MASM / TASM / JWasm / nasm as tool to write patches to existing binaries (in a limited way). It is intended for small patches that don't need recompilation of the tools. You use it in the following way:

1. Write a MAP file that tells omfpatch what file ranges contain which segments. Each line of the MAP file describes one segment, and contains in order: The name of the segment, the base offset in the file, the logical offset of the first byte stored in the file and the logical offset of the last byte stored in the file. See the .COM file example below for information about logical offsets.

2. Assemble a file with patches into the OMF format. That format is the standard .OBJ file format used by DOS development tools. Place patches into segments by naming the segments like they are named in the MAP file. Select the position to patch using the ORG directive. Write code/data to be written to the areas to be patched.

3. If you want to name locations in segments that are not included in the file, declare them as "AT xxx". This suppresses the warning that you have a segment that is not included in the MAP file. These segments must not contain initialized data, but using "dw ?" (MASM syntax) or "resb" (nasm syntax) is fine.

4. run omfpatch, and pass the binary file to patch, the MAP file and the OBJ file in this order as parameters.

Please not that omfpatch is *not* a linker. Many features of the OBJ format meant for linking are unsupported. You need to assemble the whole patch in one go into one OBJ file. omfpatch does not resolve any symbols, or apply any fixups. omfpatch actively checks that there are no relevant fixups in the object file. omfpatch allows "add segment start address" relocations, and just ignores them. This is fine, because this type of relocation is meant to add the shift induced by other components of the same segment coming from other objects files that contain fragments for the same segments. If something seems fishy, omfpatch errors out with a slightly helpful error message - you might need an OMF dumping tool and a debugger to find out the real cause of any problem.

Map file example 1 (DOS COM file)
---------------------------------

For a DOS COM file having 1024 (0x400) bytes, use something like

    MAIN    0   0x100  0x4ff

this MAP file tells omfpatch that the segment main is located at file offset 0, and the data at that location starts appearing at offset 0x100 in memory. As the file is 0x400 bytes long, the last byte of the file appears at offset 0x4ff.

Map file example 2 (BIOS image)
-------------------------------

For a classic (uncompressed) 128KB BIOS image, this kind of MAP file might be useful

    ESEG       0 0 0xffff
    FSEG 0x10000 0 0xffff

This declares two 64KB segments, named ESEG and FSEG, located at the beginning of the file, and 64KB into the file.

Patch example
-------------

Suppose we have this simple COM program (supplied in printno.asm):

```asm
CODE SEGMENT
    org 100h
entry:
    mov DX, offset message
    mov AH, 9
    int 21h
    ret
message  db "The answer is no!", 0dh, 0ah, "$"
CODE ENDS
END entry
```

The list file (generated by TASM) will look like this:

```
      1	0000			     CODE SEGMENT
      2					 org 100h
      3	0100			     entry:
      4	0100  BA 0108r			 mov dx, OFFSET	message
      5	0103  B4 09			 mov ah, 9
      6	0105  CD 21			 int 21h
      7	0107  C3			 ret
      8	0108  54 68 65 20 61 6E	73+  message db	"The answer is no!", 0dh, 0ah, "$"
      9	      77 65 72 20 69 73	20+
     10	      6E 6F 21 0D 0A 24
     11	011C			     CODE ENDS
     12				     END  entry
```

As you see, the message ends before offset 11C, and "no!" starts 6 characters before the end of the message. Let's start with a simple patch that replaces "no!" by yes, saved as patchyes.asm:

```
MAIN SEGMENT
    org 11Ch - 6
    db "yes"
MAIN ENDS
END
```

And a MAP file (printno.map) like this:

```
MAIN 0 0x100 0x11C
```

after assembling this file, you can run "omfpatch printno.com printno.map patchyes.obj" to change the message.

In case you like little britain, the message might seem a bit verbose, and you might want to skip the article at the beginning. You can do that using this patch file, supplied as britain.asm:

```
MAIN SEGMENT
    org 100h
    mov dx, OFFSET message + 4 ; skip 4 characters
    org 108h
message LABEL NEAR
MAIN ENDS
END
```

The point of this example is to show you the main point of the tool: You can write patches to assembly code using assembly language. You can add labels to addresses you don't intend to patch. You can do address arithmetics in the patch.

Checksum adjustment
-------------------

omfpatch supports the `!CHKSUM` directive in the MAP file. If a line starts with `!CHKSUM`, the remaining part of the line contains the algorithm (a single word), the offset of the first byte to include in the checksum, the offset of the last byte to include in the checksum and the offset of the first byte of the checksum location.

Currently, the only supported checksum algorithm is `SUM8`, which ensures that the sum of all bytes in the range (plus the checksum byte if it is outside of the range) have an 8-bit sum of zero. This is the algorithm used by the BIOS to validate expansion ROMS, for example. A map file for a classic 32K VGA ROM might look like this:

```
MAIN 0 0 0x7FFF
!CHKSUM SUM8 0 0x7FFF 0x7FFF
```

This will overwrite the last byte (offset 0x7fff) with a value that makes the whole file have a 8-bit checksum of zero.
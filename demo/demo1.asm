litest STRUC
    field1 dw ?
    field2 db ?
litest ENDS

SEG1 SEGMENT
     org 100h
     db "ZYX"
SEG1 ENDS

SEG2 SEGMENT
     litest 3 dup (<3131h,40h>)
SEG2 ENDS
END
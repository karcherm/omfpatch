CODE SEGMENT
    org 100h
entry:
    mov dx, OFFSET message
    mov ah, 9
    int 21h
    ret
message db "The answer is no!", 0dh, 0ah, "$"
CODE ENDS
END  entry
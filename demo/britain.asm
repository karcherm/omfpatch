MAIN SEGMENT
    org 100h
    mov dx, OFFSET message + 4 ; skip 4 characters
    org 108h
message LABEL NEAR
MAIN ENDS
END

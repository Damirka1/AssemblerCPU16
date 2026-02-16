MOV SP, 0x1000
CALL my_func
HLT

my_func:
    PUSH R1
    MOV R1, 0xFFFF
    MOV R2, [SP + 2]
    POP R1
    RET
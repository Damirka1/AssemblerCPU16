; TinyC v1.2 for CPU16
.DATA

.CODE
.ORG 0x0000
    MOV SP, 0x0100
    MOV BP, 0x0100
    CALL main
    HLT
main:
    PUSH BP
    MOV BP, SP
    SUB SP, 2 ; Alloc local x
    SUB SP, 2 ; Alloc local y
    SUB SP, 2 ; Alloc local color
    SUB SP, 2 ; Alloc local i
    SUB SP, 2 ; Alloc local j
    MOV R0, 65535
    MOV [BP + -6], R0
L0:
    MOV R0, 1
    CMP R0, 0
    JZ L1
    MOV R2, 16
    MOV R0, 0
    OUT R2, R0
    MOV R2, 17
    OUT R2, R0
    MOV R2, 18
    MOV R0, 0
    MOV [BP + -4], R0
L2:
    MOV R0, [BP + -4]
    PUSH R0
    MOV R0, 240
    MOV R1, R0
    POP R0
    CMP R0, R1
    JGE L4
    JMP L5
L3:
    MOV R0, [BP + -4]
    INC R0
    MOV [BP + -4], R0
    JMP L2
L5:
    MOV R0, 0
    MOV [BP + -2], R0
L6:
    MOV R0, [BP + -2]
    PUSH R0
    MOV R0, 320
    MOV R1, R0
    POP R0
    CMP R0, R1
    JGE L8
    JMP L9
L7:
    MOV R0, [BP + -2]
    INC R0
    MOV [BP + -2], R0
    JMP L6
L9:
    MOV R0, [BP + -6]
    OUT R2, R0
    JMP L7
L8:
    JMP L3
L4:
    MOV R0, [BP + -6]
    PUSH R0
    MOV R0, 65535
    MOV R1, R0
    POP R0
    CMP R0, R1
    JNZ L10
    MOV R0, 63488
    MOV [BP + -6], R0
    JMP L11
L10:
    MOV R0, 65535
    MOV [BP + -6], R0
L11:
    MOV R0, 0
    MOV [BP + -8], R0
L12:
    MOV R0, [BP + -8]
    PUSH R0
    MOV R0, 100
    MOV R1, R0
    POP R0
    CMP R0, R1
    JGE L14
    JMP L15
L13:
    MOV R0, [BP + -8]
    INC R0
    MOV [BP + -8], R0
    JMP L12
L15:
    MOV R0, 0
    MOV [BP + -10], R0
L16:
    MOV R0, [BP + -10]
    PUSH R0
    MOV R0, 30000
    MOV R1, R0
    POP R0
    CMP R0, R1
    JGE L18
    JMP L19
L17:
    MOV R0, [BP + -10]
    INC R0
    MOV [BP + -10], R0
    JMP L16
L19:
    JMP L17
L18:
    JMP L13
L14:
    JMP L0
L1:
main_EXIT:
    MOV SP, BP
    POP BP
    RET

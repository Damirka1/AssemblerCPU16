; TinyC v2.8 for CPU16
.DATA
results: .RESW 5
size: .WORD 5

.CODE
.ORG 0x0000
    MOV SP, 0xFFFE
    MOV BP, 0xFFFE
    CALL main
    HLT
factorial:
    PUSH BP
    MOV BP, SP
    MOV R0, [BP + -4]
    PUSH R0
    MOV R0, 1
    MOV R1, R0
    POP R0
    CMP R0, R1
    JG L0
    MOV R0, 1
    JMP factorial_EXIT
L0:
    MOV R0, [BP + -4]
    PUSH R0
    MOV R0, [BP + -4]
    PUSH R0
    MOV R0, 1
    MOV R1, R0
    POP R0
    SUB R0, R1
    PUSH R0
    CALL factorial
    ADD SP, 2
    MOV R1, R0
    POP R0
    MUL R0, R1
    JMP factorial_EXIT
factorial_EXIT:
    MOV SP, BP
    POP BP
    RET
main:
    PUSH BP
    MOV BP, SP
    SUB SP, 2 ; Alloc local i
    SUB SP, 2 ; Alloc local total_sum
    MOV R0, 0
    MOV [BP + -4], R0
    SUB SP, 2 ; Alloc local fact_res
    MOV R0, 5
    PUSH R0
    CALL factorial
    ADD SP, 2
    MOV [BP + -6], R0
    MOV R0, 0
    MOV [BP + -2], R0
L2:
    MOV R0, [BP + -2]
    PUSH R0
    MOV R0, [size]
    MOV R1, R0
    POP R0
    CMP R0, R1
    JGE L4
    JMP L5
L3:
    MOV R0, [BP + -2]
    INC R0
    MOV [BP + -2], R0
    JMP L2
L5:
    SUB SP, 2 ; Alloc local val
    MOV R0, [BP + -2]
    PUSH R0
    MOV R1, results
    POP R0
    ADD R1, R0
    ADD R1, R0
    MOV R0, [R1]
    MOV [BP + -8], R0
    MOV R0, [BP + -4]
    PUSH R0
    MOV R0, [BP + -8]
    MOV R1, R0
    POP R0
    ADD R0, R1
    MOV [BP + -4], R0
    JMP L3
L4:
    MOV R0, [BP + -4]
    PUSH R0
    MOV R0, [BP + -6]
    MOV R1, R0
    POP R0
    ADD R0, R1
    JMP main_EXIT
main_EXIT:
    MOV SP, BP
    POP BP
    RET

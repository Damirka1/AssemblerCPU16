; =============================================================
; COMPLEX CPU TEST
; Задача:
; 1. Сгенерировать последовательность Фибоначчи.
; 2. Сохранить её в RAM.
; 3. Прочитать из RAM и подсчитать сумму.
; 
; Вывод:
; R1 - Текущее число Фибоначчи (Этап 1)
; R2 - Накопленная сумма (Этап 2)
; =============================================================

; --- Константы ---
CONST LIMIT    13      
CONST RAM_PTR  0x0100  
CONST STEP     2       ; Шаг для 16-битной памяти
CONST ZERO     0       

.ORG 0x0000
    JMP start

start:
    MOV R6, ZERO        
    MOV R7, STEP        ; Регистр с шагом 2
    MOV R8, RAM_PTR     
    
    MOV R0, 0           ; Fib(i-2)
    MOV R1, 1           ; Fib(i-1)
    
    MOV R5, LIMIT       
    
    ; Записываем 0
    MOV [R8], R0        
    ADD R8, R7          ; R8 = R8 + 2 (Переход к следующему слову)
    
    ; Записываем 1
    MOV [R8], R1        
    ADD R8, R7          ; R8 = R8 + 2
    
    DEC R5
    DEC R5

loop_gen:
    MOV R3, R0
    ADD R3, R1
    
    MOV R0, R1
    MOV R1, R3          
    
    MOV [R8], R1        
    ADD R8, R7          ; R8 = R8 + 2
    
    DEC R5
    CMP R5, R6          
    JZ start_sum        
    JMP loop_gen        

; =============================================================
; ЭТАП 2: Чтение и сумма
; =============================================================
start_sum:
    MOV R8, RAM_PTR     
    MOV R5, LIMIT       
    MOV R2, 0           
    
loop_sum:
    MOV R4, [R8]        
    ADD R2, R4          
    
    ADD R8, R7          ; R8 = R8 + 2
    
    DEC R5
    CMP R5, R6          
    JZ finish           
    JMP loop_sum

finish:
    HLT                 
    JMP finish
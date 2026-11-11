; Registers: SET, MOV, LOD and STR
SET A 42    ; A = 42
MOV B, A    ; B = A
LOD B       ; push B
PSH 8
SUB         ; 42 - 8 = 34
STR C       ; C = 34 (popped)
LOD C
POP         ; prints 34
HLT

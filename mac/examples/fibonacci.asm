; Print the first 10 Fibonacci numbers.  A, B = current pair, C = counter
        SET A 0
        SET B 1
        SET C 10
loop:   LOD A
        POP         ; print A
        LOD A
        LOD B
        ADD         ; A + B
        MOV A, B    ; A = B
        STR B       ; B = old A + old B
        LOD C
        PSH 1
        SUB
        DUP
        STR C
        JNZ loop
        HLT

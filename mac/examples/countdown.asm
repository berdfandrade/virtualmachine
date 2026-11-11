; Count down from 5 to 1 using a loop and a conditional jump
        SET A 5
loop:   LOD A
        POP         ; print A
        LOD A
        PSH 1
        SUB
        DUP
        STR A       ; A = A - 1
        JNZ loop    ; keep going while A != 0
        HLT

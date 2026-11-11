; Greatest common divisor of 1071 and 462 (Euclid). Prints 21.
        SET A 1071
        SET B 462
loop:   LOD B
        JZ done      ; while B != 0
        LOD A
        LOD B
        MOD          ; A % B
        MOV A, B     ; A = B
        STR B        ; B = A % B
        JMP loop
done:   LOD A
        POP
        HLT

; Compute 10! iteratively.  A = counter, B = accumulator
        SET A 10
        SET B 1
loop:   LOD A
        JZ done     ; stop when the counter reaches 0
        LOD B
        LOD A
        MUL
        STR B       ; B = B * A
        LOD A
        PSH 1
        SUB
        STR A       ; A = A - 1
        JMP loop
done:   LOD B
        POP         ; prints 3628800
        HLT

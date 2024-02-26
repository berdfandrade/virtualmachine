#include <stdio.h>
#include <stdbool.h>

typedef enum {
    PSH,
    ADD,
    POP,
    SET,
    HLT
} InstructionSet; 

int ip = 0;

const int program[] = {
    PSH, 5,
    PSH, 6,
    ADD,
    POP,
    HLT
};
int fetch(){
    return program[ip];
}

int main(){
    int instr = program[ip];
    return 0;
}
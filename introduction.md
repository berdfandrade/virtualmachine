# Introduction 

### Introduction set 

We'll be implementing our own instruction set, it will relative simple. I'll briefly ccover instructions like moving values from registers, or jumping to other instructions, butt hopefully you'll figure this out after (...)

Our virtual machine have a set of registers `A`, `B`, `C`, `D`, `E` and `F`. These are general purpose registers, which means that they can be used for storing anythin. This is as opposed to say speciial purpose registers, for example on x86: `ip`, `flag`, `ds`,...

A program will be a read-only sequence of instructions. The virtual machine is a stack-based virtual machine, whcih means that it has a stack we can push and pop values to, and a few registers we can use too. There are also a lot more simpler to implement that a register-based virtual machine,

Without futher ado, here's an example of the instruction set we're going to be implement in action. The semi-colons are comments on what the line will do. 

```assembly
PSH 5       ; pushes 5 to the stack
PSH 10      ; pushes 10 to the stack
ADD         ; pops two values on top of the stack, adds them pushes to stack
POP         ; pops the value on the stack, will also print it for debugging
SET A 0     ; sets register A to 0
HLT         ; stop the program

```

That's our instruction set, note thatn the POP instruction will print the instruction we popped, this is more of a debugging thing (ADD will push a result to the stackm se we can POP the value from the stack to verify it is there).
I've also included a SET instruction, this is so you understand how registers can be accessed and written to. You can also try your hand at implementing instructions like`MOV A, B` (move the value A to B). `HLT` is the instruction to show we've finished executing the program. 



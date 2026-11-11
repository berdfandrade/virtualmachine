# O que é uma Stack?

A "stack" em ciência da computação é uma estrutura de dados fundamental que opera seguindo o princípio LIFO (Last in, First Out), ou seja, o último elemento inserido é o primeiro a ser removido.
Funciona de maneira semelhante a umapilha de pratos: Você pode adicionar pratos no topo da pilha e remover apenas o prato de mais recentemente adicionado.

Neste código C que escrevemos, há uma implementação bem, simples de uma máquina vitual (VM) que opera com uma pilha. Aqui está uma explicação linha por linha do que o código está fazendo:

#### 1 - Declaração de variáveis globais:

- `running` é uma variável booleana que controla se o programa está em execução ou não.
- `ip` (Intructon Pointer) é um ponteiro que aponta para o topo da pilha.
- `stack` é uma matriz que representa a pilha. 

#### 2 - Definição de um conjunto de intruções:

- `PSH` : Empurra um valor para a pilha (São instruções bem MNMÔNICAS). 
- `ADD` : Remove os dois valores do topo da pilha, adiciona-os e empurra o resultado de volta para a pilha. 
- `POP` : Remove o valor do topo da pilha. 
- `HTL` : Sinaliza o término do programa.

#### 3 - Declaração do programa: 

- É uma sequência de instruções que será executada pela VM. 

#### 4 - Função `fecth()`: 

- Retorna a próxima instrução a ser executada. 

#### 5 - Função `eval()`:

- Avalia a instrução atual e executa a operação correspondente. 

#### 6 Função `main()`:

- Um loop principal que continua executando enquanto a variável `running` for verdadeira.
- Chama `eval(fecth())` para execuar a próxima instrução.
- Incrementa o `ip` após cada interação do loop. 

Em resumo, este código implementa uma máquina virtual, básica que executa um programa represnetado pelo conjunto de instruções definido, manipulando os dados usando uma pilha. Ele segue um modelo simples de interpretação de bytecode. 

```c
/**

	This is almost identical to the articles
	VM

**/

#include <stdio.h>
#include <stdbool.h>

bool running = true;
int ip = 0;
int sp = -1;

int stack[256];

typedef enum {
   PSH,
   ADD,
   POP,
   HLT
} InstructionSet;

const int program[] = {
    PSH, 5,
    PSH, 6,
    ADD,
    POP,
    HLT
};

int fetch() {
    return program[ip];
}

void eval(int instr) {
    switch (instr) {
        case HLT: {
            running = false;
            printf("done\n");
            break;
        }
        case PSH: {
    	    sp++;
	        stack[sp] = program[++ip];
	        break;
        }
        case POP: {
	        int val_popped = stack[sp--];
	        printf("popped %d\n", val_popped);
	        break;
	    }
	    case ADD: {
	        // first we pop the stack and store it as a
	        int a = stack[sp--];
	    
	        // then we pop the top of the stack and store it as b
	        int b = stack[sp--];

	        // we then add the result and push it to the stack
	        int result = b + a;
	        sp++; // increment stack pointer **before**
	        stack[sp] = result; // set the value to the top of the stack

	        // all done!
	        break;
	    }
    }
}

int main() {
    while (running) {
        eval(fetch());
        ip++; // increment the ip every iteration
    }
}

``` 
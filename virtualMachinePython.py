running = True
ip = 0
sp = -1

stack = []


class InstructionSet:
    PSH = 0
    ADD = 1
    POP = 2
    HLT = 3


program = [
    InstructionSet.PSH,
    5,
    InstructionSet.PSH,
    6,
    InstructionSet.ADD,
    InstructionSet.POP,
    InstructionSet.HLT,
]


def fetch():
    return program[ip]


def eval(instr):
    global sp
    global ip


while running:
    eval(fetch())
    ip += 1

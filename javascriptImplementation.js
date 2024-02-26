let running = true;
let ip = 0;
let sp = -1;

const stack = [];

const InstructionSet = {
  PSH: 0,
  ADD: 1,
  POP: 2,
  HLT: 3,
};

const program = [
  InstructionSet.PSH,
  5,
  InstructionSet.PSH,
  6,
  InstructionSet.ADD,
  InstructionSet.POP,
  InstructionSet.HLT,
];

function eval(instr) {
  switch (instr) {
    case InstructionSet.HLT: {
      running = false;
      console.log("done");
      break;
    }
    case InstructionSet.PSH: {
      sp++;
      stack.push(program[++ip]);
      break;
    }
    case InstructionSet.POP: {
      const valPopped = stack.pop();
      console.log("popped", valPopped);
      break;
    }
    case InstructionSet.ADD: {
      const a = stack.pop();
      const b = stack.pop();
      const result = b + a;
      stack.push(result);
      break;
    }
  }
}

while (running) {
  eval(fetch());
  ip++;
}

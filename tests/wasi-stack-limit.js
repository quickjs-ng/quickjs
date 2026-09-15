let overflow;

// Use a large interpreter frame so QuickJS's linear-memory stack guard trips
// before Wasmtime's independent WebAssembly call stack is exhausted.
const args = Array(1024).fill("0").join(",");
const recurse = Function(
    `return function recurse() { recurse(${args}); }`
)();

try {
    recurse();
} catch (error) {
    overflow = error;
}

if (!(overflow instanceof Error) ||
    !/stack/i.test(overflow.message)) {
    throw new Error(`expected a stack overflow, got ${overflow}`);
}

if (Function("return 20 + 22")() !== 42)
    throw new Error("runtime is unusable after stack overflow");

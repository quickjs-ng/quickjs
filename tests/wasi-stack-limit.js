let overflow;

try {
    (function recurse() {
        recurse();
    })();
} catch (error) {
    overflow = error;
}

if (!(overflow instanceof Error) ||
    !/stack/i.test(overflow.message)) {
    throw new Error(`expected a stack overflow, got ${overflow}`);
}

if (Function("return 20 + 22")() !== 42)
    throw new Error("runtime is unusable after stack overflow");

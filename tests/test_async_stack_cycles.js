import { assert } from "./assert.js";

// Create a pending promise-adoption cycle. Capture inside the second adoption
// job, after both forwarding links exist, so reconstruction must bound its walk.
for (const structured of [false, true]) {
    Error.prepareStackTrace = structured ? (_, frames) => frames : undefined;
    let resolveP, resolveQ, captured;
    const p = new Promise(resolve => { resolveP = resolve; });
    const q = new Promise(resolve => { resolveQ = resolve; });
    p.then = function linkAndCapture(...args) {
        const result = Promise.prototype.then.apply(this, args);
        captured = new Error("cycle").stack;
        return result;
    };
    resolveP(q);
    resolveQ(p);
    await 0;
    if (structured) {
        assert(Array.isArray(captured));
        assert(captured[0].getFunctionName(), "linkAndCapture");
        assert(!captured.some(frame => frame.isAsync()));
    } else {
        assert(typeof captured, "string");
        assert(captured.includes("at linkAndCapture ("));
        assert(!captured.includes("at async"));
    }
}
Error.prepareStackTrace = undefined;

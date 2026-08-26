import { assert, assertThrows } from "./assert.js";

// Adapted from @mschwarzl's repros

// Test js_async_dispose_step path

let saved;
Promise.prototype.then = function (f, r) {if (typeof r === "function") saved = r;return Promise.resolve();};
let s = new AsyncDisposableStack();

s.use(undefined);
s.defer(() => {});
s.disposeAsync();
try { saved(); } catch {}
gc();
assert(saved !== undefined, true, "saved is undefined");

// Test js_async_dispose_rethrow path

let callCount = 0;
let saved2;

Promise.prototype.then = function (onFulfilled, onRejected) {
    callCount++;
    if (callCount === 1)
        return onRejected({ previous: 1 });   // drives step() into the rethrow branch
    if (callCount === 2) {
        saved2 = onRejected;                   // this is the js_async_dispose_rethrow reject_fn
        return Promise.resolve();
    }
    return Promise.resolve();
};

const stack = new AsyncDisposableStack();
stack.defer(() => undefined);
stack.defer(() => { throw { first: 1 }; });
stack.disposeAsync();

try { saved2(); } catch {}

gc();
assert(saved2 !== undefined, true, "saved2 is undefined");
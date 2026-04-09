import * as std from "qjs:std";
// https://github.com/quickjs-ng/quickjs/issues/1318
// Heap use-after-free when FinalizationRegistry is part of a reference cycle.
// Run under ASAN.

// Test 1: held value is the FR itself (original PoC pattern)
{
    let target = {};
    let fr = new FinalizationRegistry(function() {});
    fr.register(target, fr);
    fr.ref = target;
    target.ref = fr;
    target = null;
    fr = null;
    std.gc();
}

// Test 2: callback is part of the cycle
{
    let target = {};
    let cb = function() {};
    let fr = new FinalizationRegistry(cb);
    fr.register(target, 42);
    fr.ref = target;
    target.ref = fr;
    target.cb = cb;
    target = null;
    fr = null;
    cb = null;
    std.gc();
}

// Test 3: simplified real-world pattern (from issue comment)
{
    class Registry {
        #fr = new FinalizationRegistry(v => {});
        set(key) {
            const ref = new WeakRef(key);
            this.#fr.register(key, {}, ref);
        }
    }
    function Observer() {
        this.targets = new Registry;
    }
    Observer.prototype.observe = function(target) {
        this.targets.set(target);
        target.subset = new Set;
        target.subset.add(this);
    }
    let sample = {};
    new Observer().observe(sample);
    sample = null;
    std.gc();
}

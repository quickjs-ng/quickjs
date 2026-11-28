// Iterator.from wrapper next/return must forward the underlying results as
// returned by the underlying methods, without unwrapping done/value or
// requiring an object. See https://github.com/quickjs-ng/quickjs/issues/1926
import { assert, assertThrows } from "./assert.js";

// (1) next identity + no getters
{
    let doneReads = 0;
    let valueReads = 0;
    const result = {
        get done() { doneReads++; return false; },
        get value() { valueReads++; return 1; },
    };
    const iter = { next() { return result; } };
    const wrapper = Iterator.from(iter);
    assert(wrapper.next() === result, true);
    assert(doneReads, 0);
    assert(valueReads, 0);
}

// (2) non-object next results
{
    const cases = [
        [5, 5],
        ["hi", "hi"],
        [null, null],
        [undefined, undefined],
        [true, true],
        [false, false],
        [1n, 1n],
        [Symbol.iterator, Symbol.iterator],
    ];
    for (const [ret, expect] of cases) {
        const wrapper = Iterator.from({ next() { return ret; } });
        const got = wrapper.next();
        assert(got, expect);
    }
}

// (3) next() does not forward arguments
{
    let argc = -1;
    const wrapper = Iterator.from({
        next(...args) { argc = args.length; return { done: true, value: undefined }; },
    });
    wrapper.next();
    assert(argc, 0);
    wrapper.next(42);
    assert(argc, 0);
    wrapper.next(1, 2);
    assert(argc, 0);
}

{
    let argc = -1;
    const wrapper = Iterator.from({
        get return() { return function(...args) { argc = args.length; return { done: true, value: undefined }; }; },
    });
    wrapper.return();
    assert(argc, 0);
    wrapper.return(42);
    assert(argc, 0);
}

// (4) return object identity; non-object as-is; no extra args
{
    const retObj = { done: true, value: 9 };
    const wrapper = Iterator.from({
        return() { return retObj; },
    });
    assert(wrapper.return() === retObj, true);
}

for (const ret of [7, "x", null, undefined, false, 2n]) {
    const wrapper = Iterator.from({
        return() { return ret; },
    });
    assert(wrapper.return(), ret);
}

// (5) missing / undefined / null return → {value:undefined,done:true}
for (const retProp of [undefined, null]) {
  const spec = retProp === undefined ? {} : { return: retProp };
  const wrapper = Iterator.from(spec);
  const r = wrapper.return();
  assert(r.done, true);
  assert(r.value, undefined);
  const d = Object.getOwnPropertyDescriptor(r, "value");
  assert(d.writable, true);
  assert(d.enumerable, true);
  assert(d.configurable, true);
  const dd = Object.getOwnPropertyDescriptor(r, "done");
  assert(dd.writable, true);
  assert(dd.enumerable, true);
  assert(dd.configurable, true);
}

// (6) non-callable return → TypeError
assertThrows(TypeError, () => Iterator.from({ return: 1 }).return());
assertThrows(TypeError, () => Iterator.from({ return: "nope" }).return());

// (7) this is the original iterator
{
    const iter = {
        next() { assert(this, iter); return { done: true, value: undefined }; },
        return() { assert(this, iter); return { done: true, value: undefined }; },
    };
    const wrapper = Iterator.from(iter);
    wrapper.next();
    wrapper.return();
}

// (8) next captured at from(); return re-fetched each call
{
    let nextGets = 0;
    let returnGets = 0;
    const iter = {
        get next() { nextGets++; return function() { return { done: true, value: undefined }; }; },
        get return() { returnGets++; return function() { return { done: true, value: undefined }; }; },
    };
    const wrapper = Iterator.from(iter);
    assert(nextGets, 1);
    assert(returnGets, 0);
    wrapper.next();
    assert(nextGets, 1);
    wrapper.return();
    assert(returnGets, 1);
    wrapper.return();
    assert(returnGets, 2);
}

// (9) wrapper.next.call({}) / return.call({}) TypeError
assertThrows(TypeError, () => Iterator.from({ next() { return { done: true, value: undefined }; } }).next.call({}));
assertThrows(TypeError, () => Iterator.from({ return() { return { done: true, value: undefined }; } }).return.call({}));

// (10) negative controls
{
    const gen = (function* () { yield 1; })();
    assert(Iterator.from(gen) === gen, true);
    const arrIter = [][Symbol.iterator]();
    assert(Iterator.from(arrIter) === arrIter, true);
    class Sub extends Iterator {}
    const sub = new Sub();
    assert(Iterator.from(sub) === sub, true);
}
{
    const mapped = Iterator.from([1, 2]).map(x => x);
    const r1 = mapped.next();
    const r2 = mapped.next();
    assert(r1 !== r2, true);
    assert(r1.value, 1);
    assert(r1.done, false);
}

// (11) exceptions propagate
{
    const err = new Error("next err");
    assertThrows(Error, () => Iterator.from({ next() { throw err; } }).next());
    const err2 = new Error("return err");
    assertThrows(Error, () => Iterator.from({ return() { throw err2; } }).return());
}

print("PASS: bug1926");
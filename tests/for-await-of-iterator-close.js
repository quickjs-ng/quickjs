import { assert } from "./assert.js";

/* Two rules meet in for-await-of.

   ForIn/OfBodyEvaluation gets the next result with plain `?` steps: if
   next() throws, if the awaited result rejects, if it is not an object, or
   if the `done`/`value` getters throw, the loop propagates the failure
   *without* closing the iterator. Only an abrupt completion of the loop body
   (throw, break, return) closes it.

   AsyncFromSyncIteratorContinuation is the other way around: when the value
   a *sync* iterator yields is a promise that rejects, the sync iterator is
   closed with a throw completion before the rejection is handed on. */

function trace() {
    const log = [];
    return {
        log,
        push(x) { log.push(x); return x; },
        get text() { return log.join(","); },
    };
}

async function collect(t, body) {
    try {
        await body();
        t.push("no-throw");
    } catch (e) {
        t.push("caught:" + e.message);
    }
    return t.text;
}

/* an async iterator that fails in a configurable way */
function asyncIter(t, fail) {
    return {
        [Symbol.asyncIterator]() {
            return {
                next() {
                    t.push("next");
                    return fail();
                },
                return() {
                    t.push("return");
                    return Promise.resolve({ done: true });
                },
            };
        },
    };
}

/* --- an abrupt next() must not close the iterator --------------------- */
{
    const t = trace();
    assert(await collect(t, async () => {
        for await (const x of asyncIter(t, () => { throw new Error("nx"); }))
            t.push("body");
    }), "next,caught:nx");
}
{
    const t = trace();
    assert(await collect(t, async () => {
        for await (const x of asyncIter(t, () => Promise.reject(new Error("nr"))))
            t.push("body");
    }), "next,caught:nr");
}
{
    /* the result is not an object */
    const t = trace();
    const got = await collect(t, async () => {
        for await (const x of asyncIter(t, () => 1))
            t.push("body");
    });
    assert(got.startsWith("next,caught:"), true, got);
    assert(t.log.indexOf("return"), -1, got);
    assert(t.log.length, 2, got);
}
{
    /* the awaited result resolves, but its getters throw */
    const t = trace();
    assert(await collect(t, async () => {
        for await (const x of asyncIter(t, () => Promise.resolve({
            get done() { t.push("done"); throw new Error("dn"); },
            value: 1,
        }))) t.push("body");
    }), "next,done,caught:dn");
}
{
    const t = trace();
    assert(await collect(t, async () => {
        for await (const x of asyncIter(t, () => Promise.resolve({
            done: false,
            get value() { t.push("value"); throw new Error("vl"); },
        }))) t.push("body");
    }), "next,value,caught:vl");
}
/* --- an abrupt body must close it ------------------------------------- */
{
    const ok = () => Promise.resolve({ done: false, value: 1 });

    const t1 = trace();
    assert(await collect(t1, async () => {
        for await (const x of asyncIter(t1, ok)) {
            t1.push("body");
            throw new Error("bd");
        }
    }), "next,body,return,caught:bd");

    const t2 = trace();
    assert(await collect(t2, async () => {
        for await (const x of asyncIter(t2, ok)) {
            t2.push("body");
            break;
        }
    }), "next,body,return,no-throw");

    const t3 = trace();
    assert(await collect(t3, async () => {
        await (async () => {
            for await (const x of asyncIter(t3, ok)) {
                t3.push("body");
                return;
            }
        })();
    }), "next,body,return,no-throw");

    /* a normal finish never closes */
    const t4 = trace();
    let i = 0;
    assert(await collect(t4, async () => {
        for await (const x of asyncIter(t4, () =>
            Promise.resolve(i++ < 1 ? { done: false, value: 1 } : { done: true })))
            t4.push("body");
    }), "next,body,next,no-throw");
}

/* --- the sync iterator behind an async-from-sync wrapper -------------- */
function syncIter(t, next) {
    return {
        [Symbol.iterator]() {
            return {
                next() { t.push("next"); return next(); },
                return() { t.push("return"); return { done: true }; },
            };
        },
    };
}

{
    /* a yielded promise that rejects closes the sync iterator */
    const t = trace();
    assert(await collect(t, async () => {
        for await (const x of syncIter(t, () => ({
            done: false,
            value: Promise.reject(new Error("rp")),
        }))) t.push("body");
    }), "next,return,caught:rp");
}
{
    /* ... including through yield* delegation in an async generator */
    const t = trace();
    const src = syncIter(t, () => ({
        done: false,
        value: Promise.reject(new Error("yp")),
    }));
    async function* g() { yield* src; }
    assert(await collect(t, async () => {
        for await (const x of g()) t.push("body");
    }), "next,return,caught:yp");
}
{
    /* a value that is a thenable whose then() throws is the same shape */
    const t = trace();
    assert(await collect(t, async () => {
        for await (const x of syncIter(t, () => ({
            done: false,
            value: { then(res, rej) { t.push("then"); rej(new Error("th")); } },
        }))) t.push("body");
    }), "next,then,return,caught:th");
}
{
    /* a done result still has its value awaited, so a rejection propagates,
       but there is nothing left to close */
    const t = trace();
    assert(await collect(t, async () => {
        for await (const x of syncIter(t, () => ({
            done: true,
            value: Promise.reject(new Error("dr")),
        }))) t.push("body");
    }), "next,caught:dr");
    assert(t.log.indexOf("return"), -1);
}
{
    /* but an abrupt IteratorValue rejects without closing */
    const t = trace();
    assert(await collect(t, async () => {
        for await (const x of syncIter(t, () => ({
            done: false,
            get value() { t.push("value"); throw new Error("sv"); },
        }))) t.push("body");
    }), "next,value,caught:sv");
}
{
    /* and so does an abrupt next() on the sync iterator */
    const t = trace();
    assert(await collect(t, async () => {
        for await (const x of syncIter(t, () => { throw new Error("sn"); }))
            t.push("body");
    }), "next,caught:sn");
}
{
    /* a plain sync iteration still runs to completion */
    const t = trace();
    let i = 0;
    assert(await collect(t, async () => {
        for await (const x of syncIter(t, () =>
            i++ < 2 ? { done: false, value: i } : { done: true }))
            t.push("body" + x);
    }), "next,body1,next,body2,next,no-throw");
}

/* --- the loop still yields the right values -------------------------- */
{
    const seen = [];
    for await (const x of [1, Promise.resolve(2), 3])
        seen.push(x);
    assert(seen.join(","), "1,2,3");

    async function* gen() { yield 1; yield 2; }
    const seen2 = [];
    for await (const x of gen())
        seen2.push(x);
    assert(seen2.join(","), "1,2");

    /* nested loops keep their own iterators straight */
    const pairs = [];
    for await (const a of [1, 2])
        for await (const b of ["x", "y"])
            pairs.push(a + b);
    assert(pairs.join(","), "1x,1y,2x,2y");

    /* a labelled break closes only the inner iterator */
    const t = trace();
    const inner = asyncIter(t, () => Promise.resolve({ done: false, value: 1 }));
    for await (const a of [1]) {
        for await (const b of inner) {
            t.push("body");
            break;
        }
    }
    assert(t.text, "next,body,return");
}

/* --- the control flow that moves the loop's stack around -------------- */
{
    const upto = (t, n) => {
        let i = 0;
        return asyncIter(t, () => Promise.resolve(
            i++ < n ? { done: false, value: i } : { done: true }));
    };

    /* continue takes the back edge, which has to find the same stack it
       started with */
    {
        const t = trace();
        const seen = [];
        for await (const x of upto(t, 4)) {
            if (x % 2) continue;
            seen.push(x);
        }
        assert(seen.join(","), "2,4");
        assert(t.text, "next,next,next,next,next");
    }

    /* a labelled break and a labelled continue that cross a loop boundary
       close every iterator they leave, innermost first */
    {
        const t = trace();
        outer:
        for await (const a of upto(t, 3)) {
            for await (const b of upto(t, 3)) {
                if (a === 2) break outer;
                continue outer;
            }
        }
        assert(t.text, "next,next,return,next,next,return,return");
    }

    /* try/finally around and inside the loop: the finally blocks run in
       order and the iterator is still closed exactly once */
    {
        const t = trace();
        try {
            for await (const x of upto(t, 3)) {
                try {
                    throw new Error("boom");
                } finally {
                    t.push("inner-finally");
                }
            }
        } catch (e) {
            t.push("caught:" + e.message);
        } finally {
            t.push("outer-finally");
        }
        assert(t.text, "next,inner-finally,return,caught:boom,outer-finally");
    }

    /* a return out of a try with a finally, from inside the loop */
    {
        const t = trace();
        const r = await (async () => {
            for await (const x of upto(t, 3)) {
                try {
                    return "returned";
                } finally {
                    t.push("finally");
                }
            }
        })();
        assert(r, "returned");
        assert(t.text, "next,finally,return");
    }

    /* awaiting in the body suspends the function with the loop's stack live */
    {
        const t = trace();
        const seen = [];
        for await (const x of upto(t, 3)) {
            await null;
            seen.push(await Promise.resolve(x * 10));
            await null;
        }
        assert(seen.join(","), "10,20,30");
    }

    /* the same, in an async generator, where yield suspends it as well */
    {
        async function* relay(t) {
            for await (const x of upto(t, 3))
                yield x * 2;
        }
        const t = trace();
        const seen = [];
        for await (const x of relay(t))
            seen.push(x);
        assert(seen.join(","), "2,4,6");

        /* abandoning the outer loop closes the generator, which closes the
           inner iterator it was driving */
        const t2 = trace();
        for await (const x of relay(t2))
            break;
        assert(t2.text, "next,return");
    }

    /* a destructuring head, and a head that assigns to an existing lvalue,
       both run after the iterator has been restored */
    {
        const t = trace();
        let i = 0;
        const pairs = {
            [Symbol.asyncIterator]: () => ({
                next: () => Promise.resolve(
                    i++ < 2 ? { done: false, value: { a: i, b: [i, i + 1] } }
                            : { done: true }),
            }),
        };
        const seen = [];
        for await (const { a, b: [b0, b1] } of pairs)
            seen.push(a + ":" + b0 + ":" + b1);
        assert(seen.join(","), "1:1:2,2:2:3");

        const obj = {};
        let last;
        i = 0;
        for await (obj.k of pairs) last = obj.k.a;
        assert(last, 2);
        i = 0;
        for await (last of pairs) ;
        assert(last.a, 2);
    }

    /* nesting deeply enough that an understated stack effect would run off
       the end of the frame */
    {
        const t = trace();
        let n = 0;
        for await (const a of upto(t, 2))
            for await (const b of upto(t, 2))
                for await (const c of upto(t, 2))
                    for await (const d of upto(t, 2))
                        for await (const e of upto(t, 2))
                            n += a + b + c + d + e;
        assert(n, 240);
    }

    /* a rejected next() leaves the iterator open even from inside a try */
    {
        const t = trace();
        let caught = null;
        try {
            for await (const x of asyncIter(t, () => Promise.reject(new Error("nx"))))
                t.push("body");
        } catch (e) {
            caught = e.message;
        }
        assert(caught, "nx");
        assert(t.text, "next");
    }
}

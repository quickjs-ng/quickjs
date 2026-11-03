import { assert } from "./assert.js";

async function testReentry() {
    let cases = 0;
    for (const delegated of [false, true]) {
        for (const operation of ["next", "return", "throw"]) {
            const events = [];
            const reason = Symbol("throw");
            async function* body() {
                try {
                    events.push("start");
                    const sent = yield 1;
                    events.push("after-first:" + sent);
                    yield 2;
                } catch (e) {
                    assert(e, reason);
                    events.push("caught");
                    yield 3;
                } finally {
                    await 0;
                    events.push("finally");
                }
                return 4;
            }
            const g = delegated ? (async function* () { return yield* body(); })() : body();
            let second, entered = false;
            Object.defineProperty(Object.prototype, "then", {
                configurable: true,
                get() {
                    if (!entered) {
                        entered = true;
                        events.push("getter");
                        second = g[operation](operation === "throw" ? reason : 9);
                        events.push("getter-return");
                    }
                },
            });
            try {
                const first = await g.next();
                const next = await second;
                assert(first.value, 1);
                assert(first.done, false);
                assert(next.value, operation === "next" ? 2 : operation === "throw" ? 3 : 9);
                assert(next.done, operation === "return");
                assert(events[0], "start");
                assert(events[1], "getter");
                assert(events.filter(x => x === "getter-return").length, 1);
                if (operation === "next")
                    assert(events.filter(x => x === "after-first:9").length, 1);
                if (operation === "throw")
                    assert(events.filter(x => x === "caught").length, 1);
                const end = operation === "next" ? await g.next() : await g.return(4);
                assert(end.value, 4);
                assert(end.done, true);
                assert(events.filter(x => x === "finally").length, 1);
                const completed = await g.next();
                assert(completed.value, undefined);
                assert(completed.done, true);
                cases++;
            } finally {
                delete Object.prototype.then;
            }
        }
    }

    for (const rejected of [false, true]) {
        let release;
        const reason = Symbol("await");
        const gate = new Promise((resolve, reject) => {
            release = rejected ? () => reject(reason) : () => resolve(2);
        });
        const events = [];
        const g = (async function* () {
            yield 1;
            try {
                events.push("waiting");
                yield await gate;
            } catch (e) {
                assert(e, reason);
                events.push("caught");
                yield 3;
            }
        })();
        let second, settled = false, entered = false;
        Object.defineProperty(Object.prototype, "then", {
            configurable: true,
            get() {
                if (!entered) {
                    entered = true;
                    second = g.next();
                    second.then(() => { settled = true; });
                }
            },
        });
        try {
            const first = await g.next();
            assert(first.value, 1);
            assert(first.done, false);
            await 0;
            await 0;
            assert(settled, false);
            assert(events.join(","), "waiting");
            release();
            const next = await second;
            assert(next.value, rejected ? 3 : 2);
            assert(next.done, false);
            assert(events.join(","), rejected ? "waiting,caught" : "waiting");
            const end = await g.next();
            assert(end.value, undefined);
            assert(end.done, true);
            cases++;
        } finally {
            delete Object.prototype.then;
        }
    }
    return cases;
}

assert(await testReentry(), 8);

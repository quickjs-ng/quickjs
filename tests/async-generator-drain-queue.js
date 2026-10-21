import { assert } from "./assert.js";

/* AsyncGeneratorAwaitReturn and AsyncGeneratorDrainQueue: once the generator
   has completed, every request queued behind a return() must still settle,
   in order. A request that never settles leaves the module promise pending. */

async function completed() {
    const it = (async function* () {})();
    assert((await it.next()).done, true);
    return it;
}

function track(log, label, promise) {
    return promise.then(
        v => { log.push(label + ":" + v.value + ":" + v.done); },
        e => { log.push(label + ":throw:" + e); });
}

async function run(requests) {
    const it = await completed();
    const log = [];
    await Promise.all(requests(it).map(([label, p]) => track(log, label, p)));
    return log.join(",");
}

/* a single request behind the awaited return value */
assert(await run(it => [["return", it.return(1)], ["next", it.next()]]),
       "return:1:true,next:undefined:true");
assert(await run(it => [["return", it.return(1)], ["return", it.return(2)]]),
       "return:1:true,return:2:true");
assert(await run(it => [["return", it.return(1)], ["throw", it.throw("e")]]),
       "return:1:true,throw:throw:e");
assert(await run(it => [["return", it.return(Promise.reject("r"))],
                        ["return", it.return(2)]]),
       "return:throw:r,return:2:true");

/* several requests: the queue is drained past next() and throw() */
assert(await run(it => [["return", it.return(1)], ["next", it.next()],
                        ["next", it.next()]]),
       "return:1:true,next:undefined:true,next:undefined:true");
assert(await run(it => [["return", it.return(1)], ["throw", it.throw("e")],
                        ["next", it.next()]]),
       "return:1:true,throw:throw:e,next:undefined:true");
assert(await run(it => [["return", it.return(Promise.reject("r"))],
                        ["next", it.next()], ["return", it.return(2)]]),
       "return:throw:r,next:undefined:true,return:2:true");
assert(await run(it => [["return", it.return(1)], ["next", it.next()],
                        ["return", it.return(2)], ["next", it.next()]]),
       "return:1:true,next:undefined:true,return:2:true,next:undefined:true");

/* requests queued while the body is still running reach a completed
   generator without any return() call */
{
    const gate = Promise.withResolvers();
    const it = (async function* () { await gate.promise; })();
    const log = [];
    const requests = [1, 2, 3].map(i => track(log, "next" + i, it.next()));
    gate.resolve();
    await Promise.all(requests);
    assert(log.join(","),
           "next1:undefined:true,next2:undefined:true,next3:undefined:true");
}

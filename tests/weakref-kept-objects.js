import { assert } from "./assert.js";

/* the constructor keeps the target alive until the end of the job */
{
    let o = { x: 1 };
    const w = new WeakRef(o);
    o = null;
    assert(typeof w.deref(), "object");
}

/* deref() keeps it too, and the kept set is cleared at job boundaries */
{
    let o = { x: 2 };
    const w = new WeakRef(o);
    assert(w.deref().x, 2);
    o = null;
    assert(typeof w.deref(), "object");
    await Promise.resolve();
    assert(w.deref(), undefined);
}

import * as std from "qjs:std";
import { assert } from "./assert.js";

{
    const keep = [];
    for (let i = 0; i < 30; i++) {
        const a = [0];
        a.length = 245;
        a.push(2);
        keep.push(a);
    }
    std.gc();
    for (const a of keep) {
        assert(a.length, 246);
        assert(a[0], 0);
        assert(a[245], 2);
        assert(a.hasOwnProperty(1), false);
    }
}

{
    const a = [0];
    a.length = 245;
    assert(a.push(2), 246);
    assert(a.length, 246);
    assert(a[0], 0);
    assert(a[245], 2);
    assert(1 in a, false);
    assert(244 in a, false);
    assert(245 in a, true);
}

{
    const d = [9];
    d.length = 3;
    assert(d.push(7, 8), 5);
    assert(d.length, 5);
    assert(d[0], 9);
    assert(d[3], 7);
    assert(d[4], 8);
    assert(1 in d, false);
    assert(2 in d, false);
}

{
    const objs = [];
    for (let i = 0; i < 30; i++) {
        const a = [{ a: 1 }, { b: 2 }, { c: 3 }, { d: 4 }, { e: 5 }];
        a.length = 2;
        a.length = 5;
        a.push({ f: 6 });
        objs.push(a);
    }
    std.gc();
    for (const a of objs) {
        assert(a.length, 6);
        assert(a[0].a, 1);
        assert(a[1].b, 2);
        assert(2 in a, false);
        assert(3 in a, false);
        assert(4 in a, false);
        assert(a[5].f, 6);
    }
}


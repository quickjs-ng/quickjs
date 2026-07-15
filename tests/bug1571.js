import { assert } from "./assert.js";

function check(unit, halfCount) {
    const rope = unit.repeat(halfCount) + unit.repeat(halfCount);
    const flat = unit.repeat(halfCount * 2);
    assert(rope.length, flat.length);
    assert(RegExp.escape(rope), RegExp.escape(flat));
}

check("a", 50000);
check("a.b-c|d", 4000);
check("€☃", 4000);


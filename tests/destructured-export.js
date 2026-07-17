import { assert, assertArrayEquals } from "./assert.js";
import * as mod from "./destructured-export.js";

export const { a, b, c } = { a: 1, b: 2, c: 3 };
export const d = 4;

/* array destructuring targets must be exported too */
export const [e, f] = [5, 6];
export const [g, ...h] = [7, 8, 9];
export let [{ i }, [j]] = [{ i: 10 }, [11]];

assert(typeof mod === 'object');
assertArrayEquals(Object.keys(mod),
                  ["a", "b", "c", "d", "e", "f", "g", "h", "i", "j"]);
assert(mod.e, 5);
assert(mod.f, 6);
assert(mod.g, 7);
assertArrayEquals(mod.h, [8, 9]);
assert(mod.i, 10);
assert(mod.j, 11);

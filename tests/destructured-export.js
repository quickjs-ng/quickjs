import { assert, assertArrayEquals } from "./assert.js";
import * as mod from "./destructured-export.js";

export const { a, b, c } = { a: 1, b: 2, c: 3 };
export const d = 4;

assert(typeof mod === 'object');
assertArrayEquals(Object.keys(mod), ["a", "b", "c", "d"]);

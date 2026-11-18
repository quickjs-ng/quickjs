import { assert, assertArrayEquals, assertThrows } from "./assert.js";

assertArrayEquals(Array.from({ [Symbol.iterator]: null, length: 1, 0: 7 }), [7]);

const log = [];
assertThrows(TypeError, () => Array.from.call(
    function C() { log.push("construct"); },
    { get [Symbol.iterator]() { log.push("get"); return 0; } }
));
assert(log.join(", "), "get");

let gets = 0;
Array.from({ get [Symbol.iterator]() {
    gets++;
    return [1][Symbol.iterator];
} });
assert(gets, 1);

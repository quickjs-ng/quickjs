import { assert } from "./assert.js";

const regexp = new RegExp("a", "g");
let unrelated;

Object.defineProperty(regexp, "constructor", {
    get() {
        regexp.lastIndex = {
            valueOf() {
                regexp.compile("b", "g");
                unrelated = new RegExp("c", "g");
                return 0;
            },
        };
        return RegExp;
    },
});

assert("ccc".replace(regexp, ""), "ccc");
assert(regexp.source, "b");
assert(unrelated.source, "c");

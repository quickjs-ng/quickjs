import { assert } from "./assert.js";
const buffer = new ArrayBuffer(4, {maxByteLength: 8});
const array = new Int8Array(buffer);
const index = {
    valueOf() {
        buffer.resize(6);
        return 4;
    },
};
assert(array.at(index), undefined);

import { assert } from "./assert.js";
const buffer = new ArrayBuffer(4 * Int8Array.BYTES_PER_ELEMENT, {maxByteLength: 8 * Int8Array.BYTES_PER_ELEMENT,});
const array = new Int8Array(buffer);
const index = {
    valueOf() {
        buffer.resize(6 * Int8Array.BYTES_PER_ELEMENT);
        return 4;
    },
};
assert(array.at(index), undefined);

import { assert } from "./assert.js";

// TypedArray.prototype.with must re-validate after user code in valueOf
// https://github.com/bellard/quickjs/issues/492

// Test 1: resize (shrink) during valueOf — no heap over-read
function test_resize_shrink() {
    var ab = new ArrayBuffer(4096, { maxByteLength: 4096 });
    var ta = new Int32Array(ab);
    for (var i = 0; i < ta.length; i++) ta[i] = 0x42424242;

    var result = ta.with(0, {
        valueOf() {
            ab.resize(4);
            return 999;
        }
    });

    assert(result[0] === 999);
    // result should have original length with remaining elements zero-filled
    assert(result.length === 1024);
    assert(result[1] === 0);
    assert(result[1023] === 0);
}

// Test 2: detach during valueOf — must throw TypeError
function test_detach() {
    var ab = new ArrayBuffer(16);
    var ta = new Int32Array(ab);
    ta[0] = 1; ta[1] = 2;
    var caught = false;
    try {
        ta.with(0, {
            valueOf() {
                ab.transfer();
                return 999;
            }
        });
    } catch(e) {
        caught = true;
        assert(e instanceof TypeError);
    }
    assert(caught);
}

// Test 3: resize to 0 during valueOf — must throw RangeError
// (RAB-tracking typed array is not OOB, but index is out of bounds)
function test_resize_zero() {
    var ab = new ArrayBuffer(16, { maxByteLength: 16 });
    var ta = new Int32Array(ab);
    ta[0] = 1;
    var caught = false;
    try {
        ta.with(0, {
            valueOf() {
                ab.resize(0);
                return 999;
            }
        });
    } catch(e) {
        caught = true;
        assert(e instanceof RangeError);
    }
    assert(caught);
}

// Test 4: grow during valueOf — result should use original length
function test_resize_grow() {
    var ab = new ArrayBuffer(16, { maxByteLength: 1024 });
    var ta = new Int32Array(ab);
    ta[0] = 1; ta[1] = 2; ta[2] = 3; ta[3] = 4;

    var result = ta.with(0, {
        valueOf() {
            ab.resize(1024);
            return 999;
        }
    });

    assert(result.length === 4);
    assert(result[0] === 999);
    assert(result[1] === 2);
    assert(result[2] === 3);
    assert(result[3] === 4);
}

// Test 5: normal case (no resize) still works
function test_normal() {
    var ta = new Int32Array([10, 20, 30, 40]);
    var result = ta.with(2, 99);
    assert(result.length === 4);
    assert(result[0] === 10);
    assert(result[1] === 20);
    assert(result[2] === 99);
    assert(result[3] === 40);
}

// Test 6: negative index uses original length
function test_negative_index() {
    var ta = new Float64Array([1.5, 2.5, 3.5]);
    var result = ta.with(-1, 9.9);
    assert(result.length === 3);
    assert(result[0] === 1.5);
    assert(result[1] === 2.5);
    assert(result[2] === 9.9);
}

test_resize_shrink();
test_detach();
test_resize_zero();
test_resize_grow();
test_normal();
test_negative_index();

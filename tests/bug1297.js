// Test for issue #1297: Heap buffer overflow in js_typed_array_sort
// The bug occurs when a comparator function resizes the ArrayBuffer during sort

const sz = 256;
const newSz = 10;
const ab = new ArrayBuffer(sz, { maxByteLength: sz * 10 });
const u8 = new Uint8Array(ab);

for (let i = 0; i < sz; i++) u8[i] = i;
u8[sz - 1] = 0;

let cnt = 0;
u8.sort((a, b) => {
    for (let i = 0; i < 3000; i++) {
      try { ab.resize(newSz); } catch(e){}
    }

    return a - b;
});

// If we get here without crashing, the fix works
print("PASS: bug1297 - typed array sort with resize did not crash");

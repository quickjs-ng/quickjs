// Bug: expand_fast_array calls max_int(new_len, new_size) where new_len is uint32_t.
//      When new_len >= 0x80000000, cast to int makes it negative.
//      max_int returns the small grow-by-50% size. Buffer is underallocated.
//      add_fast_array_element then writes to values[new_len-1] — OOB write.
//
// Pre-fix behavior: process crashes with SIGBUS (OOB write to unmapped memory)
// Post-fix behavior: InternalError or RangeError thrown and caught cleanly
//
// Run: qjs poc_test_fixed.js
// Expected output: PASS: got graceful error: ...
const arr = [1, 2, 3];
arr.length = 0x7FFFFFFF;

try {
    const result = arr.push(4);
    // Succeeded via slow array path — no crash, no heap corruption                                                                
    print("PASS: push completed without crash, result:", result);

} catch(e) {                   
    if (e instanceof InternalError || e instanceof RangeError || e instanceof TypeError) {
        print("PASS: got graceful error:", e.message);
  } else {
      throw e;  // unexpected — re-throw                      
  }
}

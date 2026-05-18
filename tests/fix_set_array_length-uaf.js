// Crash — heap-use-after-free (confirmed with ASan)
const arr = [{a:1}, {b:2}, {c:3}, {d:4}, {e:5}];
arr.length = 2;   // frees elements 2-4
arr.length = 5;   // grows length back; count stays at 2
arr.push({f:6}); // count jumps to 6; elements 2-4 are freed but readable
print(JSON.stringify(arr[3])); // reads freed memory

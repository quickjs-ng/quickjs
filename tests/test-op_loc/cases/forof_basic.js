var b = [];
for (const a of b) {
}

/* EXPECT(a):
        loc 2:12
        put_loc 1: a
*/

/* EXPECT(b):
        loc 2:17
        for_of_next 0
*/

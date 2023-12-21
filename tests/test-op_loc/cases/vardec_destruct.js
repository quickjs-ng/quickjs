// ·d, ·f
const [d, { e: f } = { e: 2 }] = [];

// ·i, = ·{}
const { h: { i = 1 } = {} } = {};

/* EXPECT(d):
        source_loc 2:8
        put_var_init d
*/

/* EXPECT(f):
        source_loc 2:16
        put_var_init f
*/

/* EXPECT(i):
        source_loc 5:14
        put_var_init i
*/

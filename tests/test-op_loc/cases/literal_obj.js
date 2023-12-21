const fn = () => {};
// ·f, = ·{}, ·fn()
const { d, e: f = { e: 2 } } = { d: fn() };

/* EXPECT(()=>{}):
        loc 1:12
        put_var_init fn
*/

/* EXPECT(f):
        loc 3:15
        put_var_init f
*/

var a = 1;
let b = 2;
const c = 3,
  d = 4;
let e;
var f;  

/* EXPECT(a):
        source_loc 1:9
        put_var a
*/

/* EXPECT(b):
        source_loc 2:9
        put_var_init b
*/

/* EXPECT(c):
        source_loc 3:11
        put_var_init c
*/

/* EXPECT(d):
        source_loc 4:7
        put_var_init d
*/

/* EXPECT(e):
        source_loc 5:5
        put_var_init e
*/

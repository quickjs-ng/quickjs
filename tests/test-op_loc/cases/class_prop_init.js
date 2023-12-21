const f = () => {};
class c {
  a = f();
}

/* EXPECT(a = f())):
        get_var f
        source_loc 3:8
        call 0
*/

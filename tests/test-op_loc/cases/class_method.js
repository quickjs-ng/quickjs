class c {
  f() {}
}

new c().f();

/* EXPECT(c.f()):
        source_loc 5:9
        get_field2 f
        source_loc 5:10
        call_method 0
*/

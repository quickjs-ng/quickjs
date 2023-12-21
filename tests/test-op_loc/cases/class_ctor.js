class c {
  constructor(a) {
    this.a = a;
  }

  f2() {}
}

function f1() {}
const a = {
  f1,
};

1 + new a.f1(1);
2 + new c().f2();

/* EXPECT(= a):
        source_loc 3:14
        insert2
        put_field a
*/

/* EXPECT(new a.f1(1)):
        source_loc 14:11
        get_field f1
        dup
        push_i32 1
        source_loc 14:9
        call_constructor 1
*/

/* EXPECT(new c().f2()):
        get_var c
        dup
        source_loc 15:9
        call_constructor 0
        source_loc 15:13
        get_field2 f2
        source_loc 15:15
        call_method 0
*/

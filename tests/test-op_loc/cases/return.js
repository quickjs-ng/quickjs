function f() {
  return print(1);
}

function f1() {
  return;
}

function f2(a) {
  return a;
}

/* EXPECT(return print(1)):
        source_loc 2:15
        call 1
        source_loc 2:3
        return
*/

/* EXPECT(return):
        source_loc 6:3
        return_undef
*/

/* EXPECT(return a):
        get_arg 0: a
        source_loc 10:3
        return
*/

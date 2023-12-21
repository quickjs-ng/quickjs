var a = 1;

do {
  print(a);
} while (a < 0);

/* EXPECT(a < 0):
        get_var a
        push_i32 0
        lt
        source_loc 5:10
        if_true 3:36
*/

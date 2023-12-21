var a = 1;

if (print(a)) {
} else if ("😀" === a) {
  print(2);
}

/* EXPECT(print(a)):
        get_var print
        get_var a
        source_loc 3:10
        call 1
*/

/* EXPECT(print(2)):
        source_loc 5:3
        get_var print
        push_i32 2
        source_loc 5:8
        call 1
*/

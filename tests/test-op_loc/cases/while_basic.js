var a = 0,
  b = 10;

while (a > b) {
  a++;
}

/* EXPECT(a++):
        source_loc 5:3
        get_var a
        post_inc
*/

/* EXPECT(a > b):
        source_loc 4:12
        get_var b
        gt
        source_loc 4:8
        if_false 3:147
*/

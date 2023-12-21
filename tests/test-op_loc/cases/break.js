var a = 1;

outer: while (a > 1) {
  while (a < 1) {
    break outer;
  }
}

/* EXPECT(outer):
        source_loc 5:5
        goto 2:128
*/

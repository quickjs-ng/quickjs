var a = 1;

outer: while (a > 1) {
  while (a < 1) {
    break outer;
  }
}

/* EXPECT(while):
3:8 ident: 'while'
*/

/* EXPECT(outer):
5:11 ident: 'outer'
*/

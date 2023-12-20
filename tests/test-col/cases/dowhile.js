var a = 1;

do {
  print(a);
} while (a < 0);

/* EXPECT({):
3:4 token: '{'
*/

/* EXPECT(a):
4:9 ident: 'a'
*/

/* EXPECT(a <):
5:10 ident: 'a'
5:12 token: '<'
*/

/* EXPECT(0):
5:14 number: 0
*/
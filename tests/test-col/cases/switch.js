var a = 1;

switch (a) {
  case "a":
    break;
  default   :
    print(a);
}

/* EXPECT(a):
3:9 ident: 'a'
*/

/* EXPECT('a'):
4:8 string: 'a
*/

/* EXPECT(:):
6:13 token: ':'
*/

/* EXPECT(a in print):
7:11 ident: 'a'
*/

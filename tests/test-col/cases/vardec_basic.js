var a = 1;
let b = 1,
  c;
 const /* 🙂abc */ d = 1;

const fn = () => {}; /*
😀
*/ const obj = { d: fn() };

/* EXPECT(a):
1:5 ident: 'a'
1:7 token: '='
1:9 number: 1
*/

/* EXPECT(b):
2:5 ident: 'b'
2:7 token: '='
2:9 number: 1
*/

/* EXPECT(c):
3:3 ident: 'c'
*/

/* EXPECT(d):
4:19 ident: 'd'
4:21 token: '='
4:23 number: 1
*/

/* EXPECT(const after cmt):
8:4 ident: 'const'
*/

/* EXPECT(fn):
8:21 ident: 'fn'
*/

var a;

function b() {}

var c = { ["👌"]: () => {} };

a = new b();
a = a + b() + c["👌"]();

/* EXPECT(b after new):
7:9 ident: 'b'
*/

/* EXPECT(( after new b):
7:10 token: '('
*/

/* EXPECT(( after b):
8:10 token: '('
*/

/* EXPECT([ after c):
8:15 ident: 'c'
*/

/* EXPECT() after ]):
8:22 token: ')'
*/

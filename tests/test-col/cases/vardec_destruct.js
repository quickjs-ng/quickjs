const [d, { e: f } = { e: 1 }] = [];
const { h: { 
    i = 1 } = {} } = {};

/* EXPECT(d):
1:8 ident: 'd'
*/

/* EXPECT(e-alias):
1:13 ident: 'e'
*/

/* EXPECT(e-default):
1:24 ident: 'e'
*/

/* EXPECT(f):
1:16 ident: 'f'
*/

/* EXPECT(h):
2:9 ident: 'h'
*/

/* EXPECT(i):
3:5 ident: 'i'
*/

/* EXPECT([):
1:34 token: '['
*/

/* EXPECT({):
3:22 token: '{'
*/

var b;
for (const {
    a, b: c } in b) {
}

/* EXPECT(a):
3:5 ident: 'a'
*/

/* EXPECT(b):
3:8 ident: 'b'
*/

/* EXPECT(c):
3:11 ident: 'c'
*/

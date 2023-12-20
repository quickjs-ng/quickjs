var b = 
[];
for (const [
    a,
{ b: c = 1 }] of 
b) {
}

/* EXPECT(]):
2:2 token: ']'
*/

/* EXPECT(a):
4:5 ident: 'a'
*/

/* EXPECT(b):
5:3 ident: 'b'
*/

/* EXPECT(b):
6:1 ident: 'b'
*/

/* EXPECT(c):
5:6 ident: 'c' 
*/

/* EXPECT(1):
5:10 number: 1
*/
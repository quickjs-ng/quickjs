 for (let a = 1, 
  b, c; a < 10; a++, b++) {
  c += 1;
}

/* EXPECT(let a = 1):
1:7 ident: 'let'
1:11 ident: 'a'
1:13 token: '='
1:15 number: 1
*/

/* EXPECT(b):
2:3 ident: 'b'
*/

/* EXPECT(c in c += 1):
3:3 ident: 'c'
*/

/* EXPECT(1 in c += 1):
3:8 number: 1
*/

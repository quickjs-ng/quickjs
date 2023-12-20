 let a = "😀" + 1;

 let b = "🙂🙂" + 2;

/* EXPECT(let):
1:2 ident: 'let'
*/

/* EXPECT(a):
1:6 ident: 'a'
*/

/* EXPECT(=):
1:8 token: '='
*/

/* EXPECT(1):
1:16 number: 1
*/

/* EXPECT(2):
3:17 number: 2
*/
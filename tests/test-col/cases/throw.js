try {
  throw "test😀";
} catch (error) {}

/* EXPECT({ after try):
1:5 token: '{'
*/

/* EXPECT(; term throw):
2:16 token: ';'
*/

/* EXPECT(error):
3:10 ident: 'error'
*/

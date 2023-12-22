var a = 1;

switch (a) {
  case "a":
    print("a");
    break;
  case "b":
    break;
  default:
    print(a);
}

/* EXPECT(case "a"):
        source_loc 5:5
        get_var print
        push_atom_value a
        source_loc 5:10
        call 1
*/

/* EXPECT(break b):
        source_loc 8:5
        goto 1:193
*/

/* EXPECT(default):
        source_loc 10:5
        get_var print
        source_loc 10:11
        get_var a
        source_loc 10:10
        call 1
*/

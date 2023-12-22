// prettier-ignore
for (let a = 1, b, c; 
  a < 10; a++, b++) {
  c += 1;
}

/* EXPECT(let a = 1):
        push_i32 1
        source_loc 2:14
        put_loc 1: a
*/

/* EXPECT(a < 10):
        source_loc 3:3
        get_loc_check 1: a
        push_i32 10
        lt
        source_loc 3:3
        if_false 3:186
*/

/* EXPECT(a++):
        source_loc 3:11
        get_loc_check 1: a
        post_inc
*/

/* EXPECT(c += 1):
        source_loc 4:3
        get_loc_check 3: c
        push_i32 1
        add
        source_loc 4:8
        dup
        put_loc_check 3: c
*/

import { assert } from "./assert.js";

// Regression test for find_var_htab: when a var shadows a block-scoped
// let of the same name, the htab probe must skip entries with
// scope_level != 0. 27 vars are needed to trigger the htab path.
function test_find_var_htab() {
    { let x = "let"; }
    var v0, v1, v2, v3, v4, v5, v6, v7, v8, v9;
    var v10, v11, v12, v13, v14, v15, v16, v17;
    var v18, v19, v20, v21, v22, v23, v24;
    var x = "var";

    function closure() {
        return x;
    }

    assert(closure(), "var", "find_var_htab returned wrong slot index");
}

test_find_var_htab();

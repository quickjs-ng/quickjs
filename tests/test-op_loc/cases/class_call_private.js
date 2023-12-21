class c {
  #b = () => {};

  f() {
    this.#b();
  }
}

/* EXPECT(this.#b()):
        source_loc 5:10
        dup
        get_var_ref 0: "#b"
        get_private_field


        source_loc 5:12
        call_method 0
*/

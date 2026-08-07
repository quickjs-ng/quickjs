import { assert } from "./assert.js";

/* `get` and `set` are only accessor prefixes when what follows can actually
   be an accessor. An accessor is never a generator, so a `*` on the next line
   ends the field definition by automatic semicolon insertion: the class has a
   field named get/set and a separate generator method. */

function syntaxError(src) {
    try {
        (0, eval)(src);
    } catch (e) {
        return e instanceof SyntaxError;
    }
    return false;
}

/* the shape the ASI produces: an own field, and a generator on the prototype */
{
    class C {
        get
        *m() { return 1; }
    }
    const c = new C();
    assert(Object.hasOwn(c, "get"), true);
    assert(c.get, undefined);
    assert(Object.hasOwn(C.prototype, "m"), true);
    assert(typeof C.prototype.m, "function");
    assert(c.m().next().value, 1);
    /* the field is a data property, not an accessor */
    const d = Object.getOwnPropertyDescriptor(c, "get");
    assert("value" in d, true);
    assert(d.value, undefined);
    assert("get" in d, false);
    assert(d.writable, true);
}

{
    class C {
        set
        *m() { return 2; }
    }
    const c = new C();
    assert(Object.hasOwn(c, "set"), true);
    assert(c.set, undefined);
    assert(c.m().next().value, 2);
}

/* several of them in a row */
{
    class C {
        get
        *a() { return 1; }
        set
        *b() { return 2; }
    }
    const c = new C();
    assert(Object.hasOwn(c, "get"), true);
    assert(Object.hasOwn(c, "set"), true);
    assert(c.a().next().value, 1);
    assert(c.b().next().value, 2);
}

/* a computed generator name */
{
    class C {
        get
        *[Symbol.iterator]() { return 4; }
    }
    const c = new C();
    assert(Object.hasOwn(c, "get"), true);
    assert(c[Symbol.iterator]().next().value, 4);
}

/* a private generator */
{
    class C {
        get
        *#m() { return 10; }
        run() { return this.#m().next().value; }
    }
    const c = new C();
    assert(Object.hasOwn(c, "get"), true);
    assert(c.run(), 10);
}

/* `static` applies to the field only: the generator that follows the inserted
   semicolon is a separate, non-static, class element */
{
    class C {
        static get
        *m() { return 3; }
    }
    assert(Object.hasOwn(C, "get"), true);
    assert(C.get, undefined);
    assert(C.m, undefined);
    assert(new C().m().next().value, 3);
}

/* a multi-line comment is a line terminator for ASI too */
{
    class C {
        get /*
        */ *m() { return 5; }
    }
    const c = new C();
    assert(Object.hasOwn(c, "get"), true);
    assert(c.m().next().value, 5);
}

/* an explicit semicolon reaches the same place */
{
    class C {
        get;
        *m() { return 6; }
    }
    const c = new C();
    assert(Object.hasOwn(c, "get"), true);
    assert(c.m().next().value, 6);
}

/* without the line terminator there is nothing to insert: an accessor cannot
   be a generator, so this stays a SyntaxError */
{
    assert(syntaxError("class C { get *m(){} }"), true);
    assert(syntaxError("class C { set *m(){} }"), true);
    assert(syntaxError("class C { static get *m(){} }"), true);
}

/* object literals have no field definitions, so ASI has nothing to insert
   there and the line terminator changes nothing */
{
    assert(syntaxError("({ get\n *m(){} })"), true);
    assert(syntaxError("({ set\n *m(){} })"), true);
    assert(syntaxError("({ get *m(){} })"), true);
}

/* the accessor forms must keep working, with or without a line terminator
   between the prefix and the name */
{
    class C {
        get m() { return 7; }
        set m(v) { this._v = v; }
        get
        n() { return 8; }
    }
    const c = new C();
    assert(c.m, 7);
    c.m = 1;
    assert(c._v, 1);
    assert(c.n, 8);
    assert(Object.hasOwn(c, "get"), false);
    const d = Object.getOwnPropertyDescriptor(C.prototype, "n");
    assert(typeof d.get, "function");
}

/* and so must the other field spellings of a member named get/set */
{
    class C {
        get = 7;
        set;
        static get = 9;
    }
    const c = new C();
    assert(c.get, 7);
    assert(Object.hasOwn(c, "set"), true);
    assert(c.set, undefined);
    assert(C.get, 9);
}

{
    class C { get }
    assert(Object.hasOwn(new C(), "get"), true);
}

/* a getter *named* get or set is unaffected */
{
    class C {
        get get() { return 11; }
        get set() { return 12; }
    }
    const c = new C();
    assert(c.get, 11);
    assert(c.set, 12);
    assert(Object.hasOwn(c, "get"), false);
}

/* the semicolon goes in front of the offending token, so only a `*` that is
   itself preceded by the line terminator ends the field; a token that keeps
   the accessor grammar alive across the newline does not */
{
    /* `get async` is a getter named async, and its `*` is on the same line */
    assert(syntaxError("class C { get \n async *m(){} }"), true, "get/async *");
    assert(syntaxError("class C { get \n async m(){} }"), true, "get/async m");
    assert(syntaxError("class C { get \n static *m(){} }"), true, "get/static *");
    assert(syntaxError("class C { set \n static *m(){} }"), true, "set/static *");

    /* likewise a second get: `get get` is a getter named get, so the `*` is
       an error however it is separated from it */
    assert(syntaxError("class C { get \n get \n *m(){} }"), true, "get/get *");

    /* a `*` with nothing after it is still an error */
    assert(syntaxError("class C { get \n * }"), true, "get/* alone");
    assert(syntaxError("class C { get \n *m }"), true, "get/*m without body");
}

/* the generator the field is followed by may be named anything, including
   the words that started all of this */
{
    class C {
        get
        *async() { return "a"; }
        set
        *get() { return "g"; }
    }
    const c = new C();
    assert(Object.hasOwn(c, "get"), true);
    assert(Object.hasOwn(c, "set"), true);
    assert(c.get, undefined);
    assert(c.async().next().value, "a");
    assert(C.prototype.get.call(c).next().value, "g");
}

/* `async` cannot be an async generator across a line terminator either, so
   it becomes a field the same way -- while `static` is a modifier rather
   than a name and keeps applying to what follows it */
{
    class C {
        async
        *m() { return 1; }
    }
    const c = new C();
    assert(Object.hasOwn(c, "async"), true);
    assert(c.async, undefined);
    assert(C.prototype.m.call(c).next().value, 1);

    class D {
        static
        *m() { return 2; }
    }
    assert(Object.hasOwn(new D(), "static"), false);
    assert(D.prototype.m, undefined);
    assert(D.m().next().value, 2);
}

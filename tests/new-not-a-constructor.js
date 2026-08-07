import { assert } from "./assert.js";

/* `new x` where x is not an object is a "not a constructor" TypeError, the
   same as `new x` on an object that is not a constructor. It used to be
   reported as "not a function", which is what a *call* of a non-callable
   reports; the two are distinct errors. */

function message(fn) {
    try {
        fn();
    } catch (e) {
        assert(e instanceof TypeError, true);
        return e.message;
    }
    return "<no throw>";
}

/* every primitive type, both as `new x` and `new x(...)` */
{
    const notObjects = [
        ["undefined", undefined],
        ["null", null],
        ["boolean", true],
        ["number", 1],
        ["double", 1.5],
        ["string", "x"],
        ["symbol", Symbol.iterator],
        ["bigint", 1n],
    ];

    for (const [what, v] of notObjects) {
        assert(message(() => new v), "not a constructor", what);
        assert(message(() => new v()), "not a constructor", `${what} ()`);
        assert(message(() => new v(1, 2, 3)), "not a constructor",
               `${what} (args)`);
        assert(message(() => Reflect.construct(v, [])), "not a constructor",
               `${what} Reflect.construct`);
        assert(message(() => Reflect.construct(Object, [], v)),
               "not a constructor", `${what} newTarget`);
    }

    /* a missing property and an undefined variable reach the same path */
    const o = {};
    assert(message(() => new o.missing()), "not a constructor");
    assert(message(() => new o.missing), "not a constructor");
}

/* the arguments are still evaluated before the check, as for any call */
{
    let evaluated = 0;
    const v = undefined;
    assert(message(() => new v(evaluated++)), "not a constructor");
    assert(evaluated, 1);
}

/* objects that are not constructors keep the same message */
{
    assert(message(() => new {}()), "not a constructor");
    assert(message(() => new Math.max()), "not a constructor");
    assert(message(() => new Symbol()), "not a constructor");
    assert(message(() => new BigInt(1)), "not a constructor");
    assert(message(() => new (() => {})()), "not a constructor");
    assert(message(() => new (async function() {})()), "not a constructor");
    assert(message(() => new (new Proxy({}, {}))()), "not a constructor");
    assert(message(() => new (Reflect.construct)()), "not a constructor");
}

/* a named bytecode function still names itself in the message */
{
    function* g() {}
    assert(message(() => new g()), "g is not a constructor");
    assert(message(() => Reflect.construct(g, [])), "g is not a constructor");
}

/* `extends null` makes super() a construct of a non-object */
{
    class D extends null {
        constructor() {
            super();
        }
    }
    assert(message(() => new D()), "not a constructor");
}

/* calling a non-callable is a *different* error and must keep its message */
{
    const v = undefined;
    assert(message(() => v()), "not a function");
    assert(message(() => (1)()), "not a function");
    assert(message(() => "s"()), "not a function");
    assert(message(() => ({})()), "not a function");
    assert(message(() => Reflect.apply(undefined, null, [])),
           "not a function");

    const o = {};
    assert(message(() => o.missing()), "not a function");
}

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

/* the function-shaped things that are still not constructors */
{
    function* gen() {}
    async function* agen() {}
    const obj = {
        method() {},
        *genMethod() {},
        async asyncMethod() {},
        get accessor() { return 1; },
    };
    class C {
        method() {}
        static staticMethod() {}
        get accessor() { return 1; }
    }
    const accessor = Object.getOwnPropertyDescriptor(obj, "accessor").get;

    const cases = [
        ["generator", gen],
        ["async generator", agen],
        ["method shorthand", obj.method],
        ["generator method", obj.genMethod],
        ["async method", obj.asyncMethod],
        ["getter", accessor],
        ["class method", C.prototype.method],
        ["static class method", C.staticMethod],
        ["bound arrow", (() => {}).bind(null)],
        ["bound method", obj.method.bind(null)],
        ["proxy of a method", new Proxy(obj.method, {})],
        ["proxy of an arrow", new Proxy(() => {}, {})],
    ];
    for (const [what, v] of cases) {
        assert(typeof v, "function", what);
        /* a named function names itself, so only the tail is fixed */
        assert(message(() => new v()).endsWith("not a constructor"), true, what);
        assert(message(() => Reflect.construct(v, [])).endsWith(
                   "not a constructor"), true, `${what} via Reflect.construct`);
        assert(message(() => Reflect.construct(Object, [], v)).endsWith(
                   "not a constructor"), true, `${what} as new.target`);
    }

    /* ... and the ones that are */
    for (const [what, v] of [["class", C], ["function", function() {}],
                             ["bound function", (function() {}).bind(null)],
                             ["proxy of a class", new Proxy(C, {})]]) {
        const r = new v();
        assert(typeof r, "object", what);
    }
}

/* a revoked proxy reports a revoked proxy, not a missing constructor */
{
    const { proxy, revoke } = Proxy.revocable(function() {}, {});
    assert(new proxy() instanceof Object, true);
    revoke();
    let caught = null;
    try {
        new proxy();
    } catch (e) {
        caught = e;
    }
    assert(caught instanceof TypeError, true);

    const bad = Proxy.revocable({}, {});
    bad.revoke();
    let caught2 = null;
    try {
        new bad.proxy();
    } catch (e) {
        caught2 = e;
    }
    assert(caught2 instanceof TypeError, true);
}

/* super() in a derived class whose parent is not a constructor */
{
    const notCtor = () => {};
    class D extends Object {
        constructor() {
            super();
        }
    }
    assert(new D() instanceof D, true);

    let caught = null;
    try {
        const E = class extends Object { constructor() { super(); } };
        Object.setPrototypeOf(E, notCtor);
        new E();
    } catch (e) {
        caught = e;
    }
    assert(caught instanceof TypeError, true);
    assert(caught.message.includes("not a constructor"), true);
}

import * as std from "qjs:std";
import * as bjson from "qjs:bjson";
import { assert } from "./assert.js";

/* An anonymous function assigned to a property is labelled with the property
   name in stack traces (like V8's inferred names), while its observable
   `name` property stays "" as the spec requires (test262:
   language/expressions/assignment/fn-name-lhs-member.js).
   https://github.com/bellard/quickjs/issues/93 */

function stackOf(f) {
    try {
        f();
    } catch (e) {
        return e.stack;
    }
    throw new Error("expected function to throw");
}

// first line of the stack: the frame that threw
function frameOf(f) {
    return stackOf(f).split("\n")[0];
}

function assertFrame(f, label) {
    const frame = frameOf(f);
    assert(frame.startsWith(`    at ${label} (`), true, frame);
}

const dog = {};
dog.bark = function() { throw new Error("woof"); };
dog.meow = () => { throw new Error("meow"); };
dog.named = function realName() { throw new Error("named"); };
dog.paren = (function() { throw new Error("paren"); });
dog.alias = dog.bark;      // not an anonymous function definition
dog.nested = {};
dog.nested.deep = function() { throw new Error("deep"); };
dog.first = dog.second = function() { throw new Error("chained"); };

// observable behaviour is unchanged
assert(dog.bark.name, "");
assert(dog.meow.name, "");
assert(dog.paren.name, "");
assert(dog.first.name, "");
assert(dog.named.name, "realName");
const desc = Object.getOwnPropertyDescriptor(dog.bark, "name");
assert(desc.value, "");
assert(desc.writable, false);
assert(desc.enumerable, false);
assert(desc.configurable, true);

assertFrame(dog.bark, "bark");
assertFrame(dog.meow, "meow");
assertFrame(dog.named, "realName");
assertFrame(dog.paren, "paren");
assertFrame(dog.alias, "bark");
assertFrame(dog.nested.deep, "deep");
assertFrame(dog.first, "second");   // named by the innermost assignment

// bracket access with a string literal is compiled like dot access
dog["quoted"] = function() { throw new Error("quoted"); };
dog['single'] = () => { throw new Error("single"); };
assert(dog.quoted.name, "");
assertFrame(dog.quoted, "quoted");
assertFrame(dog.single, "single");

// the key of a computed member is only known at run time: use the same
// placeholder as V8
const key = "dynamic";
dog[key] = function() { throw new Error("dynamic"); };
dog[1] = () => { throw new Error("index"); };
dog["2"] = () => { throw new Error("numeric string"); };
assert(dog.dynamic.name, "");
assertFrame(dog.dynamic, "<computed>");
assertFrame(dog[1], "<computed>");
assertFrame(dog[2], "<computed>");

// an empty property name is no better than no name
dog[""] = function() { throw new Error("empty"); };
assertFrame(dog[""], "<anonymous>");

// no inference without an assignment target
assertFrame(function() { throw new Error("anon"); }, "<anonymous>");
assertFrame(() => { throw new Error("anon"); }, "<anonymous>");

// `this.x = function() {}` in a constructor and prototype methods, the
// patterns from bellard/quickjs#93
function Cat() {
    this.purr = function() { throw new Error("purr"); };
    this.hiss = () => { throw new Error("hiss"); };
}
Cat.prototype.scratch = function() { throw new Error("scratch"); };
const cat = new Cat();
assert(cat.purr.name, "");
assert(Cat.prototype.scratch.name, "");
assertFrame(cat.purr, "purr");
assertFrame(cat.hiss, "hiss");
assertFrame(cat.scratch, "scratch");

// logical assignment
const cfg = {};
cfg.onError ??= function() { throw new Error("onError"); };
cfg.onLoad ||= () => { throw new Error("onLoad"); };
assert(cfg.onError.name, "");
assertFrame(cfg.onError, "onError");
assertFrame(cfg.onLoad, "onLoad");

// private fields
class Widget {
    #onClick;
    constructor() {
        this.#onClick = () => { throw new Error("click"); };
    }
    click() { this.#onClick(); }
}
assertFrame(() => new Widget().click(), "#onClick");

// an explicit `name` property wins over the inferred name
const custom = {};
custom.fn = function() { throw new Error("custom"); };
Object.defineProperty(custom.fn, "name", { value: "customName" });
assertFrame(custom.fn, "customName");

// NamedEvaluation for identifiers still sets `name`
{
    let f = function() {};
    assert(f.name, "f");
}

// code compiled through eval
const ev = {};
eval("ev.fromEval = function() { throw new Error('eval'); }");
assert(ev.fromEval.name, "");
assertFrame(ev.fromEval, "fromEval");

// CallSite.getFunctionName() for Error.prepareStackTrace
{
    let frames;
    Error.prepareStackTrace = (_, f) => { frames = f; return f; };
    try {
        dog.bark();
    } catch (e) {
        assert(e.stack, frames);
    } finally {
        Error.prepareStackTrace = undefined;
    }
    assert(frames[0].getFunctionName(), "bark");
}

// obj["prop"] is compiled to the same bytecode as obj.prop: check the
// operations that special-case the get_field opcode in the parser
{
    const o = { n: 1, "a b": 2, f() { return this; } };
    assert(o["n"], 1);
    assert(o["a b"], 2);
    assert(o["f"](), o);
    assert(o?.["n"], 1);
    assert(undefined?.["n"] === undefined);
    assert(typeof o["missing"], "undefined");
    o["n"] += 2;
    assert(o.n, 3);
    o["n"]++;
    assert(o.n, 4);
    o["n"] ??= 9;
    assert(o.n, 4);
    o["n"] &&= 5;
    assert(o.n, 5);
    [o["n"]] = [6];
    assert(o.n, 6);
    ({ n: o["n"] } = { n: 7 });
    assert(o.n, 7);
    assert(delete o["n"], true);
    assert("n" in o, false);
    assert([1, 2, 3]["length"], 3);
    const arr = [7, 8];
    assert(arr["1"], 8);
    arr["0"] = 9;
    assert(arr[0], 9);
    assert(o["__proto__"], Object.prototype);
    class B { m() { return "B"; } }
    class D extends B { m() { return super["m"](); } }
    assert(new D().m(), "B");
    let caught;
    try { undefined["n"]; } catch (e) { caught = e; }
    assert(caught.message, "cannot read property 'n' of undefined");
}

// the inferred name survives bytecode serialization
{
    let o = std.evalScript(
        "var bc = {}; bc.serialized = function() { throw new Error('bc'); }; bc.serialized",
        { compile_only: true });
    const buf = bjson.write(o, bjson.WRITE_OBJ_BYTECODE);
    o = bjson.read(buf, 0, buf.byteLength, bjson.READ_OBJ_BYTECODE);
    const fn = std.evalScript(o, { eval_function: true });
    assert(fn.name, "");
    assertFrame(fn, "serialized");
}

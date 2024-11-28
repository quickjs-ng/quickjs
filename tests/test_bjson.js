import * as std from "qjs:std";
import * as bjson from "qjs:bjson";
import { assert } from "./assert.js";

function base64decode(s) {
    var A = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    var n = s.indexOf("=");
    if (n < 0) n = s.length;
    if (n & 3 === 1) throw Error("bad base64"); // too much padding
    var r = new Uint8Array(3 * (n>>2) + (n>>1 & 1) + (n & 1));
    var a, b, c, d, i, j;
    a = b = c = d = i = j = 0;
    while (i+3 < n) {
        a = A.indexOf(s[i++]);
        b = A.indexOf(s[i++]);
        c = A.indexOf(s[i++]);
        d = A.indexOf(s[i++]);
        if (~63 & (a|b|c|d)) throw Error("bad base64");
        r[j++] = a<<2 | b>>4;
        r[j++] = 255 & b<<4 | c>>2;
        r[j++] = 255 & c<<6 | d;
    }
    switch (n & 3) {
    case 2:
        a = A.indexOf(s[i++]);
        b = A.indexOf(s[i++]);
        if (~63 & (a|b)) throw Error("bad base64");
        if (b & 15) throw Error("bad base64");
        r[j++] = a<<2 | b>>4;
        break;
    case 3:
        a = A.indexOf(s[i++]);
        b = A.indexOf(s[i++]);
        c = A.indexOf(s[i++]);
        if (~63 & (a|b|c)) throw Error("bad base64");
        if (c & 3) throw Error("bad base64");
        r[j++] = a<<2 | b>>4;
        r[j++] = 255 & b<<4 | c>>2;
        break;
    }
    return r.buffer;
}

function toHex(a)
{
    var i, s = "", tab, v;
    tab = new Uint8Array(a);
    for(i = 0; i < tab.length; i++) {
        v = tab[i].toString(16);
        if (v.length < 2)
            v = "0" + v;
        if (i !== 0)
            s += " ";
        s += v;
    }
    return s;
}

function isArrayLike(a)
{
    return Array.isArray(a) ||
        (a instanceof Uint8ClampedArray) ||
        (a instanceof Uint8Array) ||
        (a instanceof Uint16Array) ||
        (a instanceof Uint32Array) ||
        (a instanceof Int8Array) ||
        (a instanceof Int16Array) ||
        (a instanceof Int32Array) ||
        (a instanceof Float16Array) ||
        (a instanceof Float32Array) ||
        (a instanceof Float64Array);
}

function toStr(a)
{
    var s, i, props, prop;

    switch(typeof(a)) {
    case "object":
        if (a === null)
            return "null";
        if (a instanceof Date) {
            s = "Date(" + toStr(a.valueOf()) + ")";
        } else if (a instanceof Number) {
            s = "Number(" + toStr(a.valueOf()) + ")";
        } else if (a instanceof String) {
            s = "String(" + toStr(a.valueOf()) + ")";
        } else if (a instanceof Boolean) {
            s = "Boolean(" + toStr(a.valueOf()) + ")";
        } else if (isArrayLike(a)) {
            s = "[";
            for(i = 0; i < a.length; i++) {
                if (i != 0)
                    s += ",";
                s += toStr(a[i]);
            }
            s += "]";
        } else {
            props = Object.keys(a);
            s = "{";
            for(i = 0; i < props.length; i++) {
                if (i != 0)
                    s += ",";
                prop = props[i];
                s += prop + ":" + toStr(a[prop]);
            }
            s += "}";
        }
        return s;
    case "undefined":
        return "undefined";
    case "string":
        return JSON.stringify(a);
    case "number":
        if (a == 0 && 1 / a < 0)
            return "-0";
        else
            return a.toString();
        break;
    default:
        return a.toString();
    }
}

function bjson_test(a)
{
    var buf, r, a_str, r_str;
    a_str = toStr(a);
    buf = bjson.write(a);
    if (0) {
        print(a_str, "->", toHex(buf));
    }
    r = bjson.read(buf, 0, buf.byteLength);
    r_str = toStr(r);
    if (a_str != r_str) {
        print(a_str);
        print(r_str);
        assert(false);
    }
}

function bjson_test_arraybuffer()
{
    var buf, array_buffer;

    array_buffer = new ArrayBuffer(4);
    assert(array_buffer.byteLength, 4);
    assert(array_buffer.maxByteLength, 4);
    assert(array_buffer.resizable, false);
    buf = bjson.write(array_buffer);
    array_buffer = bjson.read(buf, 0, buf.byteLength);
    assert(array_buffer.byteLength, 4);
    assert(array_buffer.maxByteLength, 4);
    assert(array_buffer.resizable, false);

    array_buffer = new ArrayBuffer(4, {maxByteLength: 4});
    assert(array_buffer.byteLength, 4);
    assert(array_buffer.maxByteLength, 4);
    assert(array_buffer.resizable, true);
    buf = bjson.write(array_buffer);
    array_buffer = bjson.read(buf, 0, buf.byteLength);
    assert(array_buffer.byteLength, 4);
    assert(array_buffer.maxByteLength, 4);
    assert(array_buffer.resizable, true);

    array_buffer = new ArrayBuffer(4, {maxByteLength: 8});
    assert(array_buffer.byteLength, 4);
    assert(array_buffer.maxByteLength, 8);
    assert(array_buffer.resizable, true);
    buf = bjson.write(array_buffer);
    array_buffer = bjson.read(buf, 0, buf.byteLength);
    assert(array_buffer.byteLength, 4);
    assert(array_buffer.maxByteLength, 8);
    assert(array_buffer.resizable, true);
}

/* test multiple references to an object including circular
   references */
function bjson_test_reference()
{
    var array, buf, i, n, array_buffer;
    n = 16;
    array = [];
    for(i = 0; i < n; i++)
        array[i] = {};
    array_buffer = new ArrayBuffer(n);
    for(i = 0; i < n; i++) {
        array[i].next = array[(i + 1) % n];
        array[i].idx = i;
        array[i].typed_array = new Uint8Array(array_buffer, i, 1);
    }
    buf = bjson.write(array, bjson.WRITE_OBJ_REFERENCE);

    array = bjson.read(buf, 0, buf.byteLength, bjson.READ_OBJ_REFERENCE);

    /* check the result */
    for(i = 0; i < n; i++) {
        assert(array[i].next, array[(i + 1) % n]);
        assert(array[i].idx, i);
        assert(array[i].typed_array.buffer, array_buffer);
        assert(array[i].typed_array.length, 1);
        assert(array[i].typed_array.byteOffset, i);
    }
}

function bjson_test_regexp()
{
    var buf, r;

    bjson_test(/xyzzy/);
    bjson_test(/xyzzy/digu);

    buf = bjson.write(/(?<𝓓𝓸𝓰>dog)/);
    r = bjson.read(buf, 0, buf.byteLength);
    assert("sup dog".match(r).groups["𝓓𝓸𝓰"], "dog");
}

function bjson_test_map()
{
    var buf, r, xs;

    xs = [["key", "value"]];
    buf = bjson.write(new Map(xs));
    r = bjson.read(buf, 0, buf.byteLength);
    assert(r instanceof Map);
    assert([...r].toString(), xs.toString());
}

function bjson_test_set()
{
    var buf, r, xs;

    xs = ["one", "two", "three"];
    buf = bjson.write(new Set(xs));
    r = bjson.read(buf, 0, buf.byteLength);
    assert(r instanceof Set);
    assert([...r].toString(), xs.toString());
}

function bjson_test_symbol()
{
    var buf, r, o;

    o = {[Symbol.toStringTag]: "42"};
    buf = bjson.write(o);
    r = bjson.read(buf, 0, buf.byteLength);
    assert(o.toString(), r.toString());

    o = Symbol('foo');
    buf = bjson.write(o);
    r = bjson.read(buf, 0, buf.byteLength);
    assert(o.toString(), r.toString());
    assert(o !== r);

    o = Symbol.for('foo');
    buf = bjson.write(o);
    r = bjson.read(buf, 0, buf.byteLength);
    assert(o, r);

    o = Symbol.toStringTag;
    buf = bjson.write(o);
    r = bjson.read(buf, 0, buf.byteLength);
    assert(o, r);
}

function bjson_test_bytecode()
{
    var buf, o, r, e, i;

    o = std.evalScript(";(function f(o){ return o.i })", {compile_only: true});
    buf = bjson.write(o, /*JS_WRITE_OBJ_BYTECODE*/(1 << 0));
    try {
        bjson.read(buf, 0, buf.byteLength);
    } catch (_e) {
        e = _e;
    }
    assert(String(e), "SyntaxError: no bytecode allowed");

    o = bjson.read(buf, 0, buf.byteLength, /*JS_READ_OBJ_BYTECODE*/(1 << 0));
    assert(String(o), "[function bytecode]");
    o = std.evalScript(o, {eval_function: true});
    for (i = 0; i < 42; i++) o({i}); // exercise o.i IC
}

function bjson_test_fuzz()
{
    var corpus = [
        "EBAAAAAABGA=",
        "EObm5oIt",
        "EAARABMGBgYGBgYGBgYGBv////8QABEALxH/vy8R/78=",
    ];
    for (var input of corpus) {
        var buf = base64decode(input);
        try {
            bjson.read(buf, 0, buf.byteLength);
        } catch (e) {
            // okay, ignore
        }
    }
}

function bjson_test_all()
{
    var obj;

    bjson_test({x:1, y:2, if:3});
    bjson_test([1, 2, 3]);
    bjson_test([1.0, "aa", true, false, undefined, null, NaN, -Infinity, -0.0]);
    if (typeof BigInt !== "undefined") {
        bjson_test([BigInt("1"), -BigInt("0x123456789"),
               BigInt("0x123456789abcdef123456789abcdef")]);
    }

    bjson_test([new Date(1234), new String("abc"), new Number(-12.1), new Boolean(true)]);

    bjson_test(new Int32Array([123123, 222111, -32222]));
    bjson_test(new Float16Array([1024, 1024.5]));
    bjson_test(new Float64Array([123123, 222111.5]));

    /* tested with a circular reference */
    obj = {};
    obj.x = obj;
    try {
        bjson.write(obj);
        assert(false);
    } catch(e) {
        assert(e instanceof TypeError);
    }

    bjson_test_arraybuffer();
    bjson_test_reference();
    bjson_test_regexp();
    bjson_test_map();
    bjson_test_set();
    bjson_test_symbol();
    bjson_test_bytecode();
    bjson_test_fuzz();
}

bjson_test_all();

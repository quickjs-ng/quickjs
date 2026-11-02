import { assert, assertArrayEquals, assertThrows } from "./assert.js";

function test_matchall_order()
{
    const log = [];
    const pattern = {
        get [Symbol.match]() { log.push("match"); return true; },
        get flags() {
            log.push("flags");
            return { toString() { log.push("flags.toString"); return ""; } };
        },
        get [Symbol.matchAll]() { throw new RangeError("matcher accessed"); },
    };
    assertThrows(TypeError, () => "".matchAll(pattern));
    assertArrayEquals(log, ["match", "flags", "flags.toString"]);

    log.length = 0;
    const receiver = { toString() { throw new Error("receiver converted"); } };
    let flags = "g";
    const custom = {
        get [Symbol.match]() { log.push("match"); return true; },
        get flags() { log.push("flags"); return flags; },
        get [Symbol.matchAll]() {
            log.push("matchAll");
            flags = "";
            return function(value) {
                assert(this, custom);
                assert(value, receiver);
                log.push("call");
                return 42;
            };
        },
    };
    assert(String.prototype.matchAll.call(receiver, custom), 42);
    assertArrayEquals(log, ["match", "flags", "matchAll", "call"]);
}

function test_regexp_create()
{
    for (const method of ["match", "search", "matchAll"]) {
        const log = [];
        const pattern = new Proxy({
            toString() { log.push("pattern.toString"); return "b"; },
        }, {
            get(target, key) {
                if ([Symbol.match, Symbol.search, Symbol.matchAll].includes(key))
                    log.push(key.description);
                return Reflect.get(target, key);
            },
        });
        const receiver = {
            toString() { log.push("receiver.toString"); return "abc"; },
        };
        const result = String.prototype[method].call(receiver, pattern);
        const expected = method === "matchAll" ?
            ["Symbol.match", "Symbol.matchAll"] : [Symbol[method].description];
        expected.push("receiver.toString", "pattern.toString");
        assertArrayEquals(log, expected);
        if (method === "search") {
            assert(result, 1);
        } else {
            const match = method === "matchAll" ? result.next().value : result;
            assert(match[0], "b");
            assert(match.index, 1);
        }
    }
}

function test_regexp_pattern_conversion()
{
    for (const method of ["match", "search", "matchAll"]) {
        const pattern = /a/g;
        pattern[Symbol[method]] = undefined;
        pattern.lastIndex = 2;
        let converted = 0;
        pattern.toString = function() { converted++; return "b"; };
        const result = "b"[method](pattern);
        if (method === "search") {
            assert(result, 0);
        } else {
            const match = method === "matchAll" ? result.next().value : result;
            assert(match[0], "b");
            assert(match.index, 0);
        }
        assert(converted, 1);
        assert(pattern.lastIndex, 2);
    }

    const regexp_like = {
        [Symbol.match]: true,
        flags: "g",
        get source() { throw new Error("source accessed"); },
        toString() { return "b"; },
    };
    assert("abc".matchAll(regexp_like).next().value[0], "b");
    const search_pattern = {
        get [Symbol.match]() { throw new Error("match accessed"); },
        toString() { return "b"; },
    };
    assert("abc".search(search_pattern), 1);
}

function test_regexp_create_exceptions()
{
    for (const method of ["match", "search", "matchAll"]) {
        const pattern = {
            toString() { throw new RangeError("pattern converted"); },
        };
        assertThrows(RangeError, () => ""[method](pattern));
        assertThrows(SyntaxError, () => ""[method]({ toString() { return "["; } }));
        assertThrows(TypeError, () => ""[method](Symbol()));
        assertThrows(TypeError, () => String.prototype[method].call(null, pattern));
        const result = ""[method](undefined);
        if (method === "search")
            assert(result, 0);
        else
            assert((method === "matchAll" ? result.next().value : result)[0], "");
    }
}

test_matchall_order();
test_regexp_create();
test_regexp_pattern_conversion();
test_regexp_create_exceptions();

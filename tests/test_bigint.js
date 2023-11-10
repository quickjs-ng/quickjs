"use strict";

function assert(actual, expected, message) {
    if (arguments.length == 1)
        expected = true;

    if (actual === expected)
        return;

    if (actual !== null && expected !== null
    &&  typeof actual == 'object' && typeof expected == 'object'
    &&  actual.toString() === expected.toString())
        return;

    throw Error("assertion failed: got |" + actual + "|" +
                ", expected |" + expected + "|" +
                (message ? " (" + message + ")" : ""));
}

function assertThrows(err, func)
{
    var ex;
    ex = false;
    try {
        func();
    } catch(e) {
        ex = true;
        assert(e instanceof err);
    }
    assert(ex, true, "exception expected");
}

// load more elaborate version of assert if available
try { __loadScript("test_assert.js"); } catch(e) {}

/*----------------*/

function bigint_pow(a, n)
{
    var r, i;
    r = 1n;
    for(i = 0n; i < n; i++)
        r *= a;
    return r;
}

/* a must be < b */
function test_less(a, b)
{
    assert(a < b);
    assert(!(b < a));
    assert(a <= b);
    assert(!(b <= a));
    assert(b > a);
    assert(!(a > b));
    assert(b >= a);
    assert(!(a >= b));
    assert(a != b);
    assert(!(a == b));
}

/* a must be numerically equal to b */
function test_eq(a, b)
{
    assert(a == b);
    assert(b == a);
    assert(!(a != b));
    assert(!(b != a));
    assert(a <= b);
    assert(b <= a);
    assert(!(a < b));
    assert(a >= b);
    assert(b >= a);
    assert(!(a > b));
}

function test_bigint1()
{
    var a, r;

    test_less(2n, 3n);
    test_eq(3n, 3n);

    test_less(2, 3n);
    test_eq(3, 3n);

    test_less(2.1, 3n);
    test_eq(Math.sqrt(4), 2n);

    a = bigint_pow(3n, 100n);
    assert((a - 1n) != a);
    assert(a == 515377520732011331036461129765621272702107522001n);
    assert(a == 0x5a4653ca673768565b41f775d6947d55cf3813d1n);

    r = 1n << 31n;
    assert(r, 2147483648n, "1 << 31n === 2147483648n");
    
    r = 1n << 32n;
    assert(r, 4294967296n, "1 << 32n === 4294967296n");
}

function test_bigint2()
{
    assert(BigInt(""), 0n);
    assert(BigInt("  123"), 123n);
    assert(BigInt("  123   "), 123n);
    assertThrows(SyntaxError, () => { BigInt("+") } );
    assertThrows(SyntaxError, () => { BigInt("-") } );
    assertThrows(SyntaxError, () => { BigInt("\x00a") } );
    assertThrows(SyntaxError, () => { BigInt("  123  r") } );
}

function test_divrem(div1, a, b, q)
{
    var div, divrem, t;
    div = BigInt[div1];
    divrem = BigInt[div1 + "rem"];
    assert(div(a, b) == q);
    t = divrem(a, b);
    assert(t[0] == q);
    assert(a == b * q + t[1]);
}

function test_idiv1(div, a, b, r)
{
    test_divrem(div, a, b, r[0]);
    test_divrem(div, -a, b, r[1]);
    test_divrem(div, a, -b, r[2]);
    test_divrem(div, -a, -b, r[3]);
}

/* QuickJS BigInt extensions */
function test_bigint_ext()
{
    var r;
    assert(BigInt.floorLog2(0n) === -1n);
    assert(BigInt.floorLog2(7n) === 2n);

    assert(BigInt.sqrt(0xffffffc000000000000000n) === 17592185913343n);
    r = BigInt.sqrtrem(0xffffffc000000000000000n);
    assert(r[0] === 17592185913343n);
    assert(r[1] === 35167191957503n);

    test_idiv1("tdiv", 3n, 2n, [1n, -1n, -1n, 1n]);
    test_idiv1("fdiv", 3n, 2n, [1n, -2n, -2n, 1n]);
    test_idiv1("cdiv", 3n, 2n, [2n, -1n, -1n, 2n]);
    test_idiv1("ediv", 3n, 2n, [1n, -2n, -1n, 2n]);
}

test_bigint1();
test_bigint2();
test_bigint_ext();

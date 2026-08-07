import { assert } from "./assert.js";

/* An ObjectLiteral or ArrayLiteral only turns into a destructuring pattern
   when it sits at the head of an AssignmentExpression. Anywhere else it is
   just a literal, and a following '=' is an assignment to a non-reference,
   i.e. an early SyntaxError. */

function syntaxError(src) {
    try {
        eval(src);
    } catch (e) {
        return e instanceof SyntaxError;
    }
    return false;
}

function evaluate(src) {
    return (0, eval)(src);
}

/* -------------------------------------------------------------------------
   the literal is not at the head: it stays a literal and the '=' is invalid
   ------------------------------------------------------------------------- */
{
    /* binary operators */
    assert(syntaxError("var a; 1 + [a] = [1];"), true, "1 + [a] = [1]");
    assert(syntaxError("var a; 1 + {a} = {a: 1};"), true, "1 + {a} = {}");
    assert(syntaxError("var a; 1 - [a] = [1];"), true, "1 - [a] = [1]");
    assert(syntaxError("var a; 1 * [a] = [1];"), true, "1 * [a] = [1]");
    assert(syntaxError("var a; 2 ** [a] = [1];"), true, "2 ** [a] = [1]");
    assert(syntaxError("var a; 1 | [a] = [1];"), true, "1 | [a] = [1]");
    assert(syntaxError("var a; 1 < [a] = [1];"), true, "1 < [a] = [1]");
    assert(syntaxError("var a; 1 instanceof [a] = [1];"), true, "instanceof");
    assert(syntaxError("var a; 'x' in {a} = {a: 1};"), true, "in");

    /* short circuit operators: the right hand side is not a head either */
    assert(syntaxError("var a, o = 1; o || [a] = [1];"), true, "|| [a] =");
    assert(syntaxError("var a, o = 1; o && [a] = [1];"), true, "&& [a] =");
    assert(syntaxError("var a, o = 1; o ?? [a] = [1];"), true, "?? [a] =");
    assert(syntaxError("var a, o = 1; o || {a} = {a: 1};"), true, "|| {a} =");
    assert(syntaxError("var a, o = 1; o && {a} = {a: 1};"), true, "&& {a} =");

    /* unary operators */
    assert(syntaxError("var a; void [a] = [1];"), true, "void");
    assert(syntaxError("var a; typeof {a} = {a: 1};"), true, "typeof");
    assert(syntaxError("var a; !{a} = {a: 1};"), true, "!");
    assert(syntaxError("var a; -[a] = [1];"), true, "unary -");
    assert(syntaxError("var a; +[a] = [1];"), true, "unary +");
    assert(syntaxError("var a; ~[a] = [1];"), true, "~");
    assert(syntaxError("var a; delete [a] = [1];"), true, "delete");

    /* the comma operator: only the first operand of each element is a head,
       the literal here follows a binary operator */
    assert(syntaxError("var a; (0, 1 + [a] = [1]);"), true, "comma element");

    /* a private name 'in' check is not a head either */
    assert(syntaxError("var a; class C { #p; m(o) { #p in [a] = [1]; } }"),
           true, "#p in [a] =");
    assert(syntaxError("class C { #p; constructor() { #p in {} = 0; } }"),
           true, "#p in {} =");
}

/* -------------------------------------------------------------------------
   the head positions that must keep working
   ------------------------------------------------------------------------- */
{
    /* plain statement level */
    let a, b;
    [a, b] = [1, 2];
    assert(a, 1);
    assert(b, 2);

    ({ a, b } = { a: 3, b: 4 });
    assert(a, 3);
    assert(b, 4);

    /* holes, defaults, rest and nesting */
    let r;
    [a, , b, ...r] = [1, 2, 3, 4, 5];
    assert(a, 1);
    assert(b, 3);
    assert(r.join(","), "4,5");

    ({ a = 10, ...r } = { b: 1, c: 2 });
    assert(a, 10);
    assert(JSON.stringify(r), '{"b":1,"c":2}');

    let c;
    [a, [b, { c }]] = [1, [2, { c: 3 }]];
    assert(a, 1);
    assert(b, 2);
    assert(c, 3);

    /* empty patterns */
    [] = [];
    ({} = {});

    /* member expression targets */
    const o = {};
    [o.x] = [7];
    assert(o.x, 7);
    ({ y: o.y } = { y: 8 });
    assert(o.y, 8);
}

{
    /* right hand side of an assignment is a fresh AssignmentExpression */
    let a, b, x;
    x = ([a] = [1]);
    assert(a, 1);
    assert(x.join(","), "1");

    [a] = [b] = [2];
    assert(a, 2);
    assert(b, 2);

    /* compound assignment right hand side */
    let n = 1;
    n += ([a] = [4])[0];
    assert(n, 5);
}

{
    /* both branches of a conditional restart an AssignmentExpression */
    let a, b;
    const t = true ? ([a] = [1]) : 0;
    assert(a, 1);
    assert(t.join(","), "1");
    false ? 0 : ({ b } = { b: 2 });
    assert(b, 2);

    /* without the parentheses too: '?' and ':' each start a new head */
    let c, d;
    true ? [c] = [5] : 0;
    assert(c, 5);
    false ? 0 : [d] = [6];
    assert(d, 6);
}

{
    /* each element of a comma expression is its own AssignmentExpression */
    let a, b;
    (([a] = [1]), ({ b } = { b: 2 }));
    assert(a, 1);
    assert(b, 2);

    /* ... and so is every argument of a call */
    let c, d;
    const seen = ((x, y) => [x, y])(([c] = [3])[0], ({ d } = { d: 4 }).d);
    assert(c, 3);
    assert(d, 4);
    assert(seen.join(","), "3,4");
}

{
    /* array/object literal elements and properties are AssignmentExpressions */
    let a, b;
    const arr = [[a] = [1], ({ b } = { b: 2 })];
    assert(a, 1);
    assert(b, 2);
    assert(arr[0].join(","), "1");

    let c;
    const obj = { p: ([c] = [3]) };
    assert(c, 3);
    assert(obj.p.join(","), "3");

    /* computed keys and template substitutions */
    let d, e;
    const k = { [([d] = ["k"])[0]]: 1 };
    assert(d, "k");
    assert(k.k, 1);
    assert(`${([e] = [9])[0]}`, "9");
}

{
    /* initialisers and default parameter values */
    let a;
    const init = ([a] = [1]);
    assert(a, 1);
    assert(init.join(","), "1");

    let b;
    function f(p = ([b] = [2])) { return p; }
    assert(f().join(","), "2");
    assert(b, 2);

    /* arrow body */
    let c;
    const g = () => ([c] = [3]);
    assert(g().join(","), "3");
    assert(c, 3);

    /* return / throw operands */
    let d;
    function h() { return [d] = [4]; }
    assert(h().join(","), "4");
    assert(d, 4);
}

{
    /* for-of and for-in bind through the same destructuring path */
    let a, b;
    for ([a, b] of [[1, 2]]) ;
    assert(a, 1);
    assert(b, 2);

    for ({ a, b } of [{ a: 3, b: 4 }]) ;
    assert(a, 3);
    assert(b, 4);

    let k;
    for ([k] in { ab: 1 }) ;
    assert(k, "a");

    /* the head of a for(;;) is an Expression: each element is a head */
    let c = 0, d;
    for ([d] = [0]; d < 3; [d] = [d + 1]) c += d;
    assert(c, 3);
    assert(d, 3);
}

/* -------------------------------------------------------------------------
   literals in non head positions still evaluate as literals
   ------------------------------------------------------------------------- */
{
    assert(1 + [2], "12");
    assert([1] + [2], "12");
    assert(typeof {}, "object");
    assert((0 || [1, 2]).join(","), "1,2");
    assert((1 && { a: 1 }).a, 1);
    assert((null ?? [3]).join(","), "3");
    assert(evaluate("1 + [1] == '11'"), true);

    /* '==' is not '=': the literal stays a literal and nothing throws */
    assert(evaluate("var a = 1; [a] == '1'"), true);
    assert(evaluate("var a = 1; ({a}) != null"), true);

    /* an '=' further inside the literal does not make the literal a
       pattern: it belongs to the nested AssignmentExpression */
    let a;
    const arr = [1, 2] + ([a] = [3]);
    assert(a, 3);
    assert(arr, "1,23");
}

/* -------------------------------------------------------------------------
   destructuring failures still report at runtime, not at parse time
   ------------------------------------------------------------------------- */
{
    let a;
    let threw = false;
    try {
        eval("[a] = null;");
    } catch (e) {
        threw = e instanceof TypeError;
    }
    assert(threw, true, "[a] = null is a runtime TypeError");
}

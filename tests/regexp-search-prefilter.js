import { assert } from "./assert.js";

/* An unanchored search may skip input positions that cannot begin a match,
   but only positions that really cannot: the set of possible first characters
   has to stay a superset of the truth. The cases below are the ones where
   getting that set wrong is easy -- case folding that crosses the Latin-1
   boundary, ranges that end just below it, `^` with and without the m flag,
   astral characters and lone surrogates. */

function m(re, s) {
    re.lastIndex = 0;
    const r = re.exec(s);
    return r === null ? "null" : r.index + ":" + r[0];
}

function all(re, s) {
    return Array.from(s.matchAll(re), r => r.index + ":" + r[0]).join(" ");
}

/* a literal first character, found at either end and in the middle */
{
    assert(m(/abc/, "abc"), "0:abc");
    assert(m(/abc/, "xxabc"), "2:abc");
    assert(m(/abc/, "abcxx"), "0:abc");
    assert(m(/abc/, "ab"), "null");
    assert(m(/abc/, ""), "null");
    assert(m(/abc/, "ababc"), "2:abc");

    /* the candidate character appears many times before the real match */
    assert(m(/ab/, "a".repeat(50) + "ab"), "50:ab");
    assert(m(/ab/, "a".repeat(50)), "null");

    /* long inputs, match at the very last possible position */
    const long = "x".repeat(5000);
    assert(m(/xy/, long + "y"), "4999:xy");
    assert(m(/zy/, long), "null");
    assert(m(/x/, long), "0:x");

    /* the first character is not in the buffer's range at all */
    assert(m(/é/, "abc"), "null");
    assert(m(/中/, "abc"), "null");
    assert(m(/a/, "中中中"), "null");
}

/* a case-insensitive first character. Without the u flag the folding stays
   inside ASCII; with it, characters far above the Latin-1 range fold down
   into it, and the candidate set has to keep them. */
{
    assert(m(/abc/i, "xABC"), "1:ABC");
    assert(m(/ABC/i, "xabc"), "1:abc");
    assert(m(/é/i, "xÉ"), "1:É");
    assert(m(/É/i, "xé"), "1:é");

    /* U+212A KELVIN SIGN folds to "k", U+017F LATIN SMALL LETTER LONG S
       folds to "s", but only under the u flag */
    assert(m(/k/iu, "xK"), "1:K");
    assert(m(/kelvin/iu, "xKelvin"), "1:Kelvin");
    assert(m(/s/iu, "xſ"), "1:ſ");
    assert(m(/k/i, "xK"), "null");
    assert(m(/s/i, "xſ"), "null");

    /* and the other direction */
    assert(m(/K/iu, "xk"), "1:k");
    assert(m(/ſ/iu, "xs"), "1:s");
}

/* a character class first, including ones whose range stops just below or
   reaches just above the Latin-1 boundary */
{
    assert(m(/[abc]d/, "xxbd"), "2:bd");
    assert(m(/[abc]d/, "xxed"), "null");
    assert(m(/[a-c]/, "zzzb"), "3:b");
    assert(m(/[^a-c]/, "aaaz"), "3:z");
    assert(m(/[^a-c]/, "aaa"), "null");
    assert(m(/\d+/, "abc123"), "3:123");
    assert(m(/\w/, "!!!a"), "3:a");
    assert(m(/\s/, "ab c"), "2: ");
    assert(m(/[\s\S]/, "a"), "0:a");

    /* a range that ends at U+00FF must not match U+0100 */
    assert(m(/[à-ÿ]/, "ABÿ"), "2:ÿ");
    assert(m(/[à-ÿ]/, "ABĀ"), "null");
    /* a range that reaches past it must */
    assert(m(/[à-Ő]/, "ABĀ"), "2:Ā");
    /* a wholly non-Latin-1 range */
    assert(m(/[一-鿿]/, "ab中"), "2:中");
    assert(m(/[一-鿿]/, "abc"), "null");

    /* case-insensitive ranges fold the same way single characters do */
    assert(m(/[a-c]/i, "ZZB"), "2:B");
    assert(m(/[j-l]/iu, "xyK"), "2:K");
    assert(m(/[r-t]/iu, "xyſ"), "2:ſ");
    assert(m(/[à-ÿ]/i, "ABÀ"), "2:À");

    /* several disjoint ranges */
    assert(m(/[a-cx-z]/, "ddddy"), "4:y");
    assert(m(/[a-cx-z]/, "dddd"), "null");
}

/* `^` without the m flag anchors the search to the start of the input;
   with it, every position after a line terminator is a candidate */
{
    assert(m(/^a/, "a"), "0:a");
    assert(m(/^a/, "ba"), "null");
    assert(m(/^a/, "b\na"), "null");
    assert(m(/^a/m, "b\na"), "2:a");
    assert(m(/^a/m, "b\ra"), "2:a");
    assert(m(/^a/m, "b a"), "2:a");
    assert(m(/^a/m, "ba"), "null");
    assert(m(/(^a)/, "ba"), "null");
    assert(m(/(^a)/m, "b\na"), "2:a");
    assert(m(/^/, "abc"), "0:");
    assert(all(/^./gm, "ab\ncd\nef"), "0:a 3:c 6:e");

    /* an anchored pattern with a lastIndex past 0 cannot match */
    {
        const re = /^a/g;
        re.lastIndex = 1;
        assert(re.exec("aa"), null);
        re.lastIndex = 0;
        assert(m(re, "aa"), "0:a");
    }

    /* ... but the multiline one can */
    {
        const re = /^a/gm;
        re.lastIndex = 1;
        const r = re.exec("a\na");
        assert(r !== null, true);
        assert(r.index, 2);
    }

    /* `$` is not an anchor for the *start* of the search */
    assert(m(/a$/, "bba"), "2:a");
    assert(all(/a$/gm, "a\nba\nc"), "0:a 3:a");
}

/* the sticky flag runs its own anchored search and must be unaffected */
{
    const re = /b/y;
    re.lastIndex = 0;
    assert(re.exec("ab"), null);
    re.lastIndex = 1;
    const r = re.exec("ab");
    assert(r !== null, true);
    assert(r.index, 1);

    const g = /b/gy;
    g.lastIndex = 1;
    assert(g.exec("ab")[0], "b");
}

/* astral characters and lone surrogates */
{
    assert(m(/\u{1F600}/u, "ab\u{1F600}"), "2:\u{1F600}");
    assert(m(/\u{1F600}/u, "ab"), "null");
    assert(m(/[\u{1F600}-\u{1F64F}]/u, "ab\u{1F601}"), "2:\u{1F601}");
    assert(m(/\u{1F600}x/u, "\u{1F600}\u{1F600}x"), "2:\u{1F600}x");

    /* without the u flag the same text is two code units */
    assert(m(/😀/, "ab\u{1F600}"), "2:\u{1F600}");

    /* a match must not start in the middle of a surrogate pair under u */
    assert(m(/\ude00/u, "\u{1F600}"), "null");
    assert(m(/./u, "\u{1F600}"), "0:\u{1F600}");
    /* a lone low surrogate is a code point of its own */
    assert(m(/\ude00/u, "a\ude00"), "1:\ude00");
    assert(m(/\ud800/u, "a\ud800"), "1:\ud800");
    /* without u, a lone half of a pair is reachable */
    assert(m(/\ude00/, "\u{1F600}"), "1:\ude00");
}

/* patterns whose first element cannot be summarised must still be correct */
{
    assert(m(/\bfoo/, "a foo"), "2:foo");
    assert(m(/\Bar/, "bar"), "1:ar");
    assert(m(/(?=ab)a/, "xab"), "1:a");
    assert(m(/(?!a)b/, "ab"), "1:b");
    assert(m(/(?<=a)b/, "xab"), "2:b");
    assert(m(/(?<!a)b/, "ab cb"), "4:b");
    assert(m(/a*b/, "xxb"), "2:b");
    assert(m(/(a|b)c/, "xxbc"), "2:bc");
    assert(m(/(?:x|y)z/, "aayz"), "2:yz");
    assert(m(/.b/, "aab"), "1:ab");
    assert(m(/[\s\S]b/, "aab"), "1:ab");
    assert(m(/(a)\1/, "xaa"), "1:aa");
    assert(m(/a?b/, "cb"), "1:b");
}

/* a failed attempt must not leave its captures behind for a later one */
{
    const r = /(a)q|b/.exec("ab");
    assert(r[0], "b");
    assert(r.index, 1);
    assert(r[1], undefined);

    const r2 = /(?:(x)y|z)/.exec("xz");
    assert(r2[0], "z");
    assert(r2[1], undefined);

    const r3 = /(a)?b/.exec("xb");
    assert(r3[0], "b");
    assert(r3[1], undefined);

    const r4 = /(?<g>a)q|b/.exec("ab");
    assert(r4.groups.g, undefined);
}

/* empty matches and the global iteration that walks over them */
{
    assert(m(/(?:)/, "abc"), "0:");
    assert(all(/(?:)/g, "abc"), "0: 1: 2: 3:");
    assert(all(/a*/g, "baab"), "0: 1:aa 3: 4:");
    assert(all(/b/g, "abcb"), "1:b 3:b");
    assert("abcb".split(/b/).join("|"), "a|c|");
    assert("abcb".replace(/b/g, "<$&>"), "a<b>c<b>");
    assert("aXbXc".split(/x/i).join("|"), "a|b|c");
    assert("Kelvin".replace(/k/giu, "K"), "Kelvin");
}

/* the same pattern over an 8-bit and a 16-bit buffer */
{
    const latin1 = "aaaaab";
    const wide = "aaaaab中";
    assert(m(/ab/, latin1), "4:ab");
    assert(m(/ab/, wide), "4:ab");
    assert(m(/b中/, wide), "5:b中");
    assert(m(/[a-b]+/, wide), "0:aaaaab");
    assert(m(/中/, latin1), "null");

    /* a Latin-1 buffer cannot hold anything above U+00FF */
    assert(m(/[Ā-Ȁ]/, latin1), "null");
    assert(m(/[Ā-Ȁ]/, "abŐ"), "2:Ő");
}

/* a NUL first character: the 8-bit search is a memchr(), and NUL is exactly
   the byte a C string search would stop at rather than find */
{
    assert(m(/\0/, "abc\0d"), "3:\0");
    assert(m(/\0/, "abc"), "null");
    assert(m(/\0x/, "\0a\0x"), "2:\0x");
    assert(m(/[\0]/, "ab\0"), "2:\0");
    assert(m(/[^\0]/, "\0\0a"), "2:a");
    assert(m(/\0/, "a".repeat(4000) + "\0"), "4000:\0");
    assert(m(/\0/u, "ab\0"), "2:\0");
    assert(all(/\0/g, "\0a\0"), "0:\0 2:\0");
    /* and over a 16-bit buffer, where memchr cannot be used at all */
    assert(m(/\0/, "中\0"), "1:\0");
    assert(m(/\0中/, "\0a中\0中"), "3:\0中");
}

/* dotAll changes what the first element accepts, not where it may start */
{
    assert(m(/.b/s, "a\nb"), "1:\nb");
    assert(m(/.b/, "a\nb"), "null");
    assert(m(/.b/s, "xxab"), "2:ab");
    assert(all(/./gs, "a\nb"), "0:a 1:\n 2:b");
    assert(all(/./g, "a\nb"), "0:a 2:b");
}

/* an inline modifier group scopes case folding to part of the pattern, so
   the first element's foldedness is not the pattern's */
{
    assert(m(/(?i:k)x/, "aKx"), "1:Kx");
    assert(m(/(?i:k)x/, "aKX"), "null");
    assert(m(/(?i:[a-c])z/, "xxBz"), "2:Bz");
    assert(m(/(?-i:k)x/i, "aKX"), "null");
    assert(m(/(?-i:k)x/i, "akX"), "1:kX");
}

/* the v flag builds its character classes differently but must summarise to
   the same set of possible first characters */
{
    assert(m(/[\q{abc|x}]/v, "zzabc"), "2:abc");
    assert(m(/[\q{abc|x}]/v, "zzx"), "2:x");
    assert(m(/[[a-z]--[b-d]]/v, "bcde"), "3:e");
    assert(m(/[[a-z]--[b-d]]/v, "bcd"), "null");
    assert(m(/[\p{ASCII}&&\p{Letter}]/v, "123x"), "3:x");
    assert(m(/[^a-c]/v, "aaaz"), "3:z");
    assert(m(/k/iv, "xK"), "1:K");
    assert(m(/[à-ÿ]/v, "ABĀ"), "null");
}

/* the search must not start in the middle of a surrogate pair even when
   lastIndex points there */
{
    const s = "\u{1F600}\u{1F600}x";
    const re = /x/gu;
    for (let i = 0; i <= s.length; i++) {
        re.lastIndex = i;
        const r = re.exec(s);
        assert(r === null ? "null" : String(r.index), i <= 4 ? "4" : "null",
               `lastIndex ${i}`);
    }

    /* a pattern whose first character is the low half of a pair */
    const lo = /\ude00/gu;
    lo.lastIndex = 1;
    assert(lo.exec(s), null);
    lo.lastIndex = 0;
    assert(lo.exec(s), null);

    /* without u the same index is a perfectly good starting point */
    const nolo = /\ude00/g;
    nolo.lastIndex = 1;
    assert(nolo.exec(s).index, 1);
}

/* long 16-bit buffers, where the candidate appears only at the very end */
{
    const wide = "中".repeat(5000);
    assert(m(/x/, wide), "null");
    assert(m(/x/, wide + "x"), "5000:x");
    assert(m(/中x/, wide + "x"), "4999:中x");
    assert(m(/[a-z]/, wide + "q"), "5000:q");
    assert(m(/[Ā-Ȁ]/, wide + "Ő"), "5000:Ő");
    assert(m(/中{4999}/, wide), "0:" + "中".repeat(4999));
    assert(m(/中{5001}/, wide), "null");

    /* the astral equivalent, where every candidate is two code units */
    const astral = "\u{1F600}".repeat(3000);
    assert(m(/x/u, astral), "null");
    assert(m(/x/u, astral + "x"), "6000:x");
    assert(m(/\u{1F600}x/u, astral + "x"), "5998:\u{1F600}x");
}

/* full width digits and letters are not \d or \w, however wide the buffer */
{
    assert(m(/\d/, "ａｂ１"), "null");
    assert(m(/\w/, "ａｂｃ"), "null");
    assert(m(/\d/, "ａｂ1"), "2:1");
    assert(m(/\d/u, "１2"), "1:2");
    assert(m(/\s/, "中 "), "1: ");
    assert(m(/\s/, "中中"), "null");
}

// for-of with yield in iterable expression should not cause stack underflow
function* x() {
    for (var e of yield []);
}
var g = x();
g.next();
g.return("test");

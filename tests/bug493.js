// Iterator.prototype.map must not leak the original item
function* gen() {
    for (let i = 0; i < 5; i++) {
        yield { index: i };
    }
}

const mapped = gen().map(x => x.index * 2);

for (const val of mapped) {}

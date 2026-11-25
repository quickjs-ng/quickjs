// Not a valid module: 'await using' in the head of a for-of loop inside a
// function that is not async. Imported by for-await-using-sync.js.
export function f(r) {
    for (await using x of [r]) {
    }
}

const it = new class extends Iterator {
    constructor() {
        super()
        this.i = 0
    }
    next() {
        if (++this.i === 1) {
            return {done: false, get value() { throw "fail" }}
        } else {
            return {done:true}
        }
    }
}
it.drop(1).next().value // should not throw

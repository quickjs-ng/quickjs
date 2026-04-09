function deferred() {
    let resolve
    const promise = new Promise(f => {
        resolve = f
    })
    return { promise, resolve }
}

let it, a, b, c
let getterHit = 0
let inGetter = false

Object.defineProperty(Object.prototype, "then", {
    configurable: true,
    get() {
        if (inGetter || !it) return undefined
        inGetter = true
        try {
            if (getterHit === 0) {
                it.return(0)
            } else if (getterHit === 1) {
                it.next(1)
                it.return(1)
            }
            getterHit++
        } catch (_) {
        }
        inGetter = false
        return undefined
    },
})

async function* g() {
    try {
        await a.promise
        yield 1
        await b.promise
        yield 2
        await c.promise
    } finally {
        await c.promise
    }
}

(async () => {
    a = deferred()
    b = deferred()
    c = deferred()
    it = g()

    it.next()
    a.resolve({})
    await Promise.resolve()
    await Promise.resolve()
    b.resolve({})
    c.resolve({})
    await Promise.resolve()
})()

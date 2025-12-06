;(function(IteratorHelper, InternalError, TypeError, call, Symbol·iterator) {
    function check(v, s) {
        if (typeof v === "object" && v !== null) return
        throw new TypeError(s)
    }
    function close(iter) {
        try {
            if (!iter) return
            let method = iter.return
            if (method) call(iter, method)
        } catch (e) {
            return e
        }
    }
    function closeall(iters, count) {
        let ex = undefined
        for (let i = count; i-- > 0;) {
            let iter = iters[i]
            iters[i] = undefined
            let e = close(iter)
            if (!ex) ex = e
        }
        return ex
    }
    return function zip(iterables, options = undefined) {
        check(iterables, "bad iterables")
        if (options === undefined) options = {__proto__: null}
        else check(options, "bad options")
        let mode = options.mode
        if (mode === undefined) mode = "shortest"
        if (!(mode === "strict" || mode === "longest" || mode === "shortest"))
            throw new TypeError("bad mode")
        let padding = undefined
        if (mode === "longest") {
            padding = options.padding
            if (padding !== undefined) check(padding, "bad padding")
        }
        let pads = []
        let iters = []
        let nexts = []
        let count = 0
        let paddingiter = undefined
        let iterablesiter = iterables[Symbol·iterator]()
        try {
            let next = iterablesiter.next
            for (;;) {
                let item
                try {
                    item = call(iterablesiter, next)
                } catch (e) {
                    // *don't* close |iterablesiter| when .next() throws
                    iterablesiter = undefined
                    throw e
                }
                // order of .done and .value property
                // access is observable and matters
                if (item.done) break
                let iter = item.value
                check(iter, "bad iterator")
                let method = iter[Symbol·iterator]
                if (method) iter = call(iter, method) // iterable -> iterator
                iters[count] = iter
                nexts[count] = iter.next
                count++
            }
            iterablesiter = undefined
            if (padding) {
                paddingiter = padding[Symbol·iterator]()
                let next = paddingiter.next
                let i = 0
                let done = false
                for (; i < count; i++) {
                    let value
                    try {
                        let item = call(paddingiter, next)
                        // order of .done and .value property
                        // access is observable and matters
                        done = item.done
                        value = item.value
                    } catch (e) {
                        // *don't* close |paddingiter| when .next()
                        // or .done or .value property access throws
                        paddingiter = undefined
                        throw e
                    }
                    if (done) break
                    pads[i] = value
                }
                // have to be careful to not double-close on exception
                // exceptions from .return() should still bubble up though
                let t = paddingiter
                paddingiter = undefined
                if (!done) {
                    let ex = close(t)
                    if (ex) throw ex
                }
                for (; i < count; i++) pads[i] = undefined
            }
        } catch (e) {
            closeall(iters, count)
            close(iterablesiter)
            close(paddingiter)
            throw e
        }
        // note: uses plain numbers for |state|, using
        // constants grows the bytecode by about 200 bytes
        let state = 0 // new, suspend, execute, complete
        let alive = count
        return {
            __proto__: IteratorHelper,
            // TODO(bnoordhuis) shows up as "at next (<null>:0:1)" in stack
            // traces when iterator throws, should be "at next (native)"
            next() {
                switch (state) {
                case 0: // new
                case 1: // suspend
                    state = 2 // execute
                    break
                case 2: // execute
                    throw new TypeError("running zipper")
                case 3: // complete
                    return {value:undefined, done:true}
                default:
                    throw new InternalError("bug")
                }
                let dones = 0
                let values = 0
                let results = []
                for (let i = 0; i < count; i++) {
                    let iter = iters[i]
                    if (!iter) {
                        if (mode !== "longest") throw new InternalError("bug")
                        results[i] = pads[i]
                        continue
                    }
                    let result
                    try {
                        result = call(iter, nexts[i])
                    } catch (e) {
                        alive = 0
                        iters[i] = undefined
                        closeall(iters, count)
                        throw e
                    }
                    // order of .done and .value property
                    // access is observable and matters
                    if (!result.done) {
                        if (mode === "strict" && dones > 0) {
                            closeall(iters, count)
                            throw new TypeError("mismatched inputs")
                        }
                        results[i] = result.value
                        values++
                        continue
                    }
                    alive--
                    dones++
                    iters[i] = undefined
                    switch (mode) {
                    case "shortest":
                        let ex = closeall(iters, count)
                        if (ex) throw ex
                        state = 3 // complete
                        return {value:undefined, done:true}
                    case "longest":
                        if (alive < 1) {
                            state = 3 // complete
                            return {value:undefined, done:true}
                        }
                        results[i] = pads[i]
                        break
                    case "strict":
                        if (values > 0) {
                            closeall(iters, count)
                            throw new TypeError("mismatched inputs")
                        }
                        break
                    }
                }
                if (values === 0) {
                    state = 3 // complete
                    return {value:undefined, done:true}
                }
                state = 1 // suspend
                return {value:results, done:false}
            },
            return() {
                switch (state) {
                case 0: // new
                    state = 3 // complete
                    break
                case 1: // suspend
                    state = 2 // execute
                    break
                case 2: // execute
                    throw new TypeError("running zipper")
                case 3: // complete
                    return {value:undefined, done:true}
                default:
                    throw new InternalError(`bug: state=${state}`)
                }
                // TODO skip when already closed because of earlier exception
                let ex = closeall(iters, count)
                if (ex) throw ex
                state = 3 // complete
                return {value:undefined, done:true}
            },
        }
    }
})

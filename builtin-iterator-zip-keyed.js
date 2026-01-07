;(function(IteratorHelper, InternalError, TypeError, call,
           hasOwnEnumProperty, getOwnPropertyKeys, Symbol·iterator) {
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
    return function zipKeyed(iterables, options = undefined) {
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
        let keys = []
        let count = 0
        let iters = []
        let nexts = []
        let pads = []
        for (let key of getOwnPropertyKeys(iterables)) keys[count++] = key
        try {
            for (let i = 0; i < count; i++) {
                let del = true
                let key = keys[i]
                // test262 goes out of its way to destroy performance here:
                // we must only add enumerable properties but because evil
                // getters can flip the visibility of subsequent properties,
                // we have to first collect *all* properties, enumerable and
                // non-enumerable, then check each one when we add it to the
                // final list
                // TODO(bnoordhuis) optimize by introducing some kind of
                // mutation observer; mutation is the uncommon case
                if (hasOwnEnumProperty(iterables, key)) {
                    let iter = iterables[key]
                    if (iter !== undefined) {
                        check(iter, "bad iterator")
                        let method = iter[Symbol·iterator]
                        if (method) iter = call(iter, method) // iterable -> iterator
                        iters[i] = iter
                        nexts[i] = iter.next
                        del = false
                    }
                }
                if (del) {
                    // splice out property; the assumption here is that the
                    // vast majority of objects don't have non-enumerable
                    // properties, or properties whose value is undefined
                    for (let j = i+1; j < count; j++) keys[j-1] = keys[j]
                    count--
                    i--
                }
            }
            if (mode === "longest") {
                if (padding) {
                    for (let i = 0; i < count; i++) pads[i] = padding[keys[i]]
                } else {
                    for (let i = 0; i < count; i++) pads[i] = undefined
                }
            }
        } catch (e) {
            closeall(iters, count)
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
                let results = {__proto__: null}
                for (let i = 0; i < count; i++) {
                    let key = keys[i]
                    let iter = iters[i]
                    if (!iter) {
                        if (mode !== "longest") throw new InternalError("bug")
                        results[key] = pads[i]
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
                        results[key] = result.value
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
                        results[key] = pads[i]
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

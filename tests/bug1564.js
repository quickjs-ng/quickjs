const s = new DisposableStack();
s.adopt({}, () => { gc(); });        // disposed last (i=0); its disposer runs the GC
s.use({ [Symbol.dispose]() {} });    // disposed first (i=1); freed before the adopt disposer runs
s.dispose();


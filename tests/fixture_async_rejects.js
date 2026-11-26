/* an async module that rejects after its top-level await */
await Promise.resolve();
throw new Error("leaf boom");

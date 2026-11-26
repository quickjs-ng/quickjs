/* the async module that actually fails */
await Promise.resolve();
throw new Error("leaf rejected");

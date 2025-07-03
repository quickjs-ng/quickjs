/* example of JS module importing a C module */

const { fib } = await import(`./fib.qjs`);

console.log("Hello World");
console.log("fib(10)=", fib(10));

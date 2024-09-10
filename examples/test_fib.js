/* example of JS module importing a C module */
import * as os from "os";

const isWin = os.platform === 'win32';
const { fib } = await import(`./fib.${isWin ? 'dll' : 'so'}`);

console.log("Hello World");
console.log("fib(10)=", fib(10));

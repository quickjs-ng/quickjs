import { assert } from "./assert.js";

const testString = "Hello, world!";
const regex = /world/;
regex.exec(testString);

assert(RegExp.leftContext === "Hello, ", true);
assert(RegExp.rightContext === "!", true);

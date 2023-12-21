import * as std from "std";
import * as os from "os";

function extractExpect(code) {
  const matches = code.matchAll(/\/\* EXPECT\((.*?)\):\n([\s\S]*?)\*\//gm);
  return [...matches].map((m) => ({
    name: m[1],
    excepted: m[2].trim(),
  }));
}

const cwd = import.meta.url
  .slice("file://".length)
  .slice(0, -"run-tests.js".length - 1);
const proj = `${cwd}/../..`;

// 1. build qjs by turn on the flag `DUMP_QJS_BYTECODE` which will print
//    the bytecode stream in stdout
// 2. add/modify the tests under the subdirectories
// 3. the test file is self-included, that means it consists of the test rules:
//
//    ```
//    /* EXCEPT(NAME_OF_THIS_CAST):
//    THE_EXCEPTED_OUTPUT_TO_COMPARE_WITH_THE_TEST
//    */
//    ```
function test(name) {
  const fds = os.pipe();
  const qjs = `${proj}/build/qjs`;
  const target = `${cwd}/cases/${name}.js`;

  os.exec([qjs, target], {
    stdout: fds[1],
    block: true,
  });
  os.close(fds[1]);

  const f = std.fdopen(fds[0], "r");
  const bc = f.readAsString();
  f.close();

  const tf = std.fdopen(os.open(target, os.O_RDONLY), "r");
  const code = tf.readAsString();
  tf.close();
  const cases = extractExpect(code);

  for (const { name: subname, excepted } of cases) {
    if (!bc.includes(excepted)) {
      throw new Error(
        `failed to test [${name}] at [${subname}],
--got--:
|${bc}|
--excepted--:
|${excepted}|`
      );
    }
  }
}

test("literal_arr");
test("literal_obj");
test("expr_call");
test("vardec_basic");
test("vardec_destruct");
test("class_ctor");
test("class_method");
test("class_prop_init");
test("class_call_private");
test("if_basic");
test("for_basic");
test("forin_basic");
test("forin_destruct");
test("forof_basic");
test("forof_destruct");
test("forof_await");
test("while_basic");
test("dowhile");
test("break");
test("continue");
test("return");
test("switch");

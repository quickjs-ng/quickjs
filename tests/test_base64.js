import { assert, assertArrayEquals, assertThrows } from "./assert.js";

let source_string = "QuickJS! //+!@#$%^&*()";

// created with node.  
let target_string = "UXVpY2tKUyEgLy8rIUAjJCVeJiooKQ==";

function test_atob() {
  let encoded_string = btoa(source_string);
  assert(encoded_string == target_string);
}

function test_btoa() {
  let decoded_string = atob(target_string);
  assert(decoded_string == source_string);
}

function test_btoatob() {
  assert(source_string == atob(btoa(source_string)));
}

function test_atobtoa() {
  assert(target_string == btoa(atob(target_string)));
}

function test_fromarray() {
  const binaryData = new Uint8Array([72, 101, 108, 108, 111]); // 'Hello'
  const text = String.fromCharCode.apply(null, binaryData);
  const targetText = "SGVsbG8=";
  assert(btoa(text) == targetText);
}

function test_nonbase64_characters() {
  const f = () => {
    atob("!@#$%^");
  };

  assert_throws(TypeError, f);
}

test_atob();
test_btoa();
test_btoatob();
test_atobtoa();
test_fromarray();

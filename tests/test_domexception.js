import { assert, assertThrows } from "./assert.js";

function test_length() {
	assert(DOMException.length, 0);
}

function test_descriptors() {
	/* Per Web IDL, these should be enumerable, configurable accessors */
	for (const name of ["name", "message", "code"]) {
		const desc = Object.getOwnPropertyDescriptor(DOMException.prototype, name);
		assert(typeof desc.get, "function");
		assert(desc.set, undefined);
		assert(desc.enumerable, true);
		assert(desc.configurable, true);
	}
}

function test_code() {
	let ex = new DOMException();
	assert(ex.code, 0);
	ex = new DOMException("", "HierarchyRequestError\0test");
	assert(ex.code, 0);
	ex = new DOMException("test", "HierarchyRequestError");
	assert(ex.code, 3);
	assert(ex.code, ex.HIERARCHY_REQUEST_ERR);
	assert(ex.code, DOMException.HIERARCHY_REQUEST_ERR);
	ex = new DOMException("", "DataCloneError");
	assert(ex.code, ex.DATA_CLONE_ERR);
	assert(ex.code, 25);
	ex = new DOMException("", "IndexSizeError");
	assert(ex.code, ex.INDEX_SIZE_ERR);
	assert(ex.code, 1);
}

function test_properties() {
	let ex = new DOMException("test");
	assert(ex.message, "test");
	assert(ex.name, "Error");
	ex = new DOMException("test", "InvalidCharacterError");
	assert(ex.name, "InvalidCharacterError");
	assertThrows(TypeError, () => ex.message = "");
	assertThrows(TypeError, () => ex.name = "test");
	assert(ex.__proto__, DOMException.prototype);
	/* Note: browsers set "stack" on the prototype, not the object.
	 * This follows node. */
	assert(Object.getOwnPropertyNames(ex), ["stack"]);
}

test_length();
test_descriptors();
test_code();
test_properties();

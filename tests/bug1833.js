// https://github.com/quickjs-ng/quickjs/issues/1833
// A module whose linking fails is reset to unlinked and linked again by the
// next import. The variable references taken by the failed link must be
// released, otherwise they leak and JS_FreeRuntime asserts in debug builds.
import { assert } from "./assert.js";

const results = await Promise.allSettled([
    import("./fixture_link_fail_a.js"),
    import("./fixture_link_fail_a.js"),
]);
for (const r of results) {
    assert(r.status, "rejected");
    assert(r.reason instanceof SyntaxError);
}

// Linking fails on the last import, after the other imports of this module
// and the imports and exports of fixture_link_fail_b.js have been resolved.
import { b, ns } from "./fixture_link_fail_b.js";
import { missing } from "./fixture_link_fail_b.js";
export const a = 1;

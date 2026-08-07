/* no top-level await of its own, but an async dependency: the module is
   marked [[AsyncEvaluation]] while it waits for the leaf and must be
   unmarked once it has run */
import { leaf } from "./fixture_async_leaf.js";
export const parent = leaf + 1;

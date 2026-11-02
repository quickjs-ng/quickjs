/* imports the already evaluated sync parent: nothing here is async */
import { parent } from "./fixture_async_sync_parent.js";
export const grandparent = parent + 1;

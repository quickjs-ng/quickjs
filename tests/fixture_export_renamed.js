import { foo as f, counter, bump } from "./fixture_export_base.js";
export { f, f as alsoF, counter, bump };
export const localOnly = 10;

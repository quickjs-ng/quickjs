// ES2020 string export names test fixture
export const regularExport = "regular";
const value1 = "value-1";
const value2 = "value-2";

// String export names (ES2020)
export { value1 as "string-export-1" };
export { value2 as "string-export-2" };

// Mixed: regular and string exports
const mixed = "mixed-value";
export { mixed as normalName, mixed as "string-name" };

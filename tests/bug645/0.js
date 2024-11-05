"use strict";

const u8 = new Uint8Array(1);
u8[100] = 123; // Should not throw.

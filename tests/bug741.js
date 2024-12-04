import {assert} from "./assert.js"

while (1) label: break

var i = 0
while (i < 3) label: {
    if (i > 0)
        break
    i++
}
assert(i, 1)

for (;;) label: break

for (i = 0; i < 3; i++) label: {
    if (i > 0)
        break
}
assert(i, 1)

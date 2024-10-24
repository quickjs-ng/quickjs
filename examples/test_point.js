/* example of JS module importing a C module */
import * as os from "qjs:os";

const isWin = os.platform === 'win32';
const { Point } = await import(`./point.${isWin ? 'dll' : 'so'}`);

function assert(b, str)
{
    if (b) {
        return;
    } else {
        throw Error("assertion failed: " + str);
    }
}

class ColorPoint extends Point {
    constructor(x, y, color) {
        super(x, y);
        this.color = color;
    }
    get_color() {
        return this.color;
    }
};

function main()
{
    var pt, pt2;

    pt = new Point(2, 3);
    assert(pt.x === 2);
    assert(pt.y === 3);
    pt.x = 4;
    assert(pt.x === 4);
    assert(pt.norm() == 5);

    pt2 = new ColorPoint(2, 3, 0xffffff);
    assert(pt2.x === 2);
    assert(pt2.color === 0xffffff);
    assert(pt2.get_color() === 0xffffff);
}

main();

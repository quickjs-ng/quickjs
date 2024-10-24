import * as os from "qjs:os";
import { assert } from "./assert.js";

var worker;

function test_worker()
{
    var counter;

    worker = new os.Worker("./test_worker_module.js");

    counter = 0;
    worker.onmessage = function (e) {
        var ev = e.data;
//        print("recv", JSON.stringify(ev));
        switch(ev.type) {
        case "num":
            assert(ev.num, counter);
            counter++;
            if (counter == 10) {
                /* test SharedArrayBuffer modification */
                let sab = new SharedArrayBuffer(10);
                let buf = new Uint8Array(sab);
                worker.postMessage({ type: "sab", buf: buf });
            }
            break;
        case "sab_done":
            {
                let buf = ev.buf;
                /* check that the SharedArrayBuffer was modified */
                assert(buf[2], 10);
                worker.postMessage({ type: "abort" });
            }
            break;
        case "done":
            /* terminate */
            worker.onmessage = null;
            break;
        }
    };
}

if (os.platform !== 'win32') {
    test_worker();
}

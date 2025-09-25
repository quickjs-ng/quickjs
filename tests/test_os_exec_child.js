import * as std from "qjs:std";
import * as os from "qjs:os";
import { assert } from  "./assert.js";

const isWin = os.platform === 'win32';

function test_os_processes() {
    var ret;

    const osShellCmd = ( isWin ? "cmd" : "/bin/sh" );
    const osShellFlag = ( isWin ? "/c" : "-c" );

    ret = os.exec( [osShellCmd, osShellFlag, "exit 2"], { usePath: false } );
    assert(ret, 2 );

    ret = os.exec( [osShellCmd, osShellFlag, ": good commands return 0"] );
    assert(ret, 0);

    ret = os.exec( [osShellCmd, osShellFlag, "errors return respective error codes"],  );
    assert( ret, ( isWin ? 1 : 127 ) );

    ret = os.exec( [osShellCmd, osShellFlag, "echo stdio not inherited by default"] );
    assert(ret, 0);

    ret = os.exec( [osShellCmd, osShellFlag, "bad command or filename @@ stdio inherited"],  
        { inherit: true }
    );
    assert( ret, ( isWin ? 1 : 127 ) );

    ret = os.exec( ["echo invalid process names fail and do not report to stdio"],  
        { inherit: true }
    );
    assert( ret, ( isWin ? 2 : 127 ) );

    ret = os.exec( ["exit 1"], { usePath: false } );
    assert(ret, ( isWin ? 2 : 127 ) );

    ret = os.exec( [osShellCmd, osShellFlag, "echo shell text passes through inherited stdio"],
        { inherit: true }
     );
    assert(ret, 0);

};

test_os_processes();

import * as std from "qjs:std";
import * as os from "qjs:os";
import { assert } from  "./assert.js";

const isWin = os.platform === 'win32';
const isCygwin = os.platform === 'cygwin';

function test_os_exec() {
    var ret, fds, pid, f, status, amend, exe;

    // cmd.exe does leave a \r that getline doesn't trim
    const osTrimLine = ( isWin ? "\r" : "" );
    const osShellCmd = ( isWin ? "cmd" : "/bin/sh" );
    const osShellFlag = ( isWin ? "/c" : "-c" );
    const osPathSeparator = ( isWin ? "\\" : "/" );

    if (!isWin) {

        ret = os.exec( ["true"] );
        assert(ret, 0);
    }

    ret = os.exec( [osShellCmd, osShellFlag, "exit 2"], { usePath: false } );
    assert(ret, 2);

    ret = os.exec( ["bad command or filename @@"],  );
    assert( ret, ( isWin ? 1 : 127 ) );

    ret = os.exec( [osShellCmd, osShellFlag, ": good commands return 0"] );
    assert(ret, 0);

    pid = os.exec( [osShellCmd, osShellFlag, ":"], { block: false} );
    /*  watchpid is similar waitpid on WIN32 + GNU but lacks a status indicator
        watchpid(p,0) == waitpid(p, WNOHANG), watchpid(p,1) == waitpid(p,0) */
    ret = os.watchpid(pid, 1);
    assert(ret, pid);

    const osShellEchoParam =  ( isWin ? 'echo %FOO%' : 'echo $FOO' );
    fds = os.pipe();
    pid = os.exec( [osShellCmd, osShellFlag, osShellEchoParam ], {
        stdout: fds[1],
        block: false,
        env: { FOO: "hello" },
    } );
    assert(pid >= 0);
    os.close(fds[1]); 
    f = std.fdopen(fds[0], "r");
    assert(f.getline(), "hello" + osTrimLine);
    assert(f.getline(), null);
    f.close();
    if (!isWin) {   
        [ret, status] = os.waitpid(pid, 0);
        assert(ret, pid);
        assert(status & 0x7f, 0); /* exited */
        assert(status >> 8, 0); /* exit code */ 
    }

    pid = os.exec(["cat"], { block: false } );
    assert(pid >= 0);
    ret = os.watchpid(pid, 0);
    assert(ret, 0);
    /* os.kill in WIN32 Just does TerminateProcess & signal control is ignored. */
    os.kill(pid, os.SIGTERM);
    os.sleep(0); // watcpid is too fast! you have to sleep or check it again.
    ret = os.watchpid(pid, 1);
    assert(ret, pid); 

    if (!isWin) {
        pid = os.exec(["cat"], { block: false } );
        assert(pid >= 0);
        [ret, status] = os.waitpid(pid, os.WNOHANG);
        assert(ret, 0);
        os.kill(pid, os.SIGTERM);
        [ret, status] = os.waitpid(pid, 0);
        assert(ret, pid);
        // Flaky on cygwin for unclear reasons, see
        // https://github.com/quickjs-ng/quickjs/issues/184
        if (!isCygwin) {
            assert(status & 0x7f, os.SIGTERM);
        }
    }

    amend = os.exePath().split(osPathSeparator);
    amend.pop();
    amend.push("qjs");
    exe = ( isWin ? amend.join("\\") : amend.join("/") );

    pid = os.exec( [ exe,"-q"], { block: true } );
    assert(pid, 0);
    fds = os.pipe();
    pid = os.exec( [exe,"thereisno.js"], { 
        block: true,
        stderr: fds[1],
    } );
    assert(pid, 1); 
    os.close(fds[1]); 
    f = std.fdopen(fds[0], "r");
    assert(f.getline(), "thereisno.js: No such file or directory" + osTrimLine);
    assert(f.getline(), null);
    f.close();

};

test_os_exec();

import * as std from "qjs:std";
import * as os from "qjs:os";
import { assert } from  "./assert.js";

const isWin = os.platform === 'win32';
const isCygwin = os.platform === 'cygwin';

function test_os_exec() {
    var ret, fdout, fderr, pid, f, status, amend, exe, dir;

    /* cmd.exe does leave a \r that getline doesn't trim */
    const osTrimLine = ( isWin ? "\r" : "" );
    const osShellCmd = ( isWin ? "cmd" : "/bin/sh" );
    const osShellFlag = ( isWin ? "/c" : "-c" );
    const osPathSeparator = ( isWin ? "\\" : "/" );
    const osStallProgram = ( isWin ? "cmd /k" : "cat" );

    pid = os.exec( [osShellCmd, osShellFlag, ":"], { block: false} );
    /*  watchpid is similar waitpid on WIN32 + GNU but lacks a status indicator
        watchpid(p,0) == waitpid(p, WNOHANG), watchpid(p,1) == waitpid(p,0) */
    ret = os.watchpid(pid, 1);
    assert(ret, pid);
    
    fdout = os.pipe();
    const osShellEchoParam =  ( isWin ? 'echo %FOO%' : 'echo $FOO' );
    pid = os.exec( [osShellCmd, osShellFlag, osShellEchoParam ], {
        stdout: fdout[1],
        block: false,
        env: { FOO: "hello" },
    } );
    assert(pid >= 0);
    os.close(fdout[1]); 
    f = std.fdopen(fdout[0], "r");
    assert(f.getline(), "hello" + osTrimLine);
    assert(f.getline(), null);
    f.close();
    if (!isWin) {   
        [ret, status] = os.waitpid(pid, 0);
        assert(ret, pid);
        assert(status & 0x7f, 0); /* exited */
        assert(status >> 8, 0); /* exit code */ 
    }

    pid = os.exec([osStallProgram], { block: false } );
    assert(pid >= 0);
    ret = os.watchpid(pid, 0);
    assert(ret, 0);
    /* os.kill in WIN32 Just does TerminateProcess & signal control is ignored. */
    os.kill(pid, os.SIGTERM);
    os.sleep(0); // watcpid is too fast! you have to sleep or check it again.
    ret = os.watchpid(pid, 1);
    assert(ret, pid); 

    if (!isWin) {
        pid = os.exec([osStallProgram], { block: false } );
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
    exe = amend.join(osPathSeparator);
    [dir, ret] = os.getcwd(); 
    amend = dir.split(osPathSeparator);
    if (amend[amend.length - 1] == "build")
        amend.pop();
    if (amend[amend.length - 1] != "tests") {
        amend.push("tests");
        dir = amend.join(osPathSeparator);
    };

    pid = os.exec( [ exe,"-q"], { block: true } );
    assert(pid, 0);

    fdout = os.pipe();
    fderr = os.pipe();
    pid = os.exec( [exe, "test_os_exec_child.js"], { 
        block: true,
        stdout: fdout[1],
        stderr: fderr[1],
        cwd: dir,
    } );
    assert(pid, 0); 
    os.close(fdout[1]); 
    os.close(fderr[1]);
    f = std.fdopen(fdout[0], "r");
    assert(f.getline(), "shell text passes through inherited stdio" + osTrimLine);
    assert(f.getline(), null);
    f.close();
    f = std.fdopen(fderr[0], "r");
    ret = f.getline().indexOf('bad');
    assert(ret > 0);
    f.close();

};

test_os_exec();

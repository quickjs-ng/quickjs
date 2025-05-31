import * as std from "qjs:std";
import * as os from "qjs:os";
import { assert } from  "./assert.js";

const isWin = os.platform === 'win32';
const isCygwin = os.platform === 'cygwin';

function test_win_os_exec() {
    var ret, fds, pid, f, status;

    ret = os.exec(["smeegul desgpglam golum"]);
    assert(ret, 1);

    ret = os.exec(["echo test 0"]);
    assert(ret, 0);

    ret = os.exec(["cmd.exe", "/c", "exit 2"], { usePath: false });
    assert(ret, 2);

    fds = os.pipe();
    pid = os.exec(["echo %FOO%"], {
        stdout: fds[1],
        block: false,
        env: { FOO: "hello" },
    } );
    assert(pid >= 0);
    os.close(fds[1]); 
    f = std.fdopen(fds[0], "r");
    var gl = f.getline();
    /* artifact of windows to c compatibility between getline and cmd.exe echo*/
    assert(gl, "hello\r");
    assert(f.getline(), null);
    f.close();
    /* I created watchpid to at least notify if the PID is complete cross-compatibly.
       There's no windows equivalent of waitpid, and windows would prefer Handles.
       watchpid returns negative error || 0 if still waiting || pid if complete. 
       watchpid returns no status. specify 1 in the 2nd param for blocking */
    ret = os.watchpid(pid, 1);
    assert(ret, pid);
    pid = os.exec(["cat"], { block: false } );
    assert(pid >= 0);
    /* Just does TerminateProcess. There's no signal. 
       Windows does signal pid groups and that takes research. */
    os.kill(pid, os.SIGTERM);
    os.sleep(1);
    ret = os.watchpid(pid, 0);
    assert(ret, pid); 
};

function test_os_exec()
{
    var ret, fds, pid, f, status;

    ret = os.exec(["true"]);
    assert(ret, 0);

    ret = os.exec(["/bin/sh", "-c", "exit 1"], { usePath: false });
    assert(ret, 1);

    fds = os.pipe();
    pid = os.exec(["sh", "-c", "echo $FOO"], {
        stdout: fds[1],
        block: false,
        env: { FOO: "hello" },
    } );
    assert(pid >= 0);
    os.close(fds[1]); /* close the write end (as it is only in the child)  */
    f = std.fdopen(fds[0], "r");
    assert(f.getline(), "hello");
    assert(f.getline(), null);
    f.close();
    [ret, status] = os.waitpid(pid, 0);
    assert(ret, pid);
    assert(status & 0x7f, 0); /* exited */
    assert(status >> 8, 0); /* exit code */

    pid = os.exec(["cat"], { block: false } );
    assert(pid >= 0);
    os.kill(pid, os.SIGTERM);
    [ret, status] = os.waitpid(pid, 0);
    assert(ret, pid);
    // Flaky on cygwin for unclear reasons, see
    // https://github.com/quickjs-ng/quickjs/issues/184
    if (!isCygwin) {
        assert(status & 0x7f, os.SIGTERM);
    }
}


if (!isWin) test_os_exec();
else test_win_os_exec();

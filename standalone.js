import * as std from "qjs:std";
import * as os from "qjs:os";
import * as bjson from "qjs:bjson";

// See quickjs.h
const JS_READ_OBJ_BYTECODE = 1 << 0;
const JS_READ_OBJ_REFERENCE = 1 << 3;
const JS_WRITE_OBJ_BYTECODE = 1 << 0;
const JS_WRITE_OBJ_REFERENCE = 1 << 3;
const JS_WRITE_OBJ_STRIP_SOURCE = 1 << 4;

/**
 * Trailer for standalone binaries. When some code gets bundled with the qjs
 * executable we add a 12 byte trailer. The first 8 bytes are the magic
 * string that helps us understand this is a standalone binary, and the
 * remaining 4 are the offset (from the beginning of the binary) where the
 * bundled data is located.
 *
 * The offset is stored as a 32bit little-endian number.
 */
const Trailer = {
    Magic: 'quickjs2',
    MagicSize: 8,
    DataSize: 4,
    Size: 12
};

function encodeAscii(txt) {
  return new Uint8Array(txt.split('').map(c => c.charCodeAt(0)));
}

function decodeAscii(buf) {
  return Array.from(buf).map(c => String.fromCharCode(c)).join('')
}

export function compileStandalone(inFile, outFile, targetExe) {
  // Step 1: compile the source file to bytecode
  const js = std.loadFile(inFile);

  if (!js) {
    throw new Error(`failed to open ${inFile}`);
  }

  const code = std.evalScript(js, {
    compile_only: true,
    compile_module: true
  });
  const bytecode = new Uint8Array(bjson.write(code, JS_WRITE_OBJ_BYTECODE | JS_WRITE_OBJ_REFERENCE | JS_WRITE_OBJ_STRIP_SOURCE));

  // Step 2: copy the bytecode to the end of the executable and add a marker.
  const exeFileName = targetExe ?? globalThis.argv0;
  const exe = std.loadFile(exeFileName, { binary: true });

  if (!exe) {
    throw new Error(`failed to open executable: ${exeFileName}`);
  }

  const exeSize = exe.length;
  const newBuffer = exe.buffer.transfer(exeSize + bytecode.length + Trailer.Size);
  const newExe = new Uint8Array(newBuffer);

  newExe.set(bytecode, exeSize);
  newExe.set(encodeAscii(Trailer.Magic), exeSize + bytecode.length);

  const dw = new DataView(newBuffer, exeSize + bytecode.length + Trailer.MagicSize, Trailer.DataSize);

  dw.setUint32(0, exeSize, true /* little-endian */);

  // We use os.open() so we can set the permissions mask.
  const newFd = os.open(outFile, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o755);

  if (newFd < 0) {
    throw new Error(`failed to create ${outFile}`);
  }
  if (os.write(newFd, newBuffer, 0, newBuffer.byteLength) < 0) {
    os.close(newFd);
    throw new Error(`failed to write to output file`);
  }
  os.close(newFd);
}

export function runStandalone() {
  const file = globalThis.argv0;
  const exe = std.open(file, 'rb');

  if (!exe) {
    throw new Error(`failed to open executable: ${file}`);
  }

  let r = exe.seek(-Trailer.Size, std.SEEK_END);
  if (r < 0) {
    throw new Error(`seek error: ${-r}`);
  }

  const trailer = new Uint8Array(Trailer.Size);

  exe.read(trailer.buffer, 0, Trailer.Size);

  const magic = new Uint8Array(trailer.buffer, 0, Trailer.MagicSize);

  // Shouldn't happen since qjs.c checks for it.
  if (decodeAscii(magic) !== Trailer.Magic) {
    exe.close();
    throw new Error('corrupted binary, magic mismatch');
  }

  const dw = new DataView(trailer.buffer, Trailer.MagicSize, Trailer.DataSize);
  const offset = dw.getUint32(0, true /* little-endian */);
  const bytecode = new Uint8Array(offset - Trailer.Size);

  r = exe.seek(offset, std.SEEK_SET);
  if (r < 0) {
    exe.close();
    throw new Error(`seek error: ${-r}`);
  }

  exe.read(bytecode.buffer, 0, bytecode.length);
  if (exe.error()) {
    exe.close();
    throw new Error('read error');
  }
  exe.close();

  const code = bjson.read(bytecode.buffer, 0, bytecode.length, JS_READ_OBJ_BYTECODE | JS_READ_OBJ_REFERENCE);

  return std.evalScript(code, {
    eval_module: true
  });
}

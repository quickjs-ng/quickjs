/*
	Copyright (c) 2021 Scott Lembcke

	Permission is hereby granted, free of charge, to any person obtaining a copy
	of this software and associated documentation files (the "Software"), to deal
	in the Software without restriction, including without limitation the rights
	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
	copies of the Software, and to permit persons to whom the Software is
	furnished to do so, subject to the following conditions:

	The above copyright notice and this permission notice shall be included in all
	copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
	SOFTWARE.
*/

#ifndef TINA_H
#define TINA_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Coroutine type.
typedef struct tina tina;

// Coroutine body function prototype.
// 'value' will be the value passed to the initial call to tina_resume() that starts the coroutine.
// The return value will be returned from the final tina_resume() call.
typedef void* tina_func(tina* coro, void* value);

struct tina {
	// Body function used by the coroutine. (readonly)
	tina_func* body;
	// User defined context pointer passed to tina_init(). (optional)
	void* user_data;
	// User defined name. (optional)
	const char* name;
	// Pointer to the coroutine's memory buffer. (readonly)
	void* buffer;
	// Size of the buffer. (readonly)
	size_t size;
	// Has the coroutine's body function exited? (readonly)
	bool completed;
	
	// Private:
	tina* _caller;
	void* _stack_pointer;
	// Stack canary values at the start and end of the buffer.
	const uint32_t* _canary_end;
	uint32_t _canary;
};

// Initialize a coroutine into a memory buffer.
// If 'buffer' is NULL, it will malloc() one for you, but you must call free(tina.buffer) yourself when done with it.
// 'body' is the function that will run inside of the coroutine, and 'user_data' will be stored in tina.user_data.
// The initialized coroutine will not be started until the first time you call 'tina_resume()' or 'tina_swap()'.
tina* tina_init(void* buffer, size_t size, tina_func* body, void* user_data);

// Assymmetric coroutines are simpler to use because they act similar to regular functions. The difference is
// when returning (yielding) from an assymetric coroutine, it returns to where it was called (resumed) from,
// but the next time it's resumed again, it starts where it left off instead of starting at the beginning.
// Since tina coroutines have their own stacks, you call functions from a coroutine, and even yield from a function.

// Resume running an assymmetric coroutine, passing a value to the coroutine.
void* tina_resume(tina* coro, void* value);
// Yield an assymmetric coroutine back to it's caller, and pass back a value.
void* tina_yield(tina* coro, void* value);

// Symmetric coroutines (sometimes called fibers) are slightly more flexible, but also slightly more complicated.
// You can transfer control between any two symmetric coroutines at any time, and there is no caller/callee relationship.
// To switch between symmetric coroutines, you need to pass the current ('from') and next ('to') coroutines to 'tina_swap()'.
// To make a symmetric coroutine for an existing thread you need to copy TINA_EMPTY to hold it's execution context.
// It is an error for the body functions of symmetric coroutines to return. Lastly, it is NOT recommended (although possible)
// to mix symmetric and assymetric coroutines together. Consider it to be undefined behavior.
// See extras/examples/symmetric.c for more info.

// A prototype for an empty symmetric coroutine. Copy this to initialize an empty coroutine that doesn't reference a new stack.
// You only need this if you are using symmetric coroutines via 'tina_swap()'.
extern const tina TINA_EMPTY;

// Swap between two symmetric coroutines, passing a value between them.
void* tina_swap(tina* from, tina* to, void* value);

#ifdef TINA_IMPLEMENTATION

#define TINA_ABI_aarch32 (__ARM_EABI__ && __GNUC__)
#define TINA_ABI_aarch64 (__aarch64__ && __GNUC__)
#define TINA_ABI_i386 ((__i386__ && __GNUC__) || (_M_IX86 && _MSC_VER))
#define TINA_ABI_SysV_AMD64 (__amd64__ && __GNUC__ && (__unix__ || __APPLE__ || __HAIKU__))
#define TINA_ABI_WIN64 ((__WIN64__ && __GNUC__) || (_M_AMD64 && _MSC_VER))
#define TINA_ABI_riscv64gc (__riscv && __riscv_xlen == 64 && __riscv_flen == 64)
// 沒有浮點擴充 (純RV32I)
#define TINA_ABI_riscv32i (__riscv && __riscv_xlen == 32 && !__riscv_flen)
// 單精度浮點 (RV32F)
#define TINA_ABI_riscv32f (__riscv && __riscv_xlen == 32 && __riscv_flen == 32)
// 雙精度浮點 (RV32D)
#define TINA_ABI_riscv32d (__riscv && __riscv_xlen == 32 && __riscv_flen == 64)

#ifndef TINA_NO_CRT
	#include <stdlib.h>
	#ifndef _TINA_ASSERT
		#include <stdio.h>
		#define _TINA_ASSERT(_COND_, _MESSAGE_) { if(!(_COND_)){fprintf(stderr, _MESSAGE_"\n"); abort();} }
	#endif
#else
	#define _TINA_ASSERT(_COND_, _MESSAGE_)
#endif

#ifndef TINA_WARN_STACK_SIZE
	#define TINA_WARN_STACK_SIZE 64*1024
#endif

#if _MSC_VER
	// Negation of unsigned integers is well defined. Warning is not helpful.
	#pragma warning(disable: 4146)
#endif

// Alignment to use for all types. (MSVC doesn't provide stdalign.h -_-)
#define _TINA_MAX_ALIGN ((size_t)16)

const tina TINA_EMPTY = {
	.body = NULL, .user_data = NULL, .name = "TINA_EMPTY",
	.buffer = NULL, .size = 0, .completed = false,
	._caller = NULL, ._stack_pointer = NULL,
	._canary_end = &TINA_EMPTY._canary, ._canary = 0x54494E41ul,
};

// Symbols for the assembly functions.
// These are either defined as inline assembly (GCC/Clang) of binary blobs (MSVC).
#if __WIN64__ || _WIN64
	extern const uint64_t _tina_swap[];
	extern const uint64_t _tina_init_stack[];
#else
	// Avoid the MSVC hack unless necessary!
	extern void* _tina_swap(void** sp_from, void** sp_to, void* value);
	extern tina* _tina_init_stack(tina* coro, void** sp_from, void* sp_to);
#endif

tina* tina_init(void* buffer, size_t size, tina_func* body, void* user_data){
	_TINA_ASSERT(size >= TINA_WARN_STACK_SIZE, "Tina Warning: Small stacks tend to not work on modern OSes. (Feel free to disable this if you have your reasons)");
#ifndef TINA_NO_CRT
	if(buffer == NULL) buffer = malloc(size);
#endif
	
	// Make sure 'buffer' is properly aligned.
	uintptr_t aligned = -(-(uintptr_t)buffer & -_TINA_MAX_ALIGN);
	size -= aligned - (uintptr_t)buffer;
	// Find the stack end, saving room for the canary value.
	void* stack_end = (uint8_t*)buffer + size - sizeof(TINA_EMPTY._canary);
	*(uint32_t*)stack_end = TINA_EMPTY._canary;
	
	tina* coro = (tina*)aligned;
	tina coro_value = {
		.body = body, .user_data = user_data, .name = "<no name>",
		.buffer = buffer, .size = size, .completed = false,
		._caller = NULL, ._stack_pointer = NULL,
		._canary_end = (uint32_t*)stack_end,
		._canary = TINA_EMPTY._canary,
	};
	(*coro) = coro_value;
	
	// Empty coroutine for the init function to use for a return location.
	tina dummy = TINA_EMPTY;
	coro->_caller = &dummy;

	typedef tina* init_func(tina* coro, void** sp_loc, void* sp);
	return ((init_func*)(void*)_tina_init_stack)(coro, &dummy._stack_pointer, stack_end);
}

// Must declare as non-static to make it visible to the asm below.
void _tina_start(tina* coro);

void _tina_start(tina* coro){
	// Yield back to the _tina_init_stack() call, and return the coroutine.
	void* value = tina_yield(coro, coro);
	// Call the body function with the first value.
	value = coro->body(coro, value);
	// body() has exited, and the coroutine is completed.
	coro->completed = true;
	// Yield the final return value back to the calling thread.
	_TINA_ASSERT(coro->_caller, "Tina Error: You must not return from a symmetric coroutine body function.");
	tina_yield(coro, value);
	
	_TINA_ASSERT(false, "Tina Error: You cannot resume a coroutine after it has finished.");
#ifndef TINA_NO_CRT
	abort(); // Crash predictably if assertions are disabled.
#endif
}

void* tina_swap(tina* from, tina* to, void* value){
	_TINA_ASSERT(from->_canary == TINA_EMPTY._canary, "Tina Error: Bad canary value. Coroutine has likely had a stack overflow.");
	_TINA_ASSERT(*from->_canary_end == TINA_EMPTY._canary, "Tina Error: Bad canary value. Coroutine has likely had a stack underflow.");
	typedef void* swap(void** sp_from, void** sp_to, void* value);
	return ((swap*)(void*)_tina_swap)(&from->_stack_pointer, &to->_stack_pointer, value);
}

void* tina_resume(tina* coro, void* value){
	_TINA_ASSERT(!coro->_caller, "Tina Error: tina_resume() called on a coroutine that hasn't yielded yet.");
	tina this_fiber = TINA_EMPTY;
	coro->_caller = &this_fiber;
	return tina_swap(&this_fiber, coro, value);
}

void* tina_yield(tina* coro, void* value){
	_TINA_ASSERT(coro->_caller, "Tina Error: tina_yield() called on a coroutine that wasn't resumed.");
	tina* caller = coro->_caller;
	coro->_caller = NULL;
	return tina_swap(coro, caller, value);
}

#if __APPLE__ || __WIN32__
	#define _TINA_SYMBOL(sym) "_"#sym
#else
	#define _TINA_SYMBOL(sym) #sym
#endif

#if TINA_ABI_aarch32
	// TODO: Is this an appropriate macro check for a 32 bit ARM ABI?
	// TODO: Only tested on RPi3.
	
	// Since the 32 bit ARM version is by far the shortest, I'll document this one.
	// The other variations are basically the same structurally.
	
	// _tina_init_stack() sets up the stack and initial execution of the coroutine.
	asm("_tina_init_stack:");
	// First things first, save the registers protected by the ABI
	asm("  push {r4-r11, lr}");
	asm("  vpush {q4-q7}");
	// Now store the stack pointer in the couroutine.
	// _tina_start() will call tina_yield() to restore the stack and registers later.
	asm("  str sp, [r1]");
	// Align the stack top to 16 bytes as requested by the ABI and set it to the stack pointer.
	asm("  and r2, r2, #-16");
	asm("  mov sp, r2");
	// Finally, tail call into _tina_start().
	// By setting the caller to null, debuggers will show _tina_start() as a base stack frame.
	asm("  mov lr, #0");
	asm("  b _tina_start");
	
	// https://static.docs.arm.com/ihi0042/g/aapcs32.pdf
	// _tina_swap() is responsible for swapping out the registers and stack pointer.
	asm("_tina_swap:");
	// Like above, save the ABI protected registers and save the stack pointer.
	asm("  push {r4-r11, lr}");
	asm("  vpush {q4-q7}");
	// Save stack pointer for the old coroutine, and load the new one.
	asm("  str sp, [r0]");
	asm("  ldr sp, [r1]");
	// Restore the new coroutine's protected registers.
	asm("  vpop {q4-q7}");
	asm("  pop {r4-r11, lr}");
	// Move the 'value' parameter to the return value register.
	asm("  mov r0, r2");
	// And perform a normal return instruction.
	// This will return from tina_yield() in the new coroutine.
	asm("  bx lr");
#elif TINA_ABI_riscv64gc
	// 64bit riscv w/ 64 bit floats
	// push s0-s11, fs0-fs11
	asm("_tina_init_stack:");
	asm("  addi sp, sp, -0xD0");
	asm("  sd  sp, (a1)");
	asm("  sd  ra,   0xC8(sp)");
	asm("  sd  s0,   0xC0(sp)");
	asm("  sd  s1,   0xB8(sp)");
	asm("  sd  s2,   0xB0(sp)");
	asm("  sd  s3,   0xA8(sp)");
	asm("  sd  s4,   0xA0(sp)");
	asm("  sd  s5,   0x98(sp)");
	asm("  sd  s6,   0x90(sp)");
	asm("  sd  s7,   0x88(sp)");
	asm("  sd  s8,   0x80(sp)");
	asm("  sd  s9,   0x78(sp)");
	asm("  sd  s10,  0x70(sp)");
	asm("  sd  s11,  0x68(sp)");
	asm("  fsd fs0,  0x60(sp)");
	asm("  fsd fs1,  0x58(sp)");
	asm("  fsd fs2,  0x50(sp)");
	asm("  fsd fs3,  0x48(sp)");
	asm("  fsd fs4,  0x40(sp)");
	asm("  fsd fs5,  0x38(sp)");
	asm("  fsd fs6,  0x30(sp)");
	asm("  fsd fs7,  0x28(sp)");
	asm("  fsd fs8,  0x20(sp)");
	asm("  fsd fs9,  0x18(sp)");
	asm("  fsd fs10, 0x10(sp)");
	asm("  fsd fs11, 0x08(sp)");
	asm("  andi a2, a2, ~0xF");
	asm("  mv sp, a2");
	asm("  mv ra, x0");
	asm("  tail _tina_start");
	
	asm("_tina_swap:");
	asm("  addi sp, sp, -0xD0");
	asm("  sd sp, (a0)");
	asm("  sd  ra,   0xC8(sp)");
	asm("  sd  s0,   0xC0(sp)");
	asm("  sd  s1,   0xB8(sp)");
	asm("  sd  s2,   0xB0(sp)");
	asm("  sd  s3,   0xA8(sp)");
	asm("  sd  s4,   0xA0(sp)");
	asm("  sd  s5,   0x98(sp)");
	asm("  sd  s6,   0x90(sp)");
	asm("  sd  s7,   0x88(sp)");
	asm("  sd  s8,   0x80(sp)");
	asm("  sd  s9,   0x78(sp)");
	asm("  sd  s10,  0x70(sp)");
	asm("  sd  s11,  0x68(sp)");
	asm("  fsd fs0,  0x60(sp)");
	asm("  fsd fs1,  0x58(sp)");
	asm("  fsd fs2,  0x50(sp)");
	asm("  fsd fs3,  0x48(sp)");
	asm("  fsd fs4,  0x40(sp)");
	asm("  fsd fs5,  0x38(sp)");
	asm("  fsd fs6,  0x30(sp)");
	asm("  fsd fs7,  0x28(sp)");
	asm("  fsd fs8,  0x20(sp)");
	asm("  fsd fs9,  0x18(sp)");
	asm("  fsd fs10, 0x10(sp)");
	asm("  fsd fs11, 0x08(sp)");

	asm("  ld sp, (a1)");
	asm("  ld  ra,   0xC8(sp)");
	asm("  ld  s0,   0xC0(sp)");
	asm("  ld  s1,   0xB8(sp)");
	asm("  ld  s2,   0xB0(sp)");
	asm("  ld  s3,   0xA8(sp)");
	asm("  ld  s4,   0xA0(sp)");
	asm("  ld  s5,   0x98(sp)");
	asm("  ld  s6,   0x90(sp)");
	asm("  ld  s7,   0x88(sp)");
	asm("  ld  s8,   0x80(sp)");
	asm("  ld  s9,   0x78(sp)");
	asm("  ld  s10,  0x70(sp)");
	asm("  ld  s11,  0x68(sp)");
	asm("  fld fs0,  0x60(sp)");
	asm("  fld fs1,  0x58(sp)");
	asm("  fld fs2,  0x50(sp)");
	asm("  fld fs3,  0x48(sp)");
	asm("  fld fs4,  0x40(sp)");
	asm("  fld fs5,  0x38(sp)");
	asm("  fld fs6,  0x30(sp)");
	asm("  fld fs7,  0x28(sp)");
	asm("  fld fs8,  0x20(sp)");
	asm("  fld fs9,  0x18(sp)");
	asm("  fld fs10, 0x10(sp)");
	asm("  fld fs11, 0x08(sp)");
	asm("  addi sp, sp, 0xD0");
	asm("  mv a0, a2");
	asm("  ret");
#elif TINA_ABI_aarch64
	asm(_TINA_SYMBOL(_tina_init_stack:));
	asm("  sub sp, sp, 0xA0");
	asm("  stp x19, x20, [sp, 0x00]");
	asm("  stp x21, x22, [sp, 0x10]");
	asm("  stp x23, x24, [sp, 0x20]");
	asm("  stp x25, x26, [sp, 0x30]");
	asm("  stp x27, x28, [sp, 0x40]");
	asm("  stp x29, x30, [sp, 0x50]");
	asm("  stp d8 , d9 , [sp, 0x60]");
	asm("  stp d10, d11, [sp, 0x70]");
	asm("  stp d12, d13, [sp, 0x80]");
	asm("  stp d14, d15, [sp, 0x90]");
	asm("  mov x3, sp");
	asm("  str x3, [x1]");
	asm("  and x2, x2, #-16");
	asm("  mov sp, x2");
	asm("  mov lr, #0");
	asm("  b " _TINA_SYMBOL(_tina_start));

	asm(_TINA_SYMBOL(_tina_swap:));
	asm("  sub sp, sp, 0xA0");
	asm("  stp x19, x20, [sp, 0x00]");
	asm("  stp x21, x22, [sp, 0x10]");
	asm("  stp x23, x24, [sp, 0x20]");
	asm("  stp x25, x26, [sp, 0x30]");
	asm("  stp x27, x28, [sp, 0x40]");
	asm("  stp x29, x30, [sp, 0x50]");
	asm("  stp d8 , d9 , [sp, 0x60]");
	asm("  stp d10, d11, [sp, 0x70]");
	asm("  stp d12, d13, [sp, 0x80]");
	asm("  stp d14, d15, [sp, 0x90]");
	asm("  mov x3, sp");
	asm("  str x3, [x0]");
	asm("  ldr x3, [x1]");
	asm("  mov sp, x3");
	asm("  ldp x19, x20, [sp, 0x00]");
	asm("  ldp x21, x22, [sp, 0x10]");
	asm("  ldp x23, x24, [sp, 0x20]");
	asm("  ldp x25, x26, [sp, 0x30]");
	asm("  ldp x27, x28, [sp, 0x40]");
	asm("  ldp x29, x30, [sp, 0x50]");
	asm("  ldp d8 , d9 , [sp, 0x60]");
	asm("  ldp d10, d11, [sp, 0x70]");
	asm("  ldp d12, d13, [sp, 0x80]");
	asm("  ldp d14, d15, [sp, 0x90]");
	asm("  add sp, sp, 0xA0");
	asm("  mov x0, x2");
	asm("  ret");
#elif TINA_ABI_i386
	#if __GNUC__
		asm(".intel_syntax noprefix");
		#define TINA_I386ASM(...) asm(#__VA_ARGS__)
		asm(_TINA_SYMBOL(_tina_init_stack:));
	#elif _MSC_VER
		#define TINA_I386ASM(...) __asm __VA_ARGS__
		__declspec(naked) tina* _tina_init_stack(tina* coro, tina_func* body, void** sp_loc, void* sp){
	#else
		#error Unknown/untested compiler for i386. 
	#endif
		TINA_I386ASM(mov eax, [esp + 0x04]); // coro
		TINA_I386ASM(mov ecx, [esp + 0x08]); // sp_loc
		TINA_I386ASM(mov edx, [esp + 0x0c]); // sp
		TINA_I386ASM(push ebp);
		TINA_I386ASM(push ebx);
		TINA_I386ASM(push esi);
		TINA_I386ASM(push edi);
		TINA_I386ASM(mov [ecx], esp);
		TINA_I386ASM(and edx, -16);
		TINA_I386ASM(mov esp, edx);
		TINA_I386ASM(push eax); // push argument
		TINA_I386ASM(push 0); // push empty retaddr
		TINA_I386ASM(jmp _tina_start);
	#if __GNUC__
	#elif _MSC_VER
		}
	#endif

	#if __GNUC__
		asm(_TINA_SYMBOL(_tina_swap:));
	#elif _MSC_VER
		__declspec(naked) void* _tina_swap(void** sp_from, void** sp_to, void* value){
	#endif
		TINA_I386ASM(mov eax, [esp + 0x0C]); // retval
		TINA_I386ASM(mov ecx, [esp + 0x04]); // sp_from
		TINA_I386ASM(mov edx, [esp + 0x08]); // sp_to
		TINA_I386ASM(push ebp);
		TINA_I386ASM(push ebx);
		TINA_I386ASM(push esi);
		TINA_I386ASM(push edi);
		TINA_I386ASM(mov [ecx], esp);
		TINA_I386ASM(mov esp, [edx]);
		TINA_I386ASM(pop edi);
		TINA_I386ASM(pop esi);
		TINA_I386ASM(pop ebx);
		TINA_I386ASM(pop ebp);
		TINA_I386ASM(ret);
	#if __GNUC__
		asm(".att_syntax");
	#elif _MSC_VER
		}
	#endif
#elif TINA_ABI_SysV_AMD64
	asm(".intel_syntax noprefix");
	
	asm(_TINA_SYMBOL(_tina_init_stack:));
	asm("  push rbp");
	asm("  push rbx");
	asm("  push r12");
	asm("  push r13");
	asm("  push r14");
	asm("  push r15");
	asm("  mov [rsi], rsp"); // rsi = arg1
	asm("  and rdx, -16"); // rdx = arg2
	asm("  mov rsp, rdx");
	asm("  push 0");
	asm("  jmp " _TINA_SYMBOL(_tina_start));
	
	// https://software.intel.com/sites/default/files/article/402129/mpx-linux64-abi.pdf
	asm(_TINA_SYMBOL(_tina_swap:));
	asm("  push rbp");
	asm("  push rbx");
	asm("  push r12");
	asm("  push r13");
	asm("  push r14");
	asm("  push r15");
	asm("  mov [rdi], rsp"); // rdri = arg0
	asm("  mov rsp, [rsi]"); // rsi = arg1
	asm("  pop r15");
	asm("  pop r14");
	asm("  pop r13");
	asm("  pop r12");
	asm("  pop rbx");
	asm("  pop rbp");
	asm("  mov rax, rdx"); // rax = ret, rdx = arg2
	asm("  ret");
	
	asm(".att_syntax");
#elif TINA_ABI_WIN64
	// MSVC doesn't allow inline assembly, assemble to binary blob then.
	
	#if __GNUC__
		#define TINA_SECTION_ATTRIBUTE __attribute__((section(".text#")))
	#elif _MSC_VER
		#pragma section(".text")
		#define TINA_SECTION_ATTRIBUTE __declspec(allocate(".text"))
	#else
		#error Unknown/untested compiler for Win64. 
	#endif
	
	// Assembled and dumped from win64-init.S
	TINA_SECTION_ATTRIBUTE
	const uint64_t _tina_init_stack[] = {
		0x5541544157565355, 0x2534ff6557415641,
		0x2534ff6500000008, 0x2534ff6500000010,
		0xa0ec814800001478, 0x9024b4290f000000,
		0x8024bc290f000000, 0x2444290f44000000,
		0x4460244c290f4470, 0x290f44502454290f,
		0x2464290f4440245c, 0x4420246c290f4430,
		0x290f44102474290f, 0xe08349228948243c,
		0x0c894865c4894cf0, 0x8948650000147825,
		0x4c6500000010250c, 0x6a00000008250489,
		0xb8489020ec834800, (uint64_t)_tina_start,
		0x909090909090e0ff, 0x9090909090909090,
	};

	// Assembled and dumped from win64-swap.S
	TINA_SECTION_ATTRIBUTE
	const uint64_t _tina_swap[] = {
		0x5541544157565355, 0x2534ff6557415641,
		0x2534ff6500000008, 0x2534ff6500000010,
		0xa0ec814800001478, 0x9024b4290f000000,
		0x8024bc290f000000, 0x2444290f44000000,
		0x4460244c290f4470, 0x290f44502454290f,
		0x2464290f4440245c, 0x4420246c290f4430,
		0x290f44102474290f, 0x228b48218948243c,
		0x0000009024b4280f, 0x0000008024bc280f,
		0x0f44702444280f44, 0x54280f4460244c28,
		0x40245c280f445024, 0x0f44302464280f44,
		0x74280f4420246c28, 0x48243c280f441024,
		0x8f65000000a0c481, 0x8f65000014782504,
		0x8f65000000102504, 0x5f41000000082504,
		0x5e5f5c415d415e41, 0x9090c3c0894c5d5b,
	};

// RV32 variants provided by https://github.com/28530367, thanks!
#elif TINA_ABI_riscv32d
	// 32-bit CPU + Double-Precision FPU (RV32D)
	asm("_tina_init_stack:");
	asm("  addi sp, sp, -0x9C");         // Allocate stack space
	asm("  sw  sp, (a1)");               // Save stack pointer

	// Save general-purpose registers (ra, s0-s11)
	asm("  sw  ra,   0x98(sp)");         // Save return address
	asm("  sw  s0,   0x94(sp)");         // Save s0
	asm("  sw  s1,   0x90(sp)");         // Save s1
	asm("  sw  s2,   0x8C(sp)");         // Save s2
	asm("  sw  s3,   0x88(sp)");         // Save s3
	asm("  sw  s4,   0x84(sp)");         // Save s4
	asm("  sw  s5,   0x80(sp)");         // Save s5
	asm("  sw  s6,   0x7C(sp)");         // Save s6
	asm("  sw  s7,   0x78(sp)");         // Save s7
	asm("  sw  s8,   0x74(sp)");         // Save s8
	asm("  sw  s9,   0x70(sp)");         // Save s9
	asm("  sw  s10,  0x6C(sp)");         // Save s10
	asm("  sw  s11,  0x68(sp)");         // Save s11

	// Save double-precision floating-point registers (fs0-fs11)
	asm("  fsd fs0,  0x60(sp)");         // Save fs0
	asm("  fsd fs1,  0x58(sp)");         // Save fs1
	asm("  fsd fs2,  0x50(sp)");         // Save fs2
	asm("  fsd fs3,  0x48(sp)");         // Save fs3
	asm("  fsd fs4,  0x40(sp)");         // Save fs4
	asm("  fsd fs5,  0x38(sp)");         // Save fs5
	asm("  fsd fs6,  0x30(sp)");         // Save fs6
	asm("  fsd fs7,  0x28(sp)");         // Save fs7
	asm("  fsd fs8,  0x20(sp)");         // Save fs8
	asm("  fsd fs9,  0x18(sp)");         // Save fs9
	asm("  fsd fs10, 0x10(sp)");         // Save fs10
	asm("  fsd fs11, 0x08(sp)");         // Save fs11

	asm("  andi a2, a2, ~0xF");          // Align stack
	asm("  mv sp, a2");                  // Set stack pointer
	asm("  mv ra, x0");                  // Clear return address
	asm("  tail _tina_start");           // Jump to coroutine start

	asm("_tina_swap:");
	asm("  addi sp, sp, -0x9C");         // Allocate stack space
	asm("  sw sp, (a0)");                // Save stack pointer

	// Save general-purpose registers
	asm("  sw  ra,   0x98(sp)");
	asm("  sw  s0,   0x94(sp)");
	asm("  sw  s1,   0x90(sp)");
	asm("  sw  s2,   0x8C(sp)");
	asm("  sw  s3,   0x88(sp)");
	asm("  sw  s4,   0x84(sp)");
	asm("  sw  s5,   0x80(sp)");
	asm("  sw  s6,   0x7C(sp)");
	asm("  sw  s7,   0x78(sp)");
	asm("  sw  s8,   0x74(sp)");
	asm("  sw  s9,   0x70(sp)");
	asm("  sw  s10,  0x6C(sp)");
	asm("  sw  s11,  0x68(sp)");

	// Save double-precision floating-point registers
	asm("  fsd fs0,  0x60(sp)");
	asm("  fsd fs1,  0x58(sp)");
	asm("  fsd fs2,  0x50(sp)");
	asm("  fsd fs3,  0x48(sp)");
	asm("  fsd fs4,  0x40(sp)");
	asm("  fsd fs5,  0x38(sp)");
	asm("  fsd fs6,  0x30(sp)");
	asm("  fsd fs7,  0x28(sp)");
	asm("  fsd fs8,  0x20(sp)");
	asm("  fsd fs9,  0x18(sp)");
	asm("  fsd fs10, 0x10(sp)");
	asm("  fsd fs11, 0x08(sp)");

	asm("  lw sp, (a1)");                // Restore stack pointer

	// Restore general-purpose registers
	asm("  lw  ra,   0x98(sp)");
	asm("  lw  s0,   0x94(sp)");
	asm("  lw  s1,   0x90(sp)");
	asm("  lw  s2,   0x8C(sp)");
	asm("  lw  s3,   0x88(sp)");
	asm("  lw  s4,   0x84(sp)");
	asm("  lw  s5,   0x80(sp)");
	asm("  lw  s6,   0x7C(sp)");
	asm("  lw  s7,   0x78(sp)");
	asm("  lw  s8,   0x74(sp)");
	asm("  lw  s9,   0x70(sp)");
	asm("  lw  s10,  0x6C(sp)");
	asm("  lw  s11,  0x68(sp)");

	// Restore double-precision floating-point registers
	asm("  fld fs0,  0x60(sp)");
	asm("  fld fs1,  0x58(sp)");
	asm("  fld fs2,  0x50(sp)");
	asm("  fld fs3,  0x48(sp)");
	asm("  fld fs4,  0x40(sp)");
	asm("  fld fs5,  0x38(sp)");
	asm("  fld fs6,  0x30(sp)");
	asm("  fld fs7,  0x28(sp)");
	asm("  fld fs8,  0x20(sp)");
	asm("  fld fs9,  0x18(sp)");
	asm("  fld fs10, 0x10(sp)");
	asm("  fld fs11, 0x08(sp)");

	asm("  addi sp, sp, 0x9C");          // Deallocate stack space
	asm("  mv a0, a2");                  // Set return value
	asm("  ret");                        // Return to caller

#elif TINA_ABI_riscv32f
	// 32-bit CPU + Single-Precision FPU (RV32F)
	asm("_tina_init_stack:");
	asm("  addi sp, sp, -0x68");       // Allocate stack space
	asm("  sw  sp, (a1)");            // Save stack pointer

	// Save general-purpose registers (ra, s0-s11)
	asm("  sw   ra,   0x64(sp)");   
	asm("  sw   s0,   0x60(sp)");    
	asm("  sw   s1,   0x5C(sp)");  
	asm("  sw   s2,   0x58(sp)");     
	asm("  sw   s3,   0x54(sp)");   
	asm("  sw   s4,   0x50(sp)");    
	asm("  sw   s5,   0x4C(sp)");     
	asm("  sw   s6,   0x48(sp)");   
	asm("  sw   s7,   0x44(sp)");    
	asm("  sw   s8,   0x40(sp)");  
	asm("  sw   s9,   0x3C(sp)");     
	asm("  sw   s10,  0x38(sp)");    
	asm("  sw   s11,  0x34(sp)");    

	// Save single-precision floating-point registers (fs0-fs11)
	asm("  fsw  fs0,  0x30(sp)");
	asm("  fsw  fs1,  0x2C(sp)");
	asm("  fsw  fs2,  0x28(sp)");
	asm("  fsw  fs3,  0x24(sp)");
	asm("  fsw  fs4,  0x20(sp)");
	asm("  fsw  fs5,  0x1C(sp)");
	asm("  fsw  fs6,  0x18(sp)");
	asm("  fsw  fs7,  0x14(sp)");
	asm("  fsw  fs8,  0x10(sp)");
	asm("  fsw  fs9,  0x0C(sp)");
	asm("  fsw  fs10, 0x08(sp)");
	asm("  fsw  fs11, 0x04(sp)");

	asm("  andi a2, a2, ~0xF");       // Align stack
	asm("  mv sp, a2");               // Set stack pointer
	asm("  mv ra, x0");               // Clear return address
	asm("  tail _tina_start");        // Jump to coroutine start

	asm("_tina_swap:");
	asm("  addi sp, sp, -0x68");      // Allocate stack space
	asm("  sw sp, (a0)");             // Save stack pointer

	// Save general-purpose registers
	asm("  sw   ra,   0x64(sp)");
	asm("  sw   s0,   0x60(sp)");
	asm("  sw   s1,   0x5C(sp)");
	asm("  sw   s2,   0x58(sp)");
	asm("  sw   s3,   0x54(sp)");
	asm("  sw   s4,   0x50(sp)");
	asm("  sw   s5,   0x4C(sp)");
	asm("  sw   s6,   0x48(sp)");
	asm("  sw   s7,   0x44(sp)");
	asm("  sw   s8,   0x40(sp)");
	asm("  sw   s9,   0x3C(sp)");
	asm("  sw   s10,  0x38(sp)");
	asm("  sw   s11,  0x34(sp)");

	// Save single-precision floating-point registers
	asm("  fsw  fs0,  0x30(sp)");
	asm("  fsw  fs1,  0x2C(sp)");
	asm("  fsw  fs2,  0x28(sp)");
	asm("  fsw  fs3,  0x24(sp)");
	asm("  fsw  fs4,  0x20(sp)");
	asm("  fsw  fs5,  0x1C(sp)");
	asm("  fsw  fs6,  0x18(sp)");
	asm("  fsw  fs7,  0x14(sp)");
	asm("  fsw  fs8,  0x10(sp)");
	asm("  fsw  fs9,  0x0C(sp)");
	asm("  fsw  fs10, 0x08(sp)");
	asm("  fsw  fs11, 0x04(sp)");

	asm("  lw sp, (a1)");             // Restore stack pointer

	// Restore general-purpose registers
	asm("  lw   ra,   0x64(sp)");
	asm("  lw   s0,   0x60(sp)");
	asm("  lw   s1,   0x5C(sp)");
	asm("  lw   s2,   0x58(sp)");
	asm("  lw   s3,   0x54(sp)");
	asm("  lw   s4,   0x50(sp)");
	asm("  lw   s5,   0x4C(sp)");
	asm("  lw   s6,   0x48(sp)");
	asm("  lw   s7,   0x44(sp)");
	asm("  lw   s8,   0x40(sp)");
	asm("  lw   s9,   0x3C(sp)");
	asm("  lw   s10,  0x38(sp)");
	asm("  lw   s11,  0x34(sp)");

	// Restore single-precision floating-point registers
	asm("  flw  fs0,  0x30(sp)");
	asm("  flw  fs1,  0x2C(sp)");
	asm("  flw  fs2,  0x28(sp)");
	asm("  flw  fs3,  0x24(sp)");
	asm("  flw  fs4,  0x20(sp)");
	asm("  flw  fs5,  0x1C(sp)");
	asm("  flw  fs6,  0x18(sp)");
	asm("  flw  fs7,  0x14(sp)");
	asm("  flw  fs8,  0x10(sp)");
	asm("  flw  fs9,  0x0C(sp)");
	asm("  flw  fs10, 0x08(sp)");
	asm("  flw  fs11, 0x04(sp)");

	asm("  addi sp, sp, 0x68");       // Deallocate stack space
	asm("  mv a0, a2");               // Set return value
	asm("  ret");                     // Return to caller

#elif TINA_ABI_riscv32i
	asm("_tina_init_stack:");
	asm("  addi sp, sp, -0x38");         // Allocate stack space
	asm("  sw sp, (a1)");                // Save stack pointer
	asm("  sw ra,   0x34(sp)");          // Save return address
	asm("  sw s0,   0x30(sp)");          // Save s0
	asm("  sw s1,   0x2C(sp)");          // Save s1
	asm("  sw s2,   0x28(sp)");          // Save s2
	asm("  sw s3,   0x24(sp)");          // Save s3
	asm("  sw s4,   0x20(sp)");          // Save s4
	asm("  sw s5,   0x1C(sp)");          // Save s5
	asm("  sw s6,   0x18(sp)");          // Save s6
	asm("  sw s7,   0x14(sp)");          // Save s7
	asm("  sw s8,   0x10(sp)");          // Save s8
	asm("  sw s9,   0x0C(sp)");          // Save s9
	asm("  sw s10,  0x08(sp)");          // Save s10
	asm("  sw s11,  0x04(sp)");          // Save s11
	asm("  andi a2, a2, ~0xF");          // Align stack
	asm("  mv sp, a2");
	asm("  mv ra, x0");
	asm("  tail _tina_start");

	asm("_tina_swap:");
	asm("  addi sp, sp, -0x38");         // Allocate stack space
	asm("  sw sp, (a0)");                // Save stack pointer
	asm("  sw ra,   0x34(sp)");          // Save return address
	asm("  sw s0,   0x30(sp)");          // Save s0
	asm("  sw s1,   0x2C(sp)");          // Save s1
	asm("  sw s2,   0x28(sp)");          // Save s2
	asm("  sw s3,   0x24(sp)");          // Save s3
	asm("  sw s4,   0x20(sp)");          // Save s4
	asm("  sw s5,   0x1C(sp)");          // Save s5
	asm("  sw s6,   0x18(sp)");          // Save s6
	asm("  sw s7,   0x14(sp)");          // Save s7
	asm("  sw s8,   0x10(sp)");          // Save s8
	asm("  sw s9,   0x0C(sp)");          // Save s9
	asm("  sw s10,  0x08(sp)");          // Save s10
	asm("  sw s11,  0x04(sp)");          // Save s11
	asm("  lw sp, (a1)");                // Restore stack pointer
	asm("  lw ra,   0x34(sp)");          // Restore return address
	asm("  lw s0,   0x30(sp)");          // Restore s0
	asm("  lw s1,   0x2C(sp)");          // Restore s1
	asm("  lw s2,   0x28(sp)");          // Restore s2
	asm("  lw s3,   0x24(sp)");          // Restore s3
	asm("  lw s4,   0x20(sp)");          // Restore s4
	asm("  lw s5,   0x1C(sp)");          // Restore s5
	asm("  lw s6,   0x18(sp)");          // Restore s6
	asm("  lw s7,   0x14(sp)");          // Restore s7
	asm("  lw s8,   0x10(sp)");          // Restore s8
	asm("  lw s9,   0x0C(sp)");          // Restore s9
	asm("  lw s10,  0x08(sp)");          // Restore s10
	asm("  lw s11,  0x04(sp)");          // Restore s11
	asm("  addi sp, sp, 0x38");          // Deallocate stack space
	asm("  mv a0, a2");                  // Set return value to a2
	asm("  ret");                        // Return
#else
	#error Unhandled target CPU/ABI/Compiler combination!
#endif

#endif // TINA_IMPLEMENTATION

#ifdef __cplusplus
}
#endif

#endif // TINA_H

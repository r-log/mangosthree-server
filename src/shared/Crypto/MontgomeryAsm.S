// SPDX-License-Identifier: GPL-3.0-or-later
//
// MaNGOS is a full featured server for World of Warcraft, supporting
// the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
//
// Copyright (C) 2005-2026 MaNGOS <https://www.getmangos.eu>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// ---------------------------------------------------------------------------
// The assembly tier of the Montgomery kernel: x86-64, BMI2 + ADX.
//
//   void mangos_montmul_adx(uint64_t* z, const uint64_t* x, const uint64_t* y,
//                           const uint64_t* m, uint64_t n0inv, uint64_t k);
//
//   z = x * y * 2^(-64k) mod m, for x, y < m, m odd, k a positive multiple of 4.
//   z may alias x or y. The result is canonical (below m).
//
// The same CIOS as the C++ tiers (src/shared/Crypto/Montgomery.cpp): for each
// limb of x, add x_i * y into the accumulator t, then add the multiple of m
// that clears t[0] and drop that limb; after k rounds t is below 2m and one
// masked subtraction makes it canonical. What the intrinsics cannot express is
// here: the products from mulx (which leaves the flags alone) feed two
// independent carry chains, adcx on CF for the accumulator and adox on OF for
// the carry between products, so the two additions of every step overlap.
// Inside a chain nothing may touch the flags: pointers and counters advance
// with lea and the loops close with jrcxz, both flag-neutral, and the loops
// are unrolled four limbs deep (hence k a multiple of 4).
//
// Constant-time: the instruction sequence, the loop counts and every address
// depend on k alone; the final subtraction is selected by a mask. The
// accumulator lives on the stack and is erased before returning.
//
// Register use (System V):
//   rdi z   rsi x   r10 y   r11 m   r8 n0inv   r9 k*8
//   rbp frame pointer (the stack as it was after the pushes, for unwinding)
//   rsp accumulator base: a scratch slot at [rsp], t[j] at [rsp + 8 + 8j]
//   rdx multiplier   rax:rbx product   r12 carry   r13 zero
//   r14 i*8   r15 j*8   rcx loop counter
//
// Both ABIs are in this one source under WINDOWS_ABI, each with its unwind
// metadata: CFI for the ELF build, the MASM FRAME directives for ml64 (the
// translator src/tests/tools/gas2masm.py copies the Windows branch through;
// GAS never sees it). The frame pointer is set before the k-dependent
// allocation, so an unwinder recovers the caller from rbp, and the frame is
// padded so rsp is 16-byte aligned inside the body.
//
// The GAS source is the one source; MontgomeryAsm.asm is generated from it,
// and a test checks the twin is current.
// ---------------------------------------------------------------------------

        .intel_syntax noprefix
        .text
        .balign 16
        .globl mangos_montmul_adx
#if !WINDOWS_ABI
        .type mangos_montmul_adx, @function
#endif
mangos_montmul_adx:
#if WINDOWS_ABI
        push rbp
        .pushreg rbp
        push rbx
        .pushreg rbx
        push r12
        .pushreg r12
        push r13
        .pushreg r13
        push r14
        .pushreg r14
        push r15
        .pushreg r15
        push rdi
        .pushreg rdi
        push rsi
        .pushreg rsi
        mov rbp, rsp
        .setframe rbp, 0
        .endprolog
        mov rdi, rcx
        mov rsi, rdx
        mov r10, r8
        mov r11, r9
        mov r8, qword ptr [rbp + 104]
        mov r9, qword ptr [rbp + 112]
#else
        .cfi_startproc
        push rbp
        .cfi_adjust_cfa_offset 8
        .cfi_rel_offset rbp, 0
        push rbx
        .cfi_adjust_cfa_offset 8
        .cfi_rel_offset rbx, 0
        push r12
        .cfi_adjust_cfa_offset 8
        .cfi_rel_offset r12, 0
        push r13
        .cfi_adjust_cfa_offset 8
        .cfi_rel_offset r13, 0
        push r14
        .cfi_adjust_cfa_offset 8
        .cfi_rel_offset r14, 0
        push r15
        .cfi_adjust_cfa_offset 8
        .cfi_rel_offset r15, 0
        mov rbp, rsp
        .cfi_def_cfa_register rbp
        mov r10, rdx
        mov r11, rcx
#endif
        shl r9, 3
        xor r13d, r13d

        // frame: scratch (8) + t[0 .. k+1] ((k+2)*8), rounded up to 16, plus 8 so
        // that rsp -- 8 mod 16 after the pushes in both ABIs -- is aligned inside
        lea rax, [r9 + 39]
        and rax, -16
        add rax, 8
        sub rsp, rax

        // t[0 .. k+1] = 0
        xor eax, eax
        lea rcx, [r9 + 16]
.Lzero:
        mov qword ptr [rsp + rcx], rax
        sub rcx, 8
        jnz .Lzero

        xor r14d, r14d
.Lrow:
        // ---- t += x[i] * y ------------------------------------------------
        mov rdx, qword ptr [rsi + r14]
        mov rcx, r9
        shr rcx, 5
        xor r15d, r15d
        xor r12d, r12d
.Lrow1:
        jrcxz .Lrow1_done
        mulx rbx, rax, qword ptr [r10 + r15]
        adcx rax, qword ptr [rsp + r15 + 8]
        adox rax, r12
        mov qword ptr [rsp + r15 + 8], rax
        mov r12, rbx
        mulx rbx, rax, qword ptr [r10 + r15 + 8]
        adcx rax, qword ptr [rsp + r15 + 16]
        adox rax, r12
        mov qword ptr [rsp + r15 + 16], rax
        mov r12, rbx
        mulx rbx, rax, qword ptr [r10 + r15 + 16]
        adcx rax, qword ptr [rsp + r15 + 24]
        adox rax, r12
        mov qword ptr [rsp + r15 + 24], rax
        mov r12, rbx
        mulx rbx, rax, qword ptr [r10 + r15 + 24]
        adcx rax, qword ptr [rsp + r15 + 32]
        adox rax, r12
        mov qword ptr [rsp + r15 + 32], rax
        mov r12, rbx
        lea r15, [r15 + 32]
        lea rcx, [rcx - 1]
        jmp .Lrow1
.Lrow1_done:
        // t[k] += carry + CF + OF; t[k+1] = what overflows
        mov rax, qword ptr [rsp + r9 + 8]
        adcx rax, r12
        adox rax, r13
        mov qword ptr [rsp + r9 + 8], rax
        mov rax, r13
        adcx rax, r13
        adox rax, r13
        mov qword ptr [rsp + r9 + 16], rax

        // ---- t = (t + u * m) / 2^64, u = t[0] * n0inv --------------------
        mov rdx, qword ptr [rsp + 8]
        imul rdx, r8
        mov rcx, r9
        shr rcx, 5
        xor r15d, r15d
        xor r12d, r12d
.Lrow2:
        jrcxz .Lrow2_done
        mulx rbx, rax, qword ptr [r11 + r15]
        adcx rax, qword ptr [rsp + r15 + 8]
        adox rax, r12
        mov qword ptr [rsp + r15], rax
        mov r12, rbx
        mulx rbx, rax, qword ptr [r11 + r15 + 8]
        adcx rax, qword ptr [rsp + r15 + 16]
        adox rax, r12
        mov qword ptr [rsp + r15 + 8], rax
        mov r12, rbx
        mulx rbx, rax, qword ptr [r11 + r15 + 16]
        adcx rax, qword ptr [rsp + r15 + 24]
        adox rax, r12
        mov qword ptr [rsp + r15 + 16], rax
        mov r12, rbx
        mulx rbx, rax, qword ptr [r11 + r15 + 24]
        adcx rax, qword ptr [rsp + r15 + 32]
        adox rax, r12
        mov qword ptr [rsp + r15 + 24], rax
        mov r12, rbx
        lea r15, [r15 + 32]
        lea rcx, [rcx - 1]
        jmp .Lrow2
.Lrow2_done:
        // t[k-1] = t[k] + carry + CF + OF; t[k] = t[k+1] + what overflows
        mov rax, qword ptr [rsp + r9 + 8]
        adcx rax, r12
        adox rax, r13
        mov qword ptr [rsp + r9], rax
        mov rax, qword ptr [rsp + r9 + 16]
        adcx rax, r13
        adox rax, r13
        mov qword ptr [rsp + r9 + 8], rax

        lea r14, [r14 + 8]
        cmp r14, r9
        jb .Lrow

        // ---- z = t - m, kept if t[k] != 0 or no borrow; else z = t ----------
        mov rcx, r9
        shr rcx, 5
        xor r15d, r15d
.Lsub:
        jrcxz .Lsub_done
        mov rax, qword ptr [rsp + r15 + 8]
        sbb rax, qword ptr [r11 + r15]
        mov qword ptr [rdi + r15], rax
        mov rax, qword ptr [rsp + r15 + 16]
        sbb rax, qword ptr [r11 + r15 + 8]
        mov qword ptr [rdi + r15 + 8], rax
        mov rax, qword ptr [rsp + r15 + 24]
        sbb rax, qword ptr [r11 + r15 + 16]
        mov qword ptr [rdi + r15 + 16], rax
        mov rax, qword ptr [rsp + r15 + 32]
        sbb rax, qword ptr [r11 + r15 + 24]
        mov qword ptr [rdi + r15 + 24], rax
        lea r15, [r15 + 32]
        lea rcx, [rcx - 1]
        jmp .Lsub
.Lsub_done:
        sbb rbx, rbx
        mov rax, qword ptr [rsp + r9 + 8]
        neg rax
        sbb rax, rax
        not rbx
        or rax, rbx
        mov rbx, rax
        not rbx
        xor r15d, r15d
.Lblend:
        mov rcx, qword ptr [rdi + r15]
        and rcx, rax
        mov rdx, qword ptr [rsp + r15 + 8]
        and rdx, rbx
        or rcx, rdx
        mov qword ptr [rdi + r15], rcx
        add r15, 8
        cmp r15, r9
        jb .Lblend

        // ---- erase the accumulator, release the frame ----------------------
        xor eax, eax
        lea rcx, [r9 + 16]
.Lerase:
        mov qword ptr [rsp + rcx], rax
        sub rcx, 8
        jnz .Lerase
        mov qword ptr [rsp], rax
        lea rsp, [rbp]
#if WINDOWS_ABI
        pop rsi
        pop rdi
        pop r15
        pop r14
        pop r13
        pop r12
        pop rbx
        pop rbp
        ret
#else
        .cfi_def_cfa_register rsp
        pop r15
        .cfi_adjust_cfa_offset -8
        pop r14
        .cfi_adjust_cfa_offset -8
        pop r13
        .cfi_adjust_cfa_offset -8
        pop r12
        .cfi_adjust_cfa_offset -8
        pop rbx
        .cfi_adjust_cfa_offset -8
        pop rbp
        .cfi_adjust_cfa_offset -8
        ret
        .cfi_endproc
        .size mangos_montmul_adx, .-mangos_montmul_adx
        .section .note.GNU-stack,"",@progbits
#endif

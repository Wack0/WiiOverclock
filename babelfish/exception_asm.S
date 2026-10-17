/*
 *  minute - a port of the "mini" IOS replacement for the Wii U.
 *
 *  Copyright (C) 2008, 2009    Hector Martin "marcan" <marcan@marcansoft.com>
 *
 *  This code is licensed to you under the terms of the GNU GPL, version 2;
 *  see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
 */

.arm

.globl v_instr_abrt
.globl v_data_abrt

.extern exc_handler

.text

v_instr_abrt:
    stmfd   sp!, {lr}
    stmfd   sp, {r0-lr}^
    sub     sp, sp, #0x3c
    mov     r2, sp
    mrs     r1, spsr
    mov     r0, #3

    blx     exc_handler

    ldmfd   sp!, {r0-r12}
    add     sp, sp, #8
    ldmfd   sp!, {lr}
    subs    pc, lr, #4

v_data_abrt:
    stmfd   sp!, {lr}
    stmfd   sp, {r0-lr}^
    sub     sp, sp, #0x3c
    mov     r2, sp
    mrs     r1, spsr
    mov     r0, #4

    blx     exc_handler

    ldmfd   sp!, {r0-r12}
    add     sp, sp, #8
    ldmfd   sp!, {lr}
    subs    pc, lr, #8

.pool
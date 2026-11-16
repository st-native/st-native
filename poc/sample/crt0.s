@ SPDX-License-Identifier: GPL-3.0-or-later
@ Start-up code and module parameters for the synthetic sample program (our own code).
@ The module parameter block imitates the layout NitroSDK programs carry (nine words ending in two magic numbers), so
@ the extractor's search for it can be tested without game data. The SDK version word is a placeholder of ours.

    .syntax unified
    .section .crt0, "ax", %progbits
    .arm
    .global _start
    .type _start, %function
_start:
    ldr     sp, =0x027E3F00
    ldr     r0, =start_loop
    blx     r0
1:  b       1b
    .pool

    .section .modparams, "a", %progbits
    .align 2
    .global _module_params
_module_params:
    .word   __static_end        @ autoload list start (an empty list at the end of the module)
    .word   __static_end        @ autoload list end
    .word   __static_end        @ autoload start
    .word   __bss_start         @ static bss start
    .word   __bss_end           @ static bss end
    .word   0                   @ compressed static end (written by mkrom.py)
    .word   0x00010000          @ "SDK version" placeholder (not a real SDK)
    .word   0xDEC00621          @ magic
    .word   0x2106C0DE          @ magic

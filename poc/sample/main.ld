/* SPDX-License-Identifier: GPL-3.0-or-later. Main (ARM9 static) module of the sample, at the DS's main RAM base. */
ENTRY(_start)
SECTIONS
{
    . = 0x02000000;
    .text : { KEEP(*(.crt0)) KEEP(*(.modparams)) *(.text .text.*) }
    .rodata : ALIGN(4) { *(.rodata .rodata.*) }
    .data : ALIGN(4) { *(.data .data.*) }
    . = ALIGN(4);
    __static_end = .;
    .bss (NOLOAD) : ALIGN(4) { __bss_start = .; *(.bss .bss.* COMMON) . = ALIGN(4); __bss_end = .; }
    /DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) *(.comment) }
}

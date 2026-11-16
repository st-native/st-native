/* SPDX-License-Identifier: GPL-3.0-or-later. Overlay 0 of the sample; main-module symbols come from --just-symbols. */
SECTIONS
{
    . = 0x02200000;
    .text : { *(.text .text.*) }
    .rodata : ALIGN(4) { *(.rodata .rodata.*) }
    .data : ALIGN(4) { *(.data .data.*) }
    . = ALIGN(4);
    __ovl_static_end = .;
    .bss (NOLOAD) : ALIGN(4) { *(.bss .bss.* COMMON) . = ALIGN(4); }
    /DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) *(.comment) }
}

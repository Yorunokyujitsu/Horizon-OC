/*
 * Copyright (c) Souldbminer
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * 
 */

MEMORY
{
    SRAM (rwx) : ORIGIN = 0x60000000, LENGTH = 32K
}

ENTRY(Reset_Handler)

SECTIONS
{

    .vectors :
    {
        KEEP(*(.vectors))
        . = ALIGN(4);
    } > SRAM

    .text :
    {
        *(.text*)
        *(.rodata*)
        . = ALIGN(4);
    } > SRAM

    .data :
    {
        _sdata = .;
        *(.data*)
        _edata = .;
        . = ALIGN(4);
    } > SRAM

    _sidata = _edata;

    .bss :
    {
        _sbss = .;
        *(.bss*)
        *(COMMON)
        _ebss = .;
        . = ALIGN(4);
    } > SRAM

    /* Stack at end of SRAM */
    _estack = ORIGIN(SRAM) + LENGTH(SRAM);

    /* Discard debug sections */
    /DISCARD/ :
    {
        *(.debug_*)
        *(.comment)
    }
}
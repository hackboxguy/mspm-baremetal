/* Owned MSPM0C1106 linker script.  Device facts: docs/device_facts.md. */
ENTRY(Reset_Handler)

STACK_SIZE = 0x400;
IMAGE_ID_SIZE = 0x40;

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x00000000, LENGTH = 0x00010000
    SRAM  (rwx) : ORIGIN = 0x20000000, LENGTH = 0x00002000
}

__stack_top = ORIGIN(SRAM) + LENGTH(SRAM);
__stack_limit = __stack_top - STACK_SIZE;
__image_identity_start = ORIGIN(FLASH) + LENGTH(FLASH) - IMAGE_ID_SIZE;

SECTIONS
{
    .isr_vector : ALIGN(256)
    {
        KEEP(*(.isr_vector))
        . = ALIGN(4);
    } > FLASH

    .text :
    {
        *(.text .text.*)
        *(.rodata .rodata.*)
        *(.glue_7 .glue_7t)
        . = ALIGN(4);
    } > FLASH

    .ARM.exidx :
    {
        __exidx_start = .;
        *(.ARM.exidx* .gnu.linkonce.armexidx.*)
        __exidx_end = .;
        . = ALIGN(4);
    } > FLASH

    .data :
    {
        __data_start = .;
        *(.data .data.*)
        . = ALIGN(4);
        __data_end = .;
    } > SRAM AT > FLASH
    __data_load_start = LOADADDR(.data);

    .bss (NOLOAD) :
    {
        __bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        . = ALIGN(4);
        __bss_end = .;
    } > SRAM

    .noinit (NOLOAD) :
    {
        __noinit_start = .;
        *(.noinit .noinit.*)
        . = ALIGN(4);
        __noinit_end = .;
    } > SRAM

    __heap_start = __noinit_end;
    __heap_end = __noinit_end;

    .stack __stack_limit (NOLOAD) :
    {
        . += STACK_SIZE;
    } > SRAM

    .image_identity __image_identity_start :
    {
        __image_identity_begin = .;
        KEEP(*(.image_identity))
        __image_identity_end = .;
    } > FLASH
}

ASSERT((LOADADDR(.data) + SIZEOF(.data)) <= __image_identity_start,
       "Flash image overlaps the reserved image identity block")
ASSERT(__noinit_end <= __stack_limit, "SRAM image overlaps the reserved stack")
ASSERT(__heap_start == __heap_end, "Dynamic heap is forbidden")
ASSERT(SIZEOF(.image_identity) == IMAGE_ID_SIZE,
       "Image identity placeholder must occupy its fixed reservation")

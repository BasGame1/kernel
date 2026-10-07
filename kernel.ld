OUTPUT_ARCH=( "riscv" )

ENTRY( _start )

SECTIONS = {
   . = 0x80000000;
   .text = {
      *(.text.init) *(.text .text.*)
   }
   .rodata = {
      *(.rodata .rodata.*)
   }
   .data = {
      *(.sdata .sdata.*) *(.data .data.*)
   }
   .bss = {
      *(.sbss .sbss.*) *(.bss .bss.*)
   }
   PROVIDE(_end = .);
}

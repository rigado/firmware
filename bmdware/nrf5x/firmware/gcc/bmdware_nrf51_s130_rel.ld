/* Linker script to configure memory regions. */

SEARCH_DIR(.)
GROUP(-lgcc -lc -lnosys)

MEMORY
{
  FLASH (rx) : 
  	ORIGIN = 0x1b000, 
  	LENGTH = 0xe400
  RAM (rwx) :  
  	ORIGIN = 0x20002800, 
  	LENGTH = 0x5800
}

SECTIONS
{
  .fs_data :
  {
    PROVIDE(__start_fs_data = .);
    KEEP(*(.fs_data))
    PROVIDE(__stop_fs_data = .);
  } > RAM

} INSERT AFTER .data;

INCLUDE "nrf5x_common.ld"
REPART
FLASH_PART bootloader u-boot.bin
FLASH_PART kernel zImage
FLASH_PART ramdisk ramdisk-uboot.img
FLASH_PART system system.img
ERASE_PART userdata
ERASE_PART cache
ERASE_PART fat
SET_MAC
REBOOT
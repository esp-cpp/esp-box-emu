/* The game cores' code overlay (include/core_overlay.h): only one game runs at
 * a time, so every core's code is linked at the same instruction-bus address
 * and the one a game needs is copied into PSRAM there when it launches
 * (src/core_overlay.c). The rest of the firmware's code is copied into PSRAM
 * at boot as before (CONFIG_SPIRAM_FETCH_INSTRUCTIONS); the cores that are not
 * running are in flash only.
 *
 * tools/core_overlay.py rename (POST_BUILD of each core's library) renames a
 * core archive's code to .core_ovl.<core>.* and its constructor tables to
 * .core_ovl_ctors_<core>*. A member holds its constructor table first (run on
 * the core's first load), then its code; literals stay in front of the code
 * that loads them (each object's .literal.* precede its .text.*).
 *
 * Linked bytes: the bootloader only loads an image's flash and internal RAM
 * segments, so after linking tools/core_overlay.py image copies each member's
 * relocated bytes into the flash store (src/store.c, in .flash.rodata), marks
 * the member NOBITS, and checks that nothing resident reaches into the
 * overlay. NOCROSSREFS: a link error if one core's code references another's.
 *
 * 0x43800000 (MMU linear page 384) is far above everything IDF maps at boot
 * (flash .text and .rodata, then all of the PSRAM) and anything mapped later
 * (src/core_overlay.c map_window()); its data-bus alias (0x3D800000) is never
 * used. */
MEMORY
{
  core_ovl_seg (RX) : org = 0x43800000, len = 0x800000
}

SECTIONS
{
  OVERLAY : NOCROSSREFS
  {
    .core_ovl.nes
    {
      _core_ovl_ctors_nes_start = ABSOLUTE(.);
      KEEP(*(SORT(.core_ovl_ctors_nes.*))) KEEP(*(.core_ovl_ctors_nes))
      _core_ovl_ctors_nes_end = ABSOLUTE(.);
      *(.core_ovl.nes.*)
      . = ALIGN(16);
    }
    .core_ovl.gbc
    {
      _core_ovl_ctors_gbc_start = ABSOLUTE(.);
      KEEP(*(SORT(.core_ovl_ctors_gbc.*))) KEEP(*(.core_ovl_ctors_gbc))
      _core_ovl_ctors_gbc_end = ABSOLUTE(.);
      *(.core_ovl.gbc.*)
      . = ALIGN(16);
    }
    .core_ovl.sms
    {
      _core_ovl_ctors_sms_start = ABSOLUTE(.);
      KEEP(*(SORT(.core_ovl_ctors_sms.*))) KEEP(*(.core_ovl_ctors_sms))
      _core_ovl_ctors_sms_end = ABSOLUTE(.);
      *(.core_ovl.sms.*)
      . = ALIGN(16);
    }
    .core_ovl.msx
    {
      _core_ovl_ctors_msx_start = ABSOLUTE(.);
      KEEP(*(SORT(.core_ovl_ctors_msx.*))) KEEP(*(.core_ovl_ctors_msx))
      _core_ovl_ctors_msx_end = ABSOLUTE(.);
      *(.core_ovl.msx.*)
      . = ALIGN(16);
    }
    .core_ovl.genesis
    {
      _core_ovl_ctors_genesis_start = ABSOLUTE(.);
      KEEP(*(SORT(.core_ovl_ctors_genesis.*))) KEEP(*(.core_ovl_ctors_genesis))
      _core_ovl_ctors_genesis_end = ABSOLUTE(.);
      *(.core_ovl.genesis.*)
      . = ALIGN(16);
    }
    .core_ovl.doom
    {
      _core_ovl_ctors_doom_start = ABSOLUTE(.);
      KEEP(*(SORT(.core_ovl_ctors_doom.*))) KEEP(*(.core_ovl_ctors_doom))
      _core_ovl_ctors_doom_end = ABSOLUTE(.);
      *(.core_ovl.doom.*)
      . = ALIGN(16);
    }
    .core_ovl.darkforces
    {
      _core_ovl_ctors_darkforces_start = ABSOLUTE(.);
      KEEP(*(SORT(.core_ovl_ctors_darkforces.*))) KEEP(*(.core_ovl_ctors_darkforces))
      _core_ovl_ctors_darkforces_end = ABSOLUTE(.);
      *(.core_ovl.darkforces.*)
      . = ALIGN(16);
    }
    .core_ovl.pico8
    {
      _core_ovl_ctors_pico8_start = ABSOLUTE(.);
      KEEP(*(SORT(.core_ovl_ctors_pico8.*))) KEEP(*(.core_ovl_ctors_pico8))
      _core_ovl_ctors_pico8_end = ABSOLUTE(.);
      *(.core_ovl.pico8.*)
      . = ALIGN(16);
    }
  } > core_ovl_seg
  _core_ovl_start = ADDR(.core_ovl.nes);
  /* after an OVERLAY, . is its start plus its largest member */
  _core_ovl_end = ALIGN(., 16);
}
_core_ovl_nes_size = SIZEOF(.core_ovl.nes);
_core_ovl_gbc_size = SIZEOF(.core_ovl.gbc);
_core_ovl_sms_size = SIZEOF(.core_ovl.sms);
_core_ovl_msx_size = SIZEOF(.core_ovl.msx);
_core_ovl_genesis_size = SIZEOF(.core_ovl.genesis);
_core_ovl_doom_size = SIZEOF(.core_ovl.doom);
_core_ovl_darkforces_size = SIZEOF(.core_ovl.darkforces);
_core_ovl_pico8_size = SIZEOF(.core_ovl.pico8);
ASSERT(_core_ovl_start % 0x10000 == 0, "the core overlay must start on an MMU page")

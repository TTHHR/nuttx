/****************************************************************************
 * ESP-IDF application descriptor for the NuttX factory image.
 *
 * The ESP-IDF 6.x second stage bootloader locates esp_app_desc_t at the
 * beginning of the first segment of the application image and validates the
 * eFuse block revision range carried in it.  Without this structure the
 * bootloader interprets application code as a descriptor and refuses to
 * boot the image ("Image requires efuse blk rev >= vX.YZ").
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 ****************************************************************************/

#include <stdint.h>

struct esp_app_desc_s
{
  uint32_t magic_word;
  uint32_t secure_version;
  uint32_t reserved1[2];
  char version[32];
  char project_name[32];
  char time[16];
  char date[16];
  char idf_ver[32];
  uint8_t app_elf_sha256[32];
  uint16_t min_efuse_blk_rev_full;
  uint16_t max_efuse_blk_rev_full;
  uint8_t mmu_page_size;
  uint8_t spi_flash_mode;
  uint8_t reserved3[2];
  uint32_t reserved2[18];
};

const struct esp_app_desc_s g_clawdmochi_esp_app_desc
  __attribute__((section(".esp_app_desc"), used, aligned(16))) =
{
  .magic_word = 0xabcd5432,
  .secure_version = 0,
  .version = "0.1.0",
  .project_name = "clawdmochi",
  .time = "boot",
  .date = "2026-11-17",
  .idf_ver = "nuttx",

  /* Do not constrain the eFuse block revision: 0 means "minimum 0.0" and
   * 0xffff means "maximum 655.35", i.e. accept the running chip.
   */

  .min_efuse_blk_rev_full = 0,
  .max_efuse_blk_rev_full = 0xffff,

  /* ESP_MMU_PAGE_SIZE_64KB == 0, SPI flash mode DIO == 2 (matches the image
   * header generated for this port).
   */

  .mmu_page_size = 0,
  .spi_flash_mode = 2,
};

/* Keep the descriptor object reachable so that the board archive member is
 * pulled in by the linker (the section is KEEP()'d by the linker script).
 */

void esp_app_desc_link_anchor(void)
{
  __asm__ volatile("" ::: "memory");
}

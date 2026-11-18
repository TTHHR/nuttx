/****************************************************************************
 * boards/risc-v/esp32c3/common/src/esp_board_spiflash.c
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <sys/mount.h>
#include <sys/stat.h>

#include "inttypes.h"
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <nuttx/debug.h>

#include <sys/param.h>

#include <nuttx/kmalloc.h>
#include <nuttx/spi/spi.h>
#include <nuttx/mtd/mtd.h>
#include <nuttx/mtd/configdata.h>
#include <nuttx/fs/nxffs.h>
#include <nuttx/fs/partition.h>

#if defined(CONFIG_ESPRESSIF_SPIFLASH_SMARTFS)
#  include <nuttx/fs/smart.h>
#  include <sys/ioctl.h>
#  include <fcntl.h>
#  include <unistd.h>
#endif

#include "espressif/esp_spiflash.h"
#include "espressif/esp_spiflash_mtd.h"

#include "esp_board_spiflash.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/****************************************************************************
 * Private Types
 ****************************************************************************/

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

#ifdef CONFIG_ESPRESSIF_HAVE_OTA_PARTITION
static int init_ota_partitions(void);
#endif

/****************************************************************************
 * Private Data
 ****************************************************************************/

#ifdef CONFIG_ESPRESSIF_HAVE_OTA_PARTITION
static const struct partition_s g_ota_partition_table[] =
{
  {
    .name       = CONFIG_ESPRESSIF_OTA_PRIMARY_SLOT_DEVPATH,
    .index      = 0,
    .firstblock = CONFIG_ESPRESSIF_OTA_PRIMARY_SLOT_OFFSET,
    .blocksize  = CONFIG_ESPRESSIF_OTA_SLOT_SIZE,
  },
  {
    .name       = CONFIG_ESPRESSIF_OTA_SECONDARY_SLOT_DEVPATH,
    .index      = 1,
    .firstblock = CONFIG_ESPRESSIF_OTA_SECONDARY_SLOT_OFFSET,
    .blocksize  = CONFIG_ESPRESSIF_OTA_SLOT_SIZE,
  },
  {
    .name       = CONFIG_ESPRESSIF_OTA_SCRATCH_DEVPATH,
    .index      = 2,
    .firstblock = CONFIG_ESPRESSIF_OTA_SCRATCH_OFFSET,
    .blocksize  = CONFIG_ESPRESSIF_OTA_SCRATCH_SIZE,
  }
};
#endif

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: init_ota_partitions
 *
 * Description:
 *   Initialize partitions that are dedicated to firmware OTA update.
 *
 * Input Parameters:
 *   None.
 *
 * Returned Value:
 *   Zero on success; a negated errno value on failure.
 *
 ****************************************************************************/

#ifdef CONFIG_ESPRESSIF_HAVE_OTA_PARTITION
static int init_ota_partitions(void)
{
  struct mtd_dev_s *mtd;
  int ret = OK;
  int i;

  for (i = 0; i < nitems(g_ota_partition_table); ++i)
    {
      const struct partition_s *part = &g_ota_partition_table[i];
      mtd = esp_spiflash_alloc_mtdpart(part->firstblock, part->blocksize);

      ret = register_mtddriver(part->name, mtd, 0755, NULL);
      if (ret < 0)
        {
          syslog(LOG_ERR, "ERROR: register_mtddriver %s failed: %d\n",
                 part->name, ret);
          return ret;
        }
    }

  return ret;
}

#endif

/****************************************************************************
 * Name: format_smartfs
 *
 * Description:
 *   Format a SMART block driver.  This is the in-kernel equivalent of the
 *   mksmartfs utility (apps/fsutils/mksmartfs).  It is only called when a
 *   mount failed with -ENODEV, i.e. when the partition has never been
 *   formatted, so existing data is never destroyed by this path.
 *
 * Input Parameters:
 *   path - Path of the SMART block driver, for example "/dev/smart0".
 *
 * Returned Value:
 *   Zero on success; a negated errno value on failure.
 *
 ****************************************************************************/

#if defined(CONFIG_ESPRESSIF_SPIFLASH_SMARTFS)
static int format_smartfs(const char *path)
{
  struct smart_read_write_s request;
  uint8_t type = SMARTFS_SECTOR_TYPE_DIR;
  int fd;
  int ret;

  fd = open(path, O_RDWR);
  if (fd < 0)
    {
      return -errno;
    }

  /* Erase the partition and write the low-level SMART format header.  A
   * sector size of zero selects CONFIG_MTD_SMART_SECTOR_SIZE.
   */

  ret = ioctl(fd, BIOC_LLFORMAT, 0);
  if (ret < 0)
    {
      ret = -errno;
      goto errout;
    }

  /* Allocate and initialize the root directory sector. */

  request.logsector = SMARTFS_ROOT_DIR_SECTOR;
  request.offset    = 0;
  request.count     = 1;
  request.buffer    = &type;

  ret = ioctl(fd, BIOC_ALLOCSECT, SMARTFS_ROOT_DIR_SECTOR);
  if (ret != SMARTFS_ROOT_DIR_SECTOR)
    {
      ret = -EIO;
      goto errout;
    }

  ret = ioctl(fd, BIOC_WRITESECT, (unsigned long)((uintptr_t)&request));
  if (ret < 0)
    {
      ret = -EIO;
      goto errout;
    }

  ret = OK;

errout:
  close(fd);
  return ret;
}
#endif

/****************************************************************************
 * Name: setup_smartfs
 *
 * Description:
 *   Provide a block driver wrapper around MTD partition and mount a
 *   SMART FS over it.
 *
 * Parameters:
 *   smartn - Number used to register the mtd partition: /dev/smartx, where
 *            x = smartn.
 *   mtd    - Pointer to a pre-allocated mtd partition.
 *   mnt_pt - Mount point
 *
 * Returned Value:
 *   Zero on success; a negated errno value on failure.
 *
 ****************************************************************************/

#if defined (CONFIG_ESPRESSIF_SPIFLASH_SMARTFS)
static int setup_smartfs(int smartn, struct mtd_dev_s *mtd,
                         const char *mnt_pt)
{
  int ret = OK;
  char path[22];

  ret = smart_initialize(smartn, mtd, NULL);
  if (ret == -EEXIST)
    {
      /* The block driver is already registered (for instance when the flash
       * was formatted by a previous session): just mount it.
       */

      ret = OK;
    }
  else if (ret < 0)
    {
      syslog(LOG_INFO, "smart_initialize failed, "
             "Trying to erase first...\n");
      ret = mtd->ioctl(mtd, MTDIOC_BULKERASE, 0);
      if (ret < 0)
        {
          syslog(LOG_ERR, "ERROR: ioctl(BULKERASE) failed: %d\n", ret);
          return ret;
        }

      syslog(LOG_INFO, "Erase successful, initializing it again.\n");
      ret = smart_initialize(smartn, mtd, NULL);
      if (ret < 0)
        {
          syslog(LOG_ERR, "ERROR: smart_initialize failed: %d\n", ret);
          return ret;
        }
    }

  if (mnt_pt != NULL)
    {
      snprintf(path, sizeof(path), "/dev/smart%d", smartn);

      ret = nx_mount(path, mnt_pt, "smartfs", 0, NULL);
      if (ret == -ENODEV)
        {
          /* The partition has not been formatted yet.  This is the normal
           * state after "erase_flash" or on a brand new device, because the
           * image written by the flash script only covers the bootloader,
           * the partition table and the NuttX application.  Format the
           * storage partition once and retry the mount.  Other mount errors
           * are reported as-is and never destroy the partition contents.
           */

          syslog(LOG_WARNING, "Smartfs unformatted, formatting %s\n", path);

          ret = format_smartfs(path);
          if (ret < 0)
            {
              syslog(LOG_ERR, "ERROR: Failed to format %s: %d\n",
                     path, ret);
              return ret;
            }

          ret = nx_mount(path, mnt_pt, "smartfs", 0, NULL);
        }

      if (ret < 0)
        {
          syslog(LOG_ERR, "ERROR: Failed to mount the FS volume: %d\n",
                 ret);
          return ret;
        }
    }

  return ret;
}
#endif

/****************************************************************************
 * Name: setup_littlefs
 *
 * Description:
 *   Register a mtd driver and mount a Little FS over it.
 *
 * Parameters:
 *   path   - Path name used to register the mtd driver.
 *   mtd    - Pointer to a pre-allocated mtd partition.
 *   mnt_pt - Mount point
 *   priv   - Privileges
 *
 * Returned Value:
 *   Zero on success; a negated errno value on failure.
 *
 ****************************************************************************/

#if defined (CONFIG_ESPRESSIF_SPIFLASH_LITTLEFS)
static int setup_littlefs(const char *path, struct mtd_dev_s *mtd,
                          const char *mnt_pt, int priv)
{
  int ret = OK;

  ret = register_mtddriver(path, mtd, priv, NULL);
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: Failed to register MTD: %d\n", ret);
      return ERROR;
    }

  if (mnt_pt != NULL)
    {
      ret = nx_mount(path, mnt_pt, "littlefs", 0, NULL);
      if (ret < 0)
        {
          ret = nx_mount(path, mnt_pt, "littlefs", 0, "forceformat");
          if (ret < 0)
            {
              syslog(LOG_ERR, "ERROR: Failed to mount the FS volume: %d\n",
                     ret);
              return ret;
            }
        }
    }

  return OK;
}
#endif

/****************************************************************************
 * Name: setup_spiffs
 *
 * Description:
 *   Register a mtd driver and mount a SPIFFS over it.
 *
 * Parameters:
 *   path   - Path name used to register the mtd driver.
 *   mtd    - Pointer to a pre-allocated mtd partition.
 *   mnt_pt - Mount point
 *   priv   - Privileges
 *
 * Returned Value:
 *   Zero on success; a negated errno value on failure.
 *
 ****************************************************************************/

#if defined  (CONFIG_ESPRESSIF_SPIFLASH_SPIFFS)
static int setup_spiffs(const char *path, struct mtd_dev_s *mtd,
                        const char *mnt_pt, int priv)
{
  int ret = OK;

  ret = register_mtddriver(path, mtd, priv, NULL);
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: Failed to register MTD: %d\n", ret);
      return ERROR;
    }

  if (mnt_pt != NULL)
    {
      ret = nx_mount(path, mnt_pt, "spiffs", 0, NULL);
      if (ret < 0)
        {
          syslog(LOG_ERR, "ERROR: Failed to mount the FS volume: %d\n",
                 ret);
          return ret;
        }
    }

  return ret;
}
#endif

/****************************************************************************
 * Name: setup_nxffs
 *
 * Description:
 *   Register a mtd driver and mount a NXFFS over it.
 *
 * Parameters:
 *   mtd    - Pointer to a pre-allocated mtd partition.
 *   mnt_pt - Mount point
 *
 * Returned Value:
 *   Zero on success; a negated errno value on failure.
 *
 ****************************************************************************/

#if defined (CONFIG_ESPRESSIF_SPIFLASH_NXFFS)
static int setup_nxffs(struct mtd_dev_s *mtd, const char *mnt_pt)
{
  int ret = OK;

  ret = nxffs_initialize(mtd);
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: NXFFS init failed: %d\n", ret);
      return ret;
    }

  if (mnt_pt != NULL)
    {
      ret = nx_mount(NULL, mnt_pt, "nxffs", 0, NULL);
      if (ret < 0)
        {
          syslog(LOG_ERR, "ERROR: Failed to mount the FS volume: %d\n",
                 ret);
          return ret;
        }
    }

  return ret;
}
#endif

/****************************************************************************
 * Name: init_storage_partition
 *
 * Description:
 *   Initialize partition that is dedicated to general use.
 *
 * Input Parameters:
 *   None
 *
 * Returned Value:
 *   Zero on success; a negated errno value on failure.
 *
 ****************************************************************************/

static int init_storage_partition(void)
{
  int ret = OK;
  struct mtd_dev_s *mtd;

  mtd = esp_spiflash_alloc_mtdpart(CONFIG_ESPRESSIF_STORAGE_MTD_OFFSET,
                                   CONFIG_ESPRESSIF_STORAGE_MTD_SIZE);
  if (!mtd)
    {
      syslog(LOG_ERR, "ERROR: Fail to alloc MTD partition of SPI Flash\n");
      return ERROR;
    }

  /* The mount point lives in the pseudo root file system, which is created
   * from scratch on every boot.  Make sure it exists before mounting;
   * otherwise nx_mount() fails with -ENOTDIR and the data partition is not
   * available at all.
   */

  ret = mkdir(CONFIG_ESPRESSIF_SPIFLASH_FS_MOUNT_PT, 0755);
  if (ret < 0 && errno != EEXIST)
    {
      syslog(LOG_ERR, "ERROR: Failed to create mount point %s: %d\n",
             CONFIG_ESPRESSIF_SPIFLASH_FS_MOUNT_PT, errno);
      return -errno;
    }

#if defined (CONFIG_ESPRESSIF_SPIFLASH_SMARTFS)

  ret = setup_smartfs(0, mtd, CONFIG_ESPRESSIF_SPIFLASH_FS_MOUNT_PT);
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: Failed to setup smartfs\n");
      return ret;
    }

#elif defined (CONFIG_ESPRESSIF_SPIFLASH_NXFFS)

  ret = setup_nxffs(mtd, CONFIG_ESPRESSIF_SPIFLASH_FS_MOUNT_PT);
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: Failed to setup nxffs\n");
      return ret;
    }

#elif defined (CONFIG_ESPRESSIF_SPIFLASH_LITTLEFS)

  const char *path = "/dev/espflash";
  ret = setup_littlefs(path, mtd, CONFIG_ESPRESSIF_SPIFLASH_FS_MOUNT_PT,
                       0755);
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: Failed to setup littlefs\n");
      return ret;
    }

#elif defined (CONFIG_ESPRESSIF_SPIFLASH_SPIFFS)

  const char *path = "/dev/espflash";
  ret = setup_spiffs(path, mtd, CONFIG_ESPRESSIF_SPIFLASH_FS_MOUNT_PT,
                     0755);
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: Failed to setup spiffs\n");
      return ret;
    }

#elif defined (CONFIG_ESPRESSIF_SPIFLASH_MTD_CONFIG)

#  if defined (CONFIG_TESTING_MTD_CONFIG_NVS)

  /* To test power-loss resilient kv system,
   * we write possible power-loss flash layout into flash
   * then start kv system to see if it recovers.
   * To do so, we need a mtd driver so that test code can
   * write into flash.
   */

  const char *path = CONFIG_TESTING_MTD_CONFIG_MOUNTPT_NAME;
  ret = register_mtddriver(path, mtd, 0777, NULL);
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: Failed to register MTD: %d\n", ret);
      return ret;
    }

#  else

  ret = mtdconfig_register(mtd);
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: Failed to setup mtd config\n");
      return ret;
    }

#  endif

#else

  ret = register_mtddriver("/dev/mtdblock0", mtd, 0755, NULL);
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: Failed to register MTD mtdblock0: %d\n", ret);
      return ret;
    }

#endif

  return ret;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: board_spiflash_init
 *
 * Description:
 *   Initialize the SPI Flash and register the MTD device.
 *
 * Input Parameters:
 *   None.
 *
 * Returned Value:
 *   Zero (OK) is returned on success; A negated errno value is returned
 *   to indicate the nature of any failure.
 *
 ****************************************************************************/

int board_spiflash_init(void)
{
  int ret = OK;

#ifdef CONFIG_ESPRESSIF_HAVE_OTA_PARTITION
  ret = init_ota_partitions();
  if (ret < 0)
    {
      return ret;
    }
#endif

  ret = init_storage_partition();
  if (ret < 0)
    {
      return ret;
    }

  return ret;
}

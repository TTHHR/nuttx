/****************************************************************************
 * arch/risc-v/src/esp32c3/esp32c3_textheap.c
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

#include <stdint.h>
#include <stdbool.h>

#include <nuttx/arch.h>
#include <nuttx/kmalloc.h>

#include <soc/soc.h>

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: up_textheap_memalign
 *
 * Description:
 *   Allocate executable module memory from the DRAM/IRAM aliased region.
 *
 ****************************************************************************/

void *up_textheap_memalign(size_t align, size_t size)
{
  void *ret;

  ret = kmm_memalign(align, size);
  if (ret != NULL)
    {
      ret = (void *)MAP_DRAM_TO_IRAM((uintptr_t)ret);
    }

  return ret;
}

/****************************************************************************
 * Name: up_textheap_free
 *
 * Description:
 *   Free executable module memory.
 *
 ****************************************************************************/

void up_textheap_free(void *p)
{
  if (p != NULL)
    {
      p = (void *)MAP_IRAM_TO_DRAM((uintptr_t)p);
      kmm_free(p);
    }
}

/****************************************************************************
 * Name: up_textheap_heapmember
 *
 * Description:
 *   Test whether the pointer belongs to the module text heap.
 *
 ****************************************************************************/

bool up_textheap_heapmember(void *p)
{
  if (p == NULL)
    {
      return false;
    }

  p = (void *)MAP_IRAM_TO_DRAM((uintptr_t)p);
  return kmm_heapmember(p);
}

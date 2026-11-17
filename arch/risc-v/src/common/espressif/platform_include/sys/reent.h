/*
 * NuttX compatibility shim for <sys/reent.h>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _SYS_REENT_H_
#define _SYS_REENT_H_

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct _reent
{
  int _nuttx_placeholder;
};

/* Note: __getreent() is a real function implemented by the port, do not
 * provide a macro for it here.
 */

#ifdef __cplusplus
}
#endif

#endif /* _SYS_REENT_H_ */

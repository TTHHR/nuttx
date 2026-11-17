/*
 * NuttX compatibility shim for <sys/cdefs.h>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _SYS_CDEFS_H_
#define _SYS_CDEFS_H_

#ifdef __cplusplus
#  define __BEGIN_DECLS extern "C" {
#  define __END_DECLS   }
#else
#  define __BEGIN_DECLS
#  define __END_DECLS
#endif

#ifndef __P
#  ifdef __STDC__
#    define __P(args) args
#  else
#    define __P(args) ()
#  endif
#endif

#ifndef __THROW
#  ifdef __cplusplus
#    define __THROW noexcept(true)
#  else
#    define __THROW
#  endif
#endif

#ifndef __THROWNL
#  define __THROWNL __THROW
#endif

#ifndef __nonnull
#  define __nonnull(params) __attribute__((__nonnull__ params))
#endif

#ifndef __pure
#  define __pure __attribute__((__pure__))
#endif

#ifndef __const
#  define __const __attribute__((__const__))
#endif

#ifndef __dead2
#  define __dead2 __attribute__((__noreturn__))
#endif

#ifndef __packed
#  define __packed __attribute__((__packed__))
#endif

#ifndef __offsetof
#  define __offsetof(type, field) __builtin_offsetof(type, field)
#endif

#ifndef __containerof
#  define __containerof(ptr, type, member) \
    ((type *)((unsigned char *)(ptr) - __offsetof(type, member)))
#endif

#ifndef __aligned
#  define __aligned(x) __attribute__((__aligned__(x)))
#endif

#ifndef __unused
#  define __unused __attribute__((__unused__))
#endif

#ifndef __used
#  define __used __attribute__((__used__))
#endif

#ifndef __printflike
#  define __printflike(fmtarg, firstvararg) \
    __attribute__((__format__(__printf__, fmtarg, firstvararg)))
#endif

#ifndef __scanflike
#  define __scanflike(fmtarg, firstvararg) \
    __attribute__((__format__(__scanf__, fmtarg, firstvararg)))
#endif

#ifndef __weak_reference
#  define __weak_reference(sym, alias) \
    __asm__(".weak " #alias);          \
    __asm__(".equ " #alias ", " #sym)
#endif

#ifndef __RCSID
#  define __RCSID(s)
#endif

#ifndef __FBSDID
#  define __FBSDID(s)
#endif

#ifndef __IDSTRING
#  define __IDSTRING(name, string)
#endif

#ifndef __COPYRIGHT
#  define __COPYRIGHT(s)
#endif

#endif /* _SYS_CDEFS_H_ */

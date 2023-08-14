// Copyright (c) 2011-2021 Columbia University, System Level Design Group
// SPDX-License-Identifier: Apache-2.0
#ifndef _SYSARRAY_RTL_H_
#define _SYSARRAY_RTL_H_

#ifdef __KERNEL__
#include <linux/ioctl.h>
#include <linux/types.h>
#else
#include <sys/ioctl.h>
#include <stdint.h>
#ifndef __user
#define __user
#endif
#endif /* __KERNEL__ */

#include "libsysarray.h"

#define SYSARRAY_DEVICES 2
;
#endif /* _SYSARRAY_RTL_H_ */

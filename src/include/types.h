#ifndef _XBOOK_TYPES_H
#define _XBOOK_TYPES_H


#include <xbook/config.h>

typedef long pid_t;
typedef unsigned long flags_t;
typedef unsigned long register_t;    //寄存器
typedef unsigned long off_t;         //偏移类型的变量
typedef unsigned long sector_t;
typedef unsigned int mode_t;
typedef unsigned long dev_t;
typedef unsigned long blksize_t;
typedef unsigned long blkcnt_t;
typedef long clock_t;
typedef long time_t;
typedef int clockid_t;
typedef unsigned long addr_t;

typedef unsigned long ino_t;
typedef unsigned long nlink_t;
typedef unsigned int uid_t;
typedef unsigned int gid_t;
typedef unsigned long useconds_t;

typedef signed char				s8_t;
typedef unsigned char			u8_t;
typedef signed short			s16_t;
typedef unsigned short			u16_t;
typedef signed int				s32_t;
typedef unsigned int			u32_t;
typedef signed long long		s64_t;
typedef unsigned long long		u64_t;

typedef long long   loff_t;
/* 在线程中作为形参 */
typedef void task_func_t(void *);

#endif  /* _XBOOK_TYPES_H */

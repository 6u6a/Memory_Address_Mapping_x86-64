#include<linux/ioctl.h>

#define DEV_IOC_MAGIC 'w'

#define DEV_GET_SYSMEM _IOR(DEV_IOC_MAGIC, 1, long)

#define DEV_NR_MAX 1


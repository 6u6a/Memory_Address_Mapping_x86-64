#include<linux/ioctl.h>

#define DEV_IOC_MAGIC 'w'

#define DEV_GET_SYSMEM _IOR(DEV_IOC_MAGIC, 1, long) //获得系统总物理内存
#define DEV_GET_RMAP _IOR(DEV_IOC_MAGIC, 2, long) //获得反向映射信息

#define DEV_NR_MAX 2
#define PAGE_MAPPING_ANON	1 //匿名物理页
#define PAGE_MAPPING_KSM	2 //KSM维护的物理页
#define PAGE_MAPPING_FILE	3 //映射文件的物理页
#define PAGE_MAPPING_FLAGS	(PAGE_MAPPING_ANON | PAGE_MAPPING_KSM)

#define MAX_PROCESS 100-1

typedef struct{
	unsigned long va[MAX_PROCESS];//对应的虚拟地址列表
	int pid[MAX_PROCESS];//对应的进程id列表，与上述虚拟地址列表一一对应
	int pid_c;//反向映射个数
}va_process;

typedef struct{
	unsigned long pa;//物理地址，最后两位表示该物理页的类型
	va_process process;
}pa_to_process;

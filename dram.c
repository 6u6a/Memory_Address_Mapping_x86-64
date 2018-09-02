//-------------------------------------------------------------------
//	dram.c
//
//	This module implements a Linux character-mode device-driver
//	for the processor's installed physical memory.  It utilizes
//	the kernel's 'kmap()' function, as a uniform way to provide
//	access to all the memory-zones (including the "high memory"
//	on systems with more than 896MB of installed physical ram).
//	The access here is 'read-only' because we deem it too risky
//	to the stable functioning of our system to allow every user
//	the unrestricted ability to arbitrarily modify memory-areas
//	which might contain some "critical" kernel data-structures.
//	We implement an 'llseek()' method so that users can readily
//	find out how much physical processor-memory is installed.
//
//	NOTE: Developed and tested with Linux kernel version 2.6.10
//
//	programmer: ALLAN CRUSE
//	written on: 30 JAN 2005
//	revised on: 28 JAN 2008 -- for Linux kernel version 2.6.22.5
//	revised on: 06 FEB 2008 -- for machines having 4GB of memory
//-------------------------------------------------------------------

#include <linux/module.h>	// for module_init()
//#include <linux/highmem.h>	// for kmap(), kunmap()
#include <asm/uaccess.h>	// for copy_to_user()
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/ksm.h>
#include <linux/rmap.h>
#include <linux/kallsyms.h>
#include "dram_dev.h"

#define RMAP_WALK_NAME "rmap_walk"
char modname[] = "dram";	// for displaying driver's name
int my_major = 85;		// note static major assignment
unsigned long	dram_size;		// total bytes of system memory

loff_t my_llseek( struct file *file, loff_t offset, int whence );
ssize_t my_read( struct file *file, char *buf, size_t count, loff_t *pos );
long self_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
int (*rmap_walk_dram)(struct page *, int (*rmap_one)(struct page *, struct vm_area_struct *, unsigned long, void *), void *);

struct file_operations
my_fops =	{
		owner:		THIS_MODULE,
		llseek:		my_llseek,
		read:		my_read,
		unlocked_ioctl: self_ioctl,
		};

static struct miscdevice dram_dev =
{
    /*
     * 内核动态生成minor
     */
    MISC_DYNAMIC_MINOR,
    /*
     * 设备名字见宏DEV_NAME
     */
    modname,
    /*
     * 绑定文件操作函数
     */
    &my_fops
};

static int __init dram_init( void )
{
	printk( "<1>\nInstalling \'%s\' module ", modname );
	rmap_walk_dram = kallsyms_lookup_name(RMAP_WALK_NAME);
	if((unsigned long)rmap_walk_dram == 0){
		printk("kallsyms_lookup_name fail!\n");
		return -1;
	}
	dram_size = (unsigned long)num_physpages << PAGE_SHIFT;
	printk( "<1>  ramtop=%08lX (%lu MB)\n", dram_size, dram_size >> 20 );
	printk("PAGE_OFFSET=%08lx\n", PAGE_OFFSET);
	return 	misc_register(&dram_dev);
	//return 	register_chrdev( my_major, modname, &my_fops );
}

static void __exit dram_exit( void )
{
	misc_deregister(&dram_dev);
	//unregister_chrdev( my_major, modname );
	printk( "<1>Removing \'%s\' module\n", modname );
}

ssize_t my_read( struct file *file, char *buf, size_t count, loff_t *pos )
{
	struct page	*pp;
	void		*from;
	int		page_number, page_indent, more;

	// we cannot read beyond the end-of-file
	if ( *pos >= dram_size ) return 0;

	// determine which physical page to temporarily map
	// and how far into that page to begin reading from
	page_number = *pos / PAGE_SIZE;
	page_indent = *pos % PAGE_SIZE;

	// map the designated physical page into kernel space
	/*If kerel vesion is 2.6.32 or later, please use pfn_to_page() to get page, and include
	    asm-generic/memory_model.h*/
#if 1
       pp = pfn_to_page( page_number);
#else
	pp = &mem_map[ page_number ];
#endif

	from = kmap( pp ) + page_indent;

	// cannot reliably read beyond the end of this mapped page
	if ( page_indent + count > PAGE_SIZE ) count = PAGE_SIZE - page_indent;

	// now transfer count bytes from mapped page to user-supplied buffer
	more = copy_to_user( buf, from, count );

	// ok now to discard the temporary page mapping
	kunmap( pp );

	// an error occurred if less than count bytes got copied
	if ( more ) return -EFAULT;

	// otherwise advance file-pointer and report number of bytes read
	*pos += count;
	return	count;
}

loff_t my_llseek( struct file *file, loff_t offset, int whence )
{
	unsigned long	newpos = -1;

	switch( whence )
		{
		case 0: newpos = offset; break;			// SEEK_SET
		case 1: newpos = file->f_pos + offset; break; 	// SEEK_CUR
		case 2: newpos = dram_size + offset; break; 	// SEEK_END
		}

	if (( newpos < 0 )||( newpos > dram_size )) return -EINVAL;
	file->f_pos = newpos;

	return	newpos;
}

static int pa_to_va(struct page *page, struct vm_area_struct *vma, unsigned long va, void *arg){
	pa_to_process *ptp = arg;
	va_process *vp = &(ptp->process);
	struct task_struct *task;
	struct mm_struct *mm = vma->vm_mm;
	if(!mm){
		printk("mm not exist!\n");
		return SWAP_FAIL;
	}
	task = mm->owner;
	if(!task){
		printk("task not exist!\n");
		return SWAP_FAIL;
	}
	if(vp->pid_c >= MAX_PROCESS){
		printk("space not enough!\n");
		return SWAP_FAIL;
	}
	vp->va[vp->pid_c] = va;
	vp->pid[vp->pid_c] = task->pid;
	vp->pid_c ++;
	//printk("va = 0x%lx, pid = %d\n", va, task->pid);
	if(vp->pid_c >= MAX_PROCESS){
		return SWAP_SUCCESS;
	}else{
		return SWAP_AGAIN;
	}
}

long self_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    int ret = 0, i;
	struct page *pp;
	pa_to_process res;
	res.process.pid_c = 0;
    if(_IOC_TYPE(cmd) != DEV_IOC_MAGIC)
    {
        printk("wrong magic! %c\n", _IOC_TYPE(cmd));
        return -EINVAL;
    }
    if(_IOC_NR(cmd) > DEV_NR_MAX)
    {
        printk("wrong NR!\n");
        return -EINVAL;
    }

    if(_IOC_DIR(cmd) & _IOC_WRITE)
    {
        if(!access_ok(VERIFY_WRITE, (void *)arg, _IOC_SIZE(cmd)))
        {
            printk("arg NOT WRITE!\n");
            return -EINVAL;
        }
    }
    if(_IOC_DIR(cmd) & _IOC_READ)
    {
        if(!access_ok(VERIFY_READ, (void *)arg, _IOC_SIZE(cmd)))
        {
            printk("arg NOT READ!\n");
            return -EINVAL;
        }
    }

    switch(cmd)
    {
    case DEV_GET_SYSMEM:
        __put_user(dram_size, (unsigned long *)arg);
        break;
		
    case DEV_GET_RMAP:
        __get_user(res.pa, (unsigned long *)arg);
#if 1
		pp = pfn_to_page(res.pa / PAGE_SIZE);
#else
		pp = &mem_map[res.pa / PAGE_SIZE];
#endif
		if(SWAP_FAIL == rmap_walk_dram(pp, pa_to_va, (void *)&res)){
			printk("rmap_walk_dram fail!\n");
			break;
		}
		res.pa = res.pa - res.pa % PAGE_SIZE;
		if (unlikely(PageKsm(pp))){
			res.pa |= PAGE_MAPPING_KSM;
		}else if (PageAnon(pp)){
			res.pa |= PAGE_MAPPING_ANON;
		}else{
			res.pa |= PAGE_MAPPING_FILE;
		}
		ret = copy_to_user((char *)arg, &res, sizeof(pa_to_process));
        break;
		
	default:
		return -EINVAL;
	}
	return ret;
}

MODULE_LICENSE("GPL");
module_init( dram_init );
module_exit( dram_exit );


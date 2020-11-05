#include <arch/page.h>
#include <arch/phymem.h>
#include <arch/mempool.h>
#include <arch/registers.h>
#include <arch/tss.h>
#include <arch/memory.h>
#include <xbook/debug.h>
#include <math.h>
#include <string.h>
#include <assert.h>
#include <xbook/schedule.h>
#include <xbook/memspace.h>

void page_link_addr(unsigned long va, unsigned long pa, unsigned long prot)
{
    unsigned long vaddr = (unsigned long )va;
    unsigned long paddr = (unsigned long )pa;
    pde_t *pde = vir_addr_to_dir_entry(vaddr);
	pte_t *pte = vir_addr_to_table_entry(vaddr);

	if (*pde & PAGE_ATTR_PRESENT) {
        ASSERT(!(*pte & PAGE_ATTR_PRESENT)); 
        if (!(*pte & PAGE_ATTR_PRESENT)) {
            *pte = (paddr | prot | PAGE_ATTR_PRESENT);
        } else {
            panic("%s: pte %x has exist!\n", __func__, *pte);
            *pte = (paddr | prot | PAGE_ATTR_PRESENT);
        }
	} else {
        unsigned long page_table = page_alloc_normal(1);
        if (!page_table) {
            panic("%s: kernel no page left!\n", __func__);
        }
        *pde = (page_table | prot | PAGE_ATTR_PRESENT);
        memset((void *)((unsigned long)pte & PAGE_MASK), 0, PAGE_SIZE);
        
        ASSERT(!(*pte & PAGE_ATTR_PRESENT));
        *pte = (paddr | prot | PAGE_ATTR_PRESENT);
    }
}

void page_link_addr_unsafe(unsigned long va, unsigned long pa, unsigned long prot)
{
    unsigned long vaddr = (unsigned long )va;
    unsigned long paddr = (unsigned long )pa;
    pde_t *pde = vir_addr_to_dir_entry(vaddr);
	pte_t *pte = vir_addr_to_table_entry(vaddr);

    if (*pde & PAGE_ATTR_PRESENT) {
    
        if ((*pte & PAGE_ATTR_PRESENT)) {
            page_free_one(*pte & PAGE_MASK);
            *pte = 0;
        }

        if (!(*pte & PAGE_ATTR_PRESENT)) {
            *pte = (paddr | prot | PAGE_ATTR_PRESENT);
        } else {
            *pte = (paddr | prot | PAGE_ATTR_PRESENT);
        }
	} else {
        unsigned long page_table = page_alloc_normal(1);
        if (!page_table) {
            panic("%s: kernel no page left!\n", __func__);
        }
        
        *pde = (page_table | prot | PAGE_ATTR_PRESENT);
        memset((void *)((unsigned long)pte & PAGE_MASK), 0, PAGE_SIZE);

        if ((*pte & PAGE_ATTR_PRESENT)) {
            page_free_one(*pte & PAGE_MASK);
            *pte = 0;
        }
        *pte = (paddr | prot | PAGE_ATTR_PRESENT);
    }
}

void page_unlink_addr(unsigned long vaddr)
{
	pte_t *pte = vir_addr_to_table_entry(vaddr);
	if (*pte & PAGE_ATTR_PRESENT) {
		*pte &= ~PAGE_ATTR_PRESENT;
        tlb_flush_one(vaddr);        
    }
}

int page_map_addr(unsigned long start, unsigned long len, unsigned long prot)
{
    unsigned long first = start & PAGE_MASK;
    len = PAGE_ALIGN(len);
    unsigned int attr = 0;    
    if (prot & PROT_USER)
        attr |= PAGE_ATTR_USER;
    else
        attr |= PAGE_ATTR_SYSTEM;
    
    if (prot & PROT_WRITE)
        attr |= PAGE_ATTR_WRITE;
    else
        attr |= PAGE_ATTR_READ;

	unsigned long pages = len / PAGE_SIZE;
    while (pages > 0) { 
        uint32_t trunk;
        if ((pages / MEM_SECTION_MAX_SIZE) > 0)
            trunk = MEM_SECTION_MAX_SIZE;
        else
            trunk = pages;

        unsigned long page_addr = page_alloc_normal(trunk);
        if (!page_addr) {
            printk(KERN_ERR "%s: map no free pages for %d count!\n", __func__, len / PAGE_SIZE);
            return -1;
        }
        
        pages -= trunk;
        while (trunk > 0) {
            --trunk;
            page_link_addr(first, page_addr, attr);
            first += PAGE_SIZE;
            page_addr += PAGE_SIZE;
        }
    }
	return 0;
}

int page_map_addr_fixed(unsigned long start, unsigned long addr, unsigned long len, unsigned long prot)
{
    unsigned long first = start & PAGE_MASK;
    len = PAGE_ALIGN(len);

    unsigned long attr = 0;
    
    if (prot & PROT_USER)
        attr |= PAGE_ATTR_USER;
    else
        attr |= PAGE_ATTR_SYSTEM;
    
    if (prot & PROT_WRITE)
        attr |= PAGE_ATTR_WRITE;
    else
        attr |= PAGE_ATTR_READ;

	unsigned long pages = addr;
    unsigned long end = first + len;
    while (first < end) {
        if (prot & PROT_REMAP) {
            page_link_addr_unsafe(first, pages, attr);
        } else {
            page_link_addr(first, pages, attr);
        }
        first += PAGE_SIZE;
        pages += PAGE_SIZE;
	}
	return 0;
}

unsigned long addr_vir2phy(unsigned long vaddr)
{
	pte_t* pte = vir_addr_to_table_entry(vaddr);
	return ((*pte & 0xfffff000) + (vaddr & 0x00000fff));
}

int page_unmap_addr(unsigned long vaddr, unsigned long len)
{
	if (!len)
		return -1;
	len = PAGE_ALIGN(len);
	unsigned long paddr, start = (unsigned long)vaddr & PAGE_MASK;
    unsigned long end = start + len;
	
	paddr = addr_vir2phy(start);
	while (start < end)	{
		page_unlink_addr(start);
		start += PAGE_SIZE;
	}
	page_free(paddr);
	return 0;
}

int page_map_addr_safe(unsigned long start, unsigned long len, unsigned long prot)
{
    unsigned long vaddr = (unsigned long )start & PAGE_MASK;
    len = PAGE_ALIGN(len);
    unsigned long pages = DIV_ROUND_UP(len, PAGE_SIZE);
    unsigned long page_idx = 0;
    unsigned long page_addr;
    unsigned long attr = 0;
    
    if (prot & PROT_USER)
        attr |= PAGE_ATTR_USER;
    else
        attr |= PAGE_ATTR_SYSTEM;
    
    if (prot & PROT_WRITE)
        attr |= PAGE_ATTR_WRITE;
    else
        attr |= PAGE_ATTR_WRITE;

    while (page_idx < pages) {
        pde_t *pde = vir_addr_to_dir_entry(vaddr);
        pte_t *pte = vir_addr_to_table_entry(vaddr);

        if (!(*pde & PAGE_ATTR_PRESENT) || !(*pte & PAGE_ATTR_PRESENT)) {
            page_addr = page_alloc_normal(1);
            if (!page_addr) {
                printk("error: user_map_vaddr -> map pages failed!\n");
                return -1;
            }
            
            page_link_addr(vaddr, page_addr, attr);
        }
        vaddr += PAGE_SIZE;
        page_idx++;
    }
    return 0;
}

static int is_page_table_empty(pte_t *page_table)
{
    int i;
    for (i = 0; i < PAGE_TABLE_ENTRY_NR; i++) {
        if (page_table[i] & PAGE_ATTR_PRESENT)
            return 0;   // not empty
    }
    return 1;   // empty
}

int page_unmap_addr_safe(unsigned long start, unsigned long len, char fixed)
{
    unsigned long vaddr = (unsigned long )start & PAGE_MASK;
    len = PAGE_ALIGN(len);
    unsigned long pages = DIV_ROUND_UP(len, PAGE_SIZE);
    unsigned long pte_idx;       /* pte -> physic page */
    pde_t *pde;
    pte_t *pte;

    while (pages > 0) {
        pde = vir_addr_to_dir_entry(vaddr);
        if ((*pde & PAGE_ATTR_PRESENT)) {
            while ((pte_idx = PAGE_TABLE_ENTRY_IDX(vaddr)) < PAGE_TABLE_ENTRY_NR) {
                pte = vir_addr_to_table_entry(vaddr);
                if (*pte & PAGE_ATTR_PRESENT) {
                    /* free physic page if not fixed */
                    if (!fixed)
                        page_free(*pte & PAGE_MASK);
                    *pte &= ~PAGE_ATTR_PRESENT;
                }
                vaddr += PAGE_SIZE;
                pages--;
                if (!pages) {
                    if (is_page_table_empty((pte_t *)((unsigned long)pte & PAGE_MASK))) {
                        page_free(*pde & PAGE_MASK);            
                    }
                    goto end_unmap;
                }
                if (pte_idx == PAGE_TABLE_ENTRY_NR - 1)
                    break;
            }
            page_free(*pde & PAGE_MASK);
            *pde &= ~PAGE_ATTR_PRESENT;
        } else {
            vaddr += PAGE_SIZE;
            --pages;
            if (!pages) {
                goto end_unmap;
            }
        }
    }
end_unmap:
    return 0;
}

unsigned long *kern_page_dir_copy_to()
{
    unsigned long page = page_alloc_normal(1);
    unsigned int *vaddr = (unsigned int *)kern_phy_addr2vir_addr(page);
    
    memset(vaddr, 0, PAGE_SIZE);
    memcpy((void *)((unsigned char *)vaddr + PAGE_SIZE / 2), 
        (void *)((unsigned char *)KERN_PAGE_DIR_VIR_ADDR + PAGE_SIZE / 2), 
        PAGE_SIZE / 2);

    vaddr[PAGE_TABLE_ENTRY_NR - 1] = page | KERN_PAGE_ATTR;
    return (unsigned long *)vaddr;
}

void kern_page_map_early(unsigned int start, unsigned int end)
{
	unsigned int *pdt = (unsigned int *)KERN_PAGE_DIR_VIR_ADDR;
	unsigned int pde_nr = (end - start) / (PAGE_TABLE_ENTRY_NR * PAGE_SIZE);
	unsigned int pte_nr = (end-start)/PAGE_SIZE;

	pte_nr = pte_nr % PAGE_TABLE_ENTRY_NR;
	
	unsigned int *pte_addr = (unsigned int *) (KERN_PAGE_TABLE_PHY_ADDR + 
			PAGE_SIZE * PAGE_TABLE_HAD_USED);

    uint32_t pde_off = KERN_PAGE_DIR_ENTRY_OFF + PAGE_TABLE_HAD_USED;

	int i, j;
	for (i = 0; i < pde_nr; i++) {
		pdt[pde_off + i] = (unsigned int)pte_addr | KERN_PAGE_ATTR;	
		for (j = 0; j < PAGE_TABLE_ENTRY_NR; j++) {
			pte_addr[j] = start | KERN_PAGE_ATTR;
			start += PAGE_SIZE;
		}
		pte_addr += PAGE_SIZE;
	}
	if (pte_nr > 0) {
		pdt[pde_off + i] = (unsigned int)pte_addr | KERN_PAGE_ATTR;
		for (j = 0; j < pte_nr; j++) {
			pte_addr[j] = start | KERN_PAGE_ATTR;
			start += PAGE_SIZE;
		}
	}

	/* 在开启分页模式之后，我们的内核就运行在高端内存，
	那么，现在我们不能通过低端内存访问内核，所以，我们在loader
	中初始化的0~8MB低端内存的映射要取消掉才行。
	我们把用户内存空间的页目录项都清空 */ 	
	for (i = 0; i < KERN_PAGE_DIR_ENTRY_OFF; i++) {
		pdt[i] = 0;
	}
}

static int do_handle_no_page(unsigned long addr, unsigned long prot)
{
    /* 映射一个物理页 */
	return page_map_addr(addr, PAGE_SIZE, prot);
}

/**
 * do_page_no_write - 让pte有写属性
 * @addr: 要设置的虚拟地址
 */
static int do_page_no_write(unsigned long addr)
{
	if (addr > USER_VMM_SIZE)
		return -1;

	pde_t *pde = vir_addr_to_dir_entry(addr);
	pte_t *pte = vir_addr_to_table_entry(addr);
	
	if (!(*pde & PAGE_ATTR_PRESENT))
		return -1;
	if (!(*pte & PAGE_ATTR_PRESENT))
		return -1;
	*pte |= PAGE_ATTR_WRITE;
	return 0;
}

static inline void do_vir_mem_fault(unsigned long addr)
{
    /* TODO: 如果是在vir_mem区域中，就进行页复制，不是的话，就发出段信号。 */
    panic("do_vir_mem_fault: at %x not support now!\n", addr);
}

static void do_expand_stack(mem_space_t *space, unsigned long addr)
{
    addr &= PAGE_MASK;
	space->start = addr;
}

static int do_protection_fault(mem_space_t *space, unsigned long addr, int write)
{
	/* 没有写标志，说明该段内存不支持内存写入，就直接返回吧 */
	if (write) {
		printk(KERN_DEBUG "page: %s: addr %x have write protection.\n", __func__, addr);
		int ret = do_page_no_write(addr);
		if (ret) {
            printk(KERN_EMERG "page: %s: touch TRIGSYS trigger because page not writable!", __func__);    
            trigger_force(TRIGSYS, task_current->pid);    
            return -1;
        }

		/* 虽然写入的写标志，但是还是会出现缺页故障，在此则处理一下缺页 */
		if (do_handle_no_page(addr, space->page_prot)) {
            printk(KERN_EMERG "page: %s: touch TRIGSYS trigger because hand no page failed!", __func__);
            trigger_force(TRIGSYS, task_current->pid);
			return -1; 
        }
		return 0;
	} else {
		printk(KERN_DEBUG "page: %s: no write protection\n", __func__);
	}
    printk(KERN_EMERG "page: %s: touch TRIGSYS trigger because page protection!", __func__);
    trigger_force(TRIGSYS, task_current->pid);
    return -1;
}

/**
 * page_do_fault - 处理页故障
 * 
 * 错误码的内容 
 * bit 0: 0 no page found, 1 protection fault
 * bit 1: 0 read, 1 write
 * bit 2: 0 kernel, 1 user
 * 
 * 如果是来自内核的页故障，就会打印信息并停机。
 * 如果是来自用户的页故障，就会根据地址来做处理。
 */
int page_do_fault(trap_frame_t *frame)
{
    task_t *cur = task_current;
    unsigned long addr = 0x00;
    addr = cpu_cr2_read(); /* cr2 saved the fault addr */

    /* in kernel page fault */
    if (!(frame->error_code & PAGE_ERR_USER) && addr >= KERN_BASE_VIR_ADDR) {
        printk("task name=%s pid=%d\n", cur->name, cur->pid);
        printk(KERN_EMERG "a memory problem had occured in kernel, please check your code! :(\n");
        printk(KERN_EMERG "page fault at %x.\n", addr);
        trap_frame_dump(frame);
        panic("halt...");
    }
    /* 如果故障地址位于内核中， */
    if (addr >= USER_VMM_SIZE) {
        /* TODO: 故障源是用户，说明用户需要访问非连续内存区域，于是复制一份给用户即可 */
        printk(KERN_DEBUG "user pid=%d name=%s access unmaped vir_mem area .\n", cur->pid, cur->name);
        trap_frame_dump(frame);
        tasks_print();
        do_vir_mem_fault(addr);
        return -1;
    }
    /* 故障地址在用户空间 */
    mem_space_t *space = mem_space_find(cur->vmm, addr);
    if (space == NULL) {    
        printk(KERN_EMERG "page_do_fault: user access user unknown space .\n");
        printk(KERN_EMERG "page fault addr:%x\n", addr);
        mem_space_dump(cur->vmm);
        tasks_print();
        trigger_force(TRIGSYS, cur->pid);
        return -1;
    }
    if (space->start > addr) { /* 故障地址在空间前，说明是栈向下拓展，那么尝试拓展栈。 */
        if (frame->error_code & PAGE_ERR_USER) {
            /* 可拓展栈：有栈标志，在可拓展限定内， */
            if ((space->flags & MEM_SPACE_MAP_STACK) &&
                ((space->end - space->start) < MAX_MEM_SPACE_STACK_SIZE) &&
                (addr + 32 >= frame->esp)) {
                do_expand_stack(space, addr);
            } else {
                printk("task name=%s pid=%d\n", cur->name, cur->pid);
                printk(KERN_EMERG "page_do_fault: touch TRIGSYS trigger because unknown space!\n");
                printk(KERN_EMERG "page fault addr:%x\n", addr);
                trap_frame_dump(frame);
        
                trigger_force(TRIGSYS, cur->pid);
                return -1;  
            }    
        }
    }
    /* 故障地址在空间里面，情况如下：
    1.读写保护故障（R/W）
    2.缺少物理页和虚拟地址的映射。（堆的向上拓展或者栈的向下拓展）
     */
    if (frame->error_code & PAGE_ERR_PROTECT) {
        return do_protection_fault(space, addr, frame->error_code & PAGE_ERR_WRITE);
    }
    do_handle_no_page(addr, space->page_prot);
    return 0;
}
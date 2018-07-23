/*
 * Realtek Semiconductor Corp.
 *
 * cache-rlx.c: RLX specific mmu/cache code.
 *
 * Tony Wu (tonywu@realtek.com)
 * Dec. 07, 2008
 */
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/smp.h>
#include <linux/mm.h>

#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/mmu_context.h>
#include <asm/isadep.h>
#include <asm/io.h>
#include <asm/bootinfo.h>
#include <asm/cpu.h>
#include <asm/cpu-features.h>


/*
 *  CACHE OP
 *   0x10 = IInval
 *   0x11 = DInval
 *   0x15 = DWBInval
 *   0x19 = DWB
 *   0x1b = DWB_IInval
 */

#define DCACHE_INV	0x11
#define DCACHE_WBINV	0x15
#define DCACHE_WB	0x19
#define ICACHE_INV	0x10

#define CACHE_OP(op, p)			\
	__asm__ __volatile__ (		\
		".set  push\n"		\
		".set  noreorder\n"	\
		"cache %0, 0x000(%1)\n"	\
		".set  pop\n"		\
		: : "i" (op), "r" (p)	\
	)

#define CACHE_UNROLL(op, p)		\
	__asm__ __volatile__ (		\
		".set  push\n"		\
		".set  noreorder\n"	\
		"cache %0, 0x000(%1)\n"	\
		"cache %0, 0x020(%1)\n"	\
		"cache %0, 0x040(%1)\n"	\
		"cache %0, 0x060(%1)\n"	\
		"cache %0, 0x080(%1)\n"	\
		"cache %0, 0x0a0(%1)\n"	\
		"cache %0, 0x0c0(%1)\n"	\
		"cache %0, 0x0e0(%1)\n"	\
		".set  pop\n"		\
		: : "i" (op), "r" (p)	\
	)

/*
 *  CCTL OP
 *   0x1   = DInval
 *   0x2   = IInval
 *   0x100 = DWB
 *   0x200 = DWB_Inval
 */
#define ICCTL_INV		0x2
 #define DCCTL_INV		0x001
 #define DCCTL_WB		0x100
 #define DCCTL_WBINV		0x200

#define CCTL_OP(op)			\
	__asm__ __volatile__(		\
		".set  push\n"		\
		".set  noreorder\n"	\
		"mtc0	$0, $20\n"	\
		"li	$8, %0\n"	\
		"mtc0	$8, $20\n"	\
		".set  pop\n"		\
		: : "i" (op)		\
	)

static inline int has_valid_asid(const struct mm_struct *mm)
{
	return cpu_context(smp_processor_id(), mm);
}

/*
 * Dummy cache handling routines for machines without boardcaches
 */
static void cache_noop(void) {}

static inline void blast_dcache_loop(unsigned long start, unsigned long end,
				     unsigned int cache_op)
{
	unsigned long p;

	for (p = start; p <= end-0x100; p += 0x100) {
		CACHE_UNROLL(cache_op, p);
	}

	p = p & ~(cpu_dcache_line -1);
	while (p < end) {
		CACHE_OP(cache_op, p);
		p += cpu_dcache_line;
	}
}

static inline void blast_icache_loop(unsigned long start, unsigned long end,
				     unsigned int cache_op)
{
	unsigned long p;

	for (p = start; p <= end-0x100; p += 0x100) {
		CACHE_UNROLL(cache_op, p);
	}

	p = p & ~(cpu_icache_line -1);
	while (p < end) {
		CACHE_OP(cache_op, p);
		p += cpu_icache_line;
	}
}

/*
 * DCACHE part
 *
 * flush_dcache_range:
 * flush_data_cache_page: flush a data cache page
 */
static inline void rlx_flush_dcache_range(unsigned long start, unsigned long end,
					  unsigned int cctl_op, unsigned int cache_op)
{
	if (end - start > cpu_dcache_size) {
		CCTL_OP(cctl_op);
		return;
	}
	blast_dcache_loop(start, end, cache_op);
}

static inline void local_rlx_flush_data_cache_page(void *addr)
{
	unsigned long start = (unsigned long)addr;
	blast_dcache_loop(start, start + PAGE_SIZE, DCACHE_WBINV);
}

static void rlx_flush_data_cache_page(unsigned long addr)
{
	if (in_atomic())
		local_rlx_flush_data_cache_page((void *)addr);
	else {
		preempt_disable();
		local_rlx_flush_data_cache_page((void *)addr);
		preempt_enable();
	}
}

/*
 * ICACHE part
 *
 * flush_icache_range: flush a range of instructions on all CPUs
 * local_flush_icache_range: flush a range of instructions on this CPU
 */
static void rlx_flush_icache_range(unsigned long start, unsigned long end)
{
	unsigned long size;

	size = end - start;
	if (size > cpu_icache_size) {
		CCTL_OP(DCCTL_WBINV);
		CCTL_OP(ICCTL_INV);
		return;
	}

	/*
	 * If we get here, it means we cannot do combo_op.
	 *
	 * Due to:
	 * 1. combo_op = n
	 * 2. combo_op = y, but in one of the following cases
	 *    cpu_icache_size < size < cpu_dcache_size
	 *    cpu_dcache_size < size < cpu_icache_size
	 */
	rlx_flush_dcache_range(start, end, DCCTL_WBINV, DCACHE_WBINV);
	if (size > cpu_icache_size)
		CCTL_OP(ICCTL_INV);
	else
		blast_icache_loop(start, end, ICACHE_INV);
}

/*
 * CACHE part
 *
 * flush_cache_all:
 * __flush_cache_all: flush all caches
 * flush_cache_sigtramp: flush signal trampoline
 */
static inline void rlx___flush_cache_all(void)
{
	CCTL_OP(DCCTL_WBINV);
	CCTL_OP(ICCTL_INV);
}

static inline void rlx_flush_cache_page(struct vm_area_struct *vma,
					unsigned long addr, unsigned long pfn)
{
	unsigned long kaddr = KSEG0ADDR(pfn << PAGE_SHIFT);
	int exec = vma->vm_flags & VM_EXEC;
	struct mm_struct *mm = vma->vm_mm;
	pgd_t *pgdp;
	pud_t *pudp;
	pmd_t *pmdp;
	pte_t *ptep;

	pr_debug("cpage[%08lx,%08lx]\n",
		 cpu_context(smp_processor_id(), mm), addr);

	/* No ASID => no such page in the cache.  */
	if (cpu_context(smp_processor_id(), mm) == 0)
		return;

	pgdp = pgd_offset(mm, addr);
	pudp = pud_offset(pgdp, addr);
	pmdp = pmd_offset(pudp, addr);
	ptep = pte_offset(pmdp, addr);

	/* Invalid => no such page in the cache.  */
	if (!(pte_val(*ptep) & _PAGE_PRESENT))
		return;

	blast_dcache_loop(kaddr, kaddr + PAGE_SIZE, DCACHE_WBINV);
	if (exec) {
		blast_icache_loop(kaddr, kaddr + PAGE_SIZE, ICACHE_INV);
	}
}

static void rlx_flush_cache_sigtramp(unsigned long addr)
{
	pr_debug("csigtramp[%08lx]\n", addr);

	CACHE_OP(DCACHE_WBINV, addr);
	CACHE_OP(ICACHE_INV, addr);
}

static void rlx_dma_cache_wback_inv(unsigned long start, unsigned long size)
{
	/* Catch bad driver code */
	BUG_ON(size == 0);

	iob();
	rlx_flush_dcache_range(start, start + size, DCCTL_WBINV, DCACHE_WBINV);
}

static void rlx_dma_cache_wback(unsigned long start, unsigned long size)
{
	/* Catch bad driver code */
	BUG_ON(size == 0);

	iob();
	rlx_flush_dcache_range(start, start + size, DCCTL_WB, DCACHE_WB);
}

static void rlx_dma_cache_inv(unsigned long start, unsigned long size)
{
	/* Catch bad driver code */
	BUG_ON(size == 0);

	iob();
	blast_dcache_loop(start, start + size, DCACHE_INV);
}

static void rlx_flush_kernel_vmap_range(unsigned long vaddr, int size)
{
	BUG();
}

static void coherency_setup(void)
{
	unsigned int cca;

	cca = _CACHE_CACHABLE_NONCOHERENT;

	pr_debug("Using cache attribute %d\n", cca);

	_page_cachable_default = cca;
}

static void probe_pcache(void)
{
	struct cpuinfo_mips *c = &current_cpu_data;

	c->icache.linesz = cpu_icache_line;
	c->dcache.linesz = cpu_dcache_line;
	c->scache.linesz = cpu_scache_line;
	c->tcache.linesz = 0;
}

void rlx_cache_init(void)
{
	extern void build_clear_page(void);
	extern void build_copy_page(void);

	probe_pcache();

	flush_cache_all = (void *) cache_noop;
	__flush_cache_all = rlx___flush_cache_all;
	flush_cache_mm = (void *) cache_noop;
	flush_cache_range = (void *) cache_noop;
	flush_cache_page = rlx_flush_cache_page;
	flush_icache_range = rlx_flush_icache_range;
	local_flush_icache_range = rlx_flush_icache_range;

	__flush_kernel_vmap_range = rlx_flush_kernel_vmap_range;

	/* MIPS specific */
	flush_cache_sigtramp = rlx_flush_cache_sigtramp;
	flush_data_cache_page = rlx_flush_data_cache_page;
	local_flush_data_cache_page = local_rlx_flush_data_cache_page;

	_dma_cache_wback_inv = rlx_dma_cache_wback_inv;
	_dma_cache_wback = rlx_dma_cache_wback;
	_dma_cache_inv = rlx_dma_cache_inv;

	printk("icache: %dkB/%dB, dcache: %dkB/%dB, scache: %dkB/%dB\n",
		cpu_icache_size >> 10, cpu_icache_line,
		cpu_dcache_size >> 10, cpu_dcache_line,
		cpu_scache_size >> 10, cpu_scache_line);

	build_clear_page();
	build_copy_page();
	rlx___flush_cache_all();

	coherency_setup();
}

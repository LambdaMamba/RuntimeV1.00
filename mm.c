#include "rt_util.h"
#include "common.h"
#include "syscall.h"
#include "mm.h"
#include "freemem.h"
#include "paging.h"

#ifdef USE_FREEMEM

/* Page table utilities */
static pte*
__walk_create(pte* root, uintptr_t addr);

static pte*
__walk_create_nvm(pte* root, uintptr_t addr);

/* Hacky storage of current u-mode break */
static uintptr_t current_program_break;

uintptr_t get_program_break(){
  return current_program_break;
}

void set_program_break(uintptr_t new_break){
  current_program_break = new_break;
}

static pte*
__continue_walk_create(pte* root, uintptr_t addr, pte* pte)
{
  uintptr_t new_page = spa_get_zero();
  assert(new_page);

  unsigned long free_ppn = ppn(__pa(new_page));
  *pte = ptd_create(free_ppn);
  return __walk_create(root, addr);
}

static pte*
__continue_walk_create_nvm(pte* root, uintptr_t addr, pte* pte)
{
  //printf("__continue_walk_create_nvm\n");
  uintptr_t new_page = spa_get_zero_nvm();
  assert(new_page);

  unsigned long free_ppn = ppn(__pa_nvm(new_page));
  //printf("free_ppn is 0x%lx\n", free_ppn);
  *pte = ptd_create(free_ppn);
  //printf("ptd_created\n");
  return __walk_create_nvm(root, addr);
}

static pte*
__walk_internal(pte* root, uintptr_t addr, int create)
{
  pte* t = root;
  int i;
  for (i = 1; i < RISCV_PT_LEVELS; i++)
  {
    size_t idx = RISCV_GET_PT_INDEX(addr, i);
    ////printf("DRAM page table index is %ld on level %d with addr 0x%lx\n", idx,i, addr);

    if (!(t[idx] & PTE_V))
      return create ? __continue_walk_create(root, addr, &t[idx]) : 0;

    t = (pte*) __va(pte_ppn(t[idx]) << RISCV_PAGE_BITS);
    ////printf("dram pte* is 0x%lx\n", t);
  }

  return &t[RISCV_GET_PT_INDEX(addr, 3)];
}

static pte*
__walk_internal_nvm(pte* root, uintptr_t addr, int create)
{
  pte* t = root;
  int i;
  for (i = 1; i < RISCV_PT_LEVELS; i++)
  {
    size_t idx = RISCV_GET_PT_INDEX(addr, i);
    ////printf("NVM page table index is %ld on level %d with addr 0x%lx\n", idx,i, addr);
    if (!(t[idx] & PTE_V))
      return create ? __continue_walk_create_nvm(root, addr, &t[idx]) : 0;

    t = (pte*) __va_nvm(pte_ppn(t[idx]) << RISCV_PAGE_BITS);
    ////printf("nvm pte* is 0x%lx\n", t);
  }

  return &t[RISCV_GET_PT_INDEX(addr, 3)];
}

/* walk the page table and return PTE
 * return 0 if no mapping exists */
static pte*
__walk(pte* root, uintptr_t addr)
{
  return __walk_internal(root, addr, 0);
}


static pte*
__walk_nvm(pte* root, uintptr_t addr)
{
  return __walk_internal_nvm(root, addr, 0);
}

/* walk the page table and return PTE
 * create the mapping if non exists */
static pte*
__walk_create(pte* root, uintptr_t addr)
{
  return __walk_internal(root, addr, 1);
}


static pte*
__walk_create_nvm(pte* root, uintptr_t addr)
{
  return __walk_internal_nvm(root, addr, 1);
}


/* allocate a new page to a given vpn
 * returns VA of the page, (returns 0 if fails) */
uintptr_t
alloc_page(uintptr_t vpn, int flags)
{
  uintptr_t page;
  pte* pte = __walk_create(root_page_table, vpn << RISCV_PAGE_BITS);

  assert(flags & PTE_U);

  if (!pte)
    return 0;

	/* if the page has been already allocated, return the page */
  if(*pte & PTE_V) {
    return __va(*pte << RISCV_PAGE_BITS);
  }

	/* otherwise, allocate one from the freemem */
  page = spa_get();
  assert(page);

  *pte = pte_create(ppn(__pa(page)), flags | PTE_V);
  //printf("alloc page dram new pte 0x%lx, ppn is 0x%lx\n", (uintptr_t)pte, (uintptr_t)ppn(__pa(page)) );
#ifdef USE_PAGING
  paging_inc_user_page();
#endif
  //printf("alloc page dram 0x%lx\n", page);
  return page;
}


/* allocate a new page to a given vpn
 * returns VA of the page, (returns 0 if fails) */
uintptr_t
alloc_page_nvm(uintptr_t vpn, int flags)
{
  uintptr_t page;
  pte* pte = __walk_create_nvm(root_page_table, vpn << RISCV_PAGE_BITS);

  assert(flags & PTE_U);

  if (!pte)
    return 0;

	/* if the page has been already allocated, return the page */
  if(*pte & PTE_V) {
    return __va_nvm(*pte << RISCV_PAGE_BITS);
  }

	/* otherwise, allocate one from the freemem */
  page = spa_get_nvm();
  assert(page);

  *pte = pte_create(ppn(__pa_nvm(page)), flags | PTE_V);
  //printf("alloc page nvm new pte 0x%lx, ppn is 0x%lx\n", (uintptr_t)pte, (uintptr_t)ppn(__pa_nvm(page)) );
#ifdef USE_PAGING
  paging_inc_user_page();
#endif
  //printf("alloc page nvm 0x%lx\n", page);

  return page;
}

void
free_page(uintptr_t vpn){

  pte* pte = __walk(root_page_table, vpn << RISCV_PAGE_BITS);

  // No such PTE, or invalid
  if(!pte || !(*pte & PTE_V))
    return;

  assert(*pte & PTE_U);

  uintptr_t ppn = pte_ppn(*pte);
  // Mark invalid
  // TODO maybe do more here
  *pte = 0;

#ifdef USE_PAGING
  paging_dec_user_page();
#endif
  // Return phys page
  spa_put(__va(ppn << RISCV_PAGE_BITS));

  return;

}



void
free_page_nvm(uintptr_t vpn){

  pte* pte = __walk_nvm(root_page_table, vpn << RISCV_PAGE_BITS);

  // No such PTE, or invalid
  if(!pte || !(*pte & PTE_V))
    return;

  assert(*pte & PTE_U);

  uintptr_t ppn = pte_ppn(*pte);
  // Mark invalid
  // TODO maybe do more here
  *pte = 0;

#ifdef USE_PAGING
  paging_dec_user_page();
#endif
  // Return phys page
  spa_put_nvm(__va_nvm(ppn << RISCV_PAGE_BITS));

  return;

}

/* allocate n new pages from a given vpn
 * returns the number of pages allocated */
size_t
alloc_pages(uintptr_t vpn, size_t count, int flags)
{
  unsigned int i;
  for (i = 0; i < count; i++) {
    if(!alloc_page(vpn + i, flags))
      break;
  }

  return i;
}


/* allocate n new pages from a given vpn
 * returns the number of pages allocated */
size_t
alloc_pages_nvm(uintptr_t vpn, size_t count, int flags)
{
  unsigned int i;
  for (i = 0; i < count; i++) {
    if(!alloc_page_nvm(vpn + i, flags))
      break;
  }

  return i;
}

void
free_pages(uintptr_t vpn, size_t count){
  unsigned int i;
  for (i = 0; i < count; i++) {
    free_page(vpn + i);
  }

}


void
free_pages_nvm(uintptr_t vpn, size_t count){
  unsigned int i;
  for (i = 0; i < count; i++) {
    free_page_nvm(vpn + i);
  }

}

/*
 * Check if a range of VAs contains any allocated pages, starting with
 * the given VA. Returns the number of sequential pages that meet the
 * conditions.
 */
size_t
test_va_range(uintptr_t vpn, size_t count){

  unsigned int i;
  /* Validate the region */
  for (i = 0; i < count; i++) {
    pte* pte = __walk_internal(root_page_table, (vpn+i) << RISCV_PAGE_BITS, 0);
    //printf("test_va_range dram pte is 0x%lx\n", pte);
    // If the page exists and is valid then we cannot use it
    if(pte && *pte){
      break;
    }
  }
  return i;
}


size_t
test_va_range_nvm(uintptr_t vpn, size_t count){

  unsigned int i;
  /* Validate the region */
  for (i = 0; i < count; i++) {
  
    pte* pte = __walk_internal_nvm(root_page_table, (vpn+i) << RISCV_PAGE_BITS, 0);

    //printf("test_va_range nvm pte is 0x%lx\n", pte);
    // If the page exists and is valid then we cannot use it
    if(pte && *pte){
      break;
    }
  }
  return i;
}

/* get a mapped physical address for a VA */
uintptr_t
translate(uintptr_t va)
{
  pte* pte = __walk(root_page_table, va);

  if(pte && (*pte & PTE_V))
    return (pte_ppn(*pte) << RISCV_PAGE_BITS) | (RISCV_PAGE_OFFSET(va));
  else
    return 0;
}

uintptr_t
translate_nvm(uintptr_t va)
{
  pte* pte = __walk_nvm(root_page_table, va);

  if(pte && (*pte & PTE_V))
    return (pte_ppn(*pte) << RISCV_PAGE_BITS) | (RISCV_PAGE_OFFSET(va));
  else
    return 0;
}

/* try to retrieve PTE for a VA, return 0 if fail */
pte*
pte_of_va(uintptr_t va)
{
  pte* pte = __walk(root_page_table, va);
  return pte;
}

void
__map_with_reserved_page_table_32(uintptr_t dram_base,
                               uintptr_t dram_size,
                               uintptr_t ptr,
                               pte* l2_pt)
{
  uintptr_t offset = 0;
  uintptr_t leaf_level = 2;
  pte* leaf_pt = l2_pt;
  unsigned long dram_max =  RISCV_GET_LVL_PGSIZE(leaf_level - 1);

  /* use megapage if l2_pt is null */
  if (!l2_pt) {
    leaf_level = 1;
    leaf_pt = root_page_table;
    dram_max = -1UL; 
  }

  assert(dram_size <= dram_max);
  assert(IS_ALIGNED(dram_base, RISCV_GET_LVL_PGSIZE_BITS(leaf_level)));
  assert(IS_ALIGNED(ptr, RISCV_GET_LVL_PGSIZE_BITS(leaf_level - 1)));

  if(l2_pt) {
       /* set root page table entry */
       root_page_table[RISCV_GET_PT_INDEX(ptr, 1)] =
       ptd_create(ppn(kernel_va_to_pa(l2_pt)));
  }

  for (offset = 0;
       offset < dram_size;
       offset += RISCV_GET_LVL_PGSIZE(leaf_level))
  {
        leaf_pt[RISCV_GET_PT_INDEX(ptr + offset, leaf_level)] =
        pte_create(ppn(dram_base + offset),
                 PTE_R | PTE_W | PTE_X | PTE_A | PTE_D);
  }

}

void
__map_with_reserved_page_table_64(uintptr_t dram_base,
                               uintptr_t dram_size,
                               uintptr_t ptr,
                               pte* l2_pt,
                               pte* l3_pt)
{
  uintptr_t offset = 0;
  uintptr_t leaf_level = 3;
  pte* leaf_pt = l3_pt;
  /* use megapage if l3_pt is null */
  if (!l3_pt) {
    leaf_level = 2;
    leaf_pt = l2_pt;
  }

  assert(dram_size <= RISCV_GET_LVL_PGSIZE(leaf_level - 1));
  assert(IS_ALIGNED(dram_base, RISCV_GET_LVL_PGSIZE_BITS(leaf_level)));
  assert(IS_ALIGNED(ptr, RISCV_GET_LVL_PGSIZE_BITS(leaf_level - 1)));
  
  uintptr_t rootpte = ptd_create(ppn(kernel_va_to_pa(l2_pt)));
  /* set root page table entry */
  //printf("Setting dram root page table entry, index: 0x%lx, ppn: 0x%lx , created ptd: 0x%lx\n", RISCV_GET_PT_INDEX(ptr, 1), ppn(kernel_va_to_pa(l2_pt)), rootpte);
 // //printf("DRAM RISCV_GET_PT_INDEX(ptr, 1) is %d\n", RISCV_GET_PT_INDEX(ptr, 1));
  root_page_table[RISCV_GET_PT_INDEX(ptr, 1)] = rootpte;

  /* set L2 if it's not leaf */
  if (leaf_pt != l2_pt) {
    uintptr_t l2pte = ptd_create(ppn(kernel_va_to_pa(l3_pt)));
    //printf("Setting dram L2 entry, index: 0x%lx, ppn: 0x%lx, created ptd: 0x%lx\n", RISCV_GET_PT_INDEX(ptr, 2), ppn(kernel_va_to_pa(l3_pt)), l2pte);
    l2_pt[RISCV_GET_PT_INDEX(ptr, 2)] = l2pte;
  }

  /* set leaf level */
  for (offset = 0;
       offset < dram_size;
       offset += RISCV_GET_LVL_PGSIZE(leaf_level))
  {
    uintptr_t leafpte =  pte_create(ppn(dram_base + offset), PTE_R | PTE_W | PTE_X | PTE_A | PTE_D);
    //printf("Setting dram leaf entry, index: 0x%lx, ppn: 0x%lx, created pte: 0x%lx\n", RISCV_GET_PT_INDEX(ptr + offset, leaf_level), ppn(dram_base + offset), leafpte);
    leaf_pt[RISCV_GET_PT_INDEX(ptr + offset, leaf_level)] = leafpte;
  }
}


void
__map_with_reserved_page_table_nvm(uintptr_t nvm_base,
                               uintptr_t nvm_size,
                               uintptr_t ptr,
                               pte* l2_pt_nvm,
                               pte* l3_pt_nvm)
{
  uintptr_t offset = 0;
  uintptr_t leaf_level = 3;
  pte* leaf_pt_nvm = l3_pt_nvm;
  /* use megapage if l3_pt_nvm is null */
  if (!l3_pt_nvm) {
    leaf_level = 2;
    leaf_pt_nvm = l2_pt_nvm;
  }

  assert(nvm_size <= RISCV_GET_LVL_PGSIZE(leaf_level - 1));

  // for(uintptr_t i = 0xffffffff00000000; i < 0xffffffff00f00000; i++){
  //   if(IS_ALIGNED(i, RISCV_GET_LVL_PGSIZE_BITS(leaf_level - 1)) == 1){
  //     //printf("i=0x%lx aligned is %d\n", i, IS_ALIGNED(i, RISCV_GET_LVL_PGSIZE_BITS(leaf_level - 1)));
  //   }
  // }

  
  ////printf("IS_ALIGNED(nvm_base, RISCV_GET_LVL_PGSIZE_BITS(leaf_level)) is %d\n", IS_ALIGNED(nvm_base, RISCV_GET_LVL_PGSIZE_BITS(leaf_level)));
  assert(IS_ALIGNED(nvm_base, RISCV_GET_LVL_PGSIZE_BITS(leaf_level)));

  ////printf("nvm base is aligned\n");
  //printf("ptr is 0x%lx\n", ptr);
  ////printf("IS_ALIGNED(ptr, RISCV_GET_LVL_PGSIZE_BITS(leaf_level - 1)) is %d\n", IS_ALIGNED(ptr, RISCV_GET_LVL_PGSIZE_BITS(leaf_level - 1)));
  assert(IS_ALIGNED(ptr, RISCV_GET_LVL_PGSIZE_BITS(leaf_level - 1)));

  uintptr_t rootpte = ptd_create(ppn(kernel_va_to_pa(l2_pt_nvm)));
  /* set root page table entry */
  //printf("Setting nvm root page table entry, index: 0x%lx, ppn: 0x%lx, created ptd: 0x%lx\n", RISCV_GET_PT_INDEX(ptr, 1), ppn(kernel_va_to_pa(l2_pt_nvm)), rootpte);
  ////printf("NVM RISCV_GET_PT_INDEX(ptr, 1) is %d\n", RISCV_GET_PT_INDEX(ptr, 1));
  root_page_table[RISCV_GET_PT_INDEX(ptr, 1)] = rootpte;

  /* set L2 if it's not leaf */
  if (leaf_pt_nvm != l2_pt_nvm) {
    uintptr_t l2pte = ptd_create(ppn(kernel_va_to_pa(l3_pt_nvm)));
    //printf("Setting nvm L2 entry, index: 0x%lx, ppn: 0x%lx, created ptd: 0x%lx\n", RISCV_GET_PT_INDEX(ptr, 2), ppn(kernel_va_to_pa(l3_pt_nvm)), l2pte);
    l2_pt_nvm[RISCV_GET_PT_INDEX(ptr, 2)] = l2pte;
  }

  /* set leaf level */
  for (offset = 0;
       offset < nvm_size;
       offset += RISCV_GET_LVL_PGSIZE(leaf_level))
  {
    uintptr_t leafpte = pte_create(ppn(nvm_base + offset), PTE_R | PTE_W | PTE_X | PTE_A | PTE_D);
    //printf("Setting nvm leaf entry, index: 0x%lx, ppn: 0x%lx, created pte: 0x%lx\n", RISCV_GET_PT_INDEX(ptr + offset, leaf_level), ppn(nvm_base + offset), leafpte);
    leaf_pt_nvm[RISCV_GET_PT_INDEX(ptr + offset, leaf_level)] = leafpte;
  }

  //printf("Finished nvm leaf entry\n");
}

void
map_with_reserved_page_table(uintptr_t dram_base,
                             uintptr_t dram_size,
                             uintptr_t ptr,
                             pte* l2_pt,
                             pte* l3_pt)
{
  #if __riscv_xlen == 64

  if (dram_size > RISCV_GET_LVL_PGSIZE(2))
        __map_with_reserved_page_table_64(dram_base, dram_size, ptr, l2_pt, 0);
  else
    __map_with_reserved_page_table_64(dram_base, dram_size, ptr, l2_pt, l3_pt);
  #elif __riscv_xlen == 32
  if (dram_size > RISCV_GET_LVL_PGSIZE(1))
    __map_with_reserved_page_table_32(dram_base, dram_size, ptr, 0);
  
  else
    __map_with_reserved_page_table_32(dram_base, dram_size, ptr, l2_pt);
  #endif
}

void
map_with_reserved_page_table_nvm(uintptr_t nvm_base,
                             uintptr_t nvm_size,
                             uintptr_t ptr,
                             pte* l2_pt_nvm,
                             pte* l3_pt_nvm)
{
  if (nvm_size > RISCV_GET_LVL_PGSIZE(2))
    __map_with_reserved_page_table_nvm(nvm_base, nvm_size, ptr, l2_pt_nvm, 0);
  else
    __map_with_reserved_page_table_nvm(nvm_base, nvm_size, ptr, l2_pt_nvm, l3_pt_nvm);
}

#endif /* USE_FREEMEM */
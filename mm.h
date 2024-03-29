#ifndef _MM_H_
#define _MM_H_
//#include <stdint.h>
#include <stddef.h>
//#include "vm.h"

#include <stdint.h>

#include "vm_defs.h"

uintptr_t translate(uintptr_t va);
uintptr_t translate_nvm(uintptr_t va);
pte* pte_of_va(uintptr_t va);
#ifdef USE_FREEMEM
uintptr_t alloc_page(uintptr_t vpn, int flags);
uintptr_t alloc_page_nvm(uintptr_t vpn, int flags);
void free_page(uintptr_t vpn);
void free_page_nvm(uintptr_t vpn);
size_t alloc_pages(uintptr_t vpn, size_t count, int flags);
size_t alloc_pages_nvm(uintptr_t vpn, size_t count, int flags);
void free_pages(uintptr_t vpn, size_t count);
void free_pages_nvm(uintptr_t vpn, size_t count);
size_t test_va_range(uintptr_t vpn, size_t count);
size_t test_va_range_nvm(uintptr_t vpn, size_t count);

uintptr_t get_program_break();
void set_program_break(uintptr_t new_break);

void map_with_reserved_page_table(uintptr_t base, uintptr_t size, uintptr_t ptr, pte* l2_pt, pte* l3_pt);
void map_with_reserved_page_table_nvm(uintptr_t base, uintptr_t size, uintptr_t ptr, pte* l2_pt, pte* l3_pt);
#endif /* USE_FREEMEM */

#endif /* _MM_H_ */
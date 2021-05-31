//******************************************************************************
// Copyright (c) 2018, The Regents of the University of California (Regents).
// All Rights Reserved. See LICENSE for license details.
//------------------------------------------------------------------------------
#include "eapp_utils.h"
#include "string.h"
#include "edge_call.h"
#include <syscall.h>
#include <unistd.h>
#include <sys/mman.h>

#include <stdio.h>
#include "syscall_nums.h"


#define OCALL_PRINT_STRING 1



unsigned long ocall_print_string(char* string);





int main(){
	unsigned long size = 0x1000;
 	//printf("pagesize is 0x%lx\n", size);	
	char * mmap_region = (char*)SYSCALL(
	           SYS_mmap,
	           0x2000000000,
	           size,
	           PROT_READ|PROT_WRITE|PROT_EXEC,
	           MAP_ANON|MAP_PRIVATE,
	           -1
	          );

	  	if (mmap_region == MAP_FAILED) {
		  //printf("!! MMAP 1 FAILED !!\n");
                } else {
		  // printf("!! MMAP 1 SUCCESS !!\n");
        }

	    char * mmap_region2 = (char*)SYSCALL(
	           SYS_mmap,
	           0x1000000000,
	           size,
	           PROT_READ|PROT_WRITE|PROT_EXEC,
	           MAP_ANON|MAP_PRIVATE,
	           -2
	          );

  	if (mmap_region2 == MAP_FAILED) {
                   // printf("!! MMAP 2 FAILED !!\n");
                } else {
                    //  printf("!! MMAP 2 SUCCESS !!\n");
        }

  	int unmap_result = SYSCALL(SYS_munmap, mmap_region, size, -1,0,0);
 	 if (unmap_result != 0) {
    	//printf("Could not munmap mmap region 1\n");
  	} else {
      	//printf("Successfully unmapped region 1\n");
  	}


	  	
	int unmap_result2 = SYSCALL(SYS_munmap, mmap_region2, size, -2,0,0);
 	 if (unmap_result2 != 0) {
    	//printf("Could not munmap mmap region 2\n");
  	} else {
      	//printf("Successfully unmapped region 2\n");
  	}




  EAPP_RETURN(0);
}

unsigned long ocall_print_string(char* string){
  unsigned long retval;
  ocall(OCALL_PRINT_STRING, string, strlen(string)+1, &retval ,sizeof(unsigned long));
  return retval;
}
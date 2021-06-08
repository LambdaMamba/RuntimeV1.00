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


char *my_strcpy(char *strDest, const char *strSrc);


int main(){
	unsigned long size = (0x1000)*5;
	char * mmap_region = (char*)SYSCALL(
	           SYS_mmap,
	           0x2000000000,
	           size,
	           PROT_READ|PROT_WRITE|PROT_EXEC,
	           MAP_ANON|MAP_PRIVATE,
	           -1
	          );

	  	if (mmap_region == MAP_FAILED) {
		    ocall_print_string("mmap into DRAM region fail");
                } else {
		    ocall_print_string("mmap into DRAM region successful");

        }
		my_strcpy(mmap_region, "SUCCESSFULLY WROTE INTO DRAM REGION");
		ocall_print_string(mmap_region);


	    char * mmap_region2 = (char*)SYSCALL(
	           SYS_mmap,
	           0x1000000000,
	           size,
	           PROT_READ|PROT_WRITE|PROT_EXEC,
	           MAP_ANON|MAP_PRIVATE,
	           -2
	          );

  	if (mmap_region2 == MAP_FAILED) {
                  ocall_print_string("mmap into NVM region failed");

                } else {
	   	  ocall_print_string("mmap into NVM region successful");

        }
                my_strcpy(mmap_region2, "SUCCESSFULLY WROTE INTO NVM REGION");
                ocall_print_string(mmap_region2);


  	int unmap_result = SYSCALL(SYS_munmap, mmap_region, size, -1,0,0);
 	 if (unmap_result != 0) {
		 ocall_print_string("munmap of DRAM failed");
    
  	} else {
		ocall_print_string("munmap of DRAM success");
      
  	}


	  	
	int unmap_result2 = SYSCALL(SYS_munmap, mmap_region2, size, -2,0,0);
 	 if (unmap_result2 != 0) {
                 ocall_print_string("munmap of NVM failed");
  	} else {
                 ocall_print_string("munmap of NVM sucess");

  	}

	my_strcpy(mmap_region, "SUCCESSFULLY WROTE INTO DRAM REGION");
	my_strcpy(mmap_region2, "SUCCESSFULLY WROTE INTO NVM REGION");

  EAPP_RETURN(0);
}

unsigned long ocall_print_string(char* string){
  unsigned long retval;
  ocall(OCALL_PRINT_STRING, string, strlen(string)+1, &retval ,sizeof(unsigned long));
  return retval;
}

char *my_strcpy(char *strDest, const char *strSrc)
                {
                    if (strSrc == NULL || strDest == NULL)
                        return NULL;

                    char *dest = strDest;

                    while ((*strDest++ = *strSrc++) != '\0');

                    return dest;
                }


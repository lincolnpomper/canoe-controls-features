#define _GNU_SOURCE
#include "arm_hook.h"
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#if !defined(__arm__) || defined(__thumb__)
#error This hook requires 32-bit ARM instruction state.
#endif
static int protect_entry(void *p, int prot) {
    long ps=sysconf(_SC_PAGESIZE);
    if(ps<=0) {errno=EINVAL;return -1;}
    uintptr_t first=(uintptr_t)p & ~((uintptr_t)ps-1);
    uintptr_t end=((uintptr_t)p+8+(uintptr_t)ps-1)&~((uintptr_t)ps-1);
    return mprotect((void*)first,end-first,prot);
}
static void flush(void *p,size_t n) {__builtin___clear_cache((char*)p,(char*)p+n);}
int arm_hook_install(struct arm_hook *h,void *entry,void *replacement,
                     const uint32_t *expected,size_t words) {
    if(!h || !entry || !replacement || !expected || words<2 ||
       ((uintptr_t)entry&3) || ((uintptr_t)replacement&3) || h->installed) {
        errno=EINVAL;return -1;
    }
    if(memcmp(entry,expected,words*4)) {errno=ENOEXEC;return -1;}
    long ps=sysconf(_SC_PAGESIZE);if(ps<=0){errno=EINVAL;return -1;}
    uint32_t *tr=mmap(NULL,(size_t)ps,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    if(tr==MAP_FAILED)return -1;
    memcpy(h->original,entry,8);
    tr[0]=h->original[0];tr[1]=h->original[1];
    tr[2]=0xe51ff004u; /* ldr pc,[pc,#-4]; absolute ARM jump */
    tr[3]=(uint32_t)((uintptr_t)entry+8);
    flush(tr,16);
    if(mprotect(tr,(size_t)ps,PROT_READ|PROT_EXEC)) {munmap(tr,(size_t)ps);return -1;}
    if(protect_entry(entry,PROT_READ|PROT_WRITE|PROT_EXEC)) {munmap(tr,(size_t)ps);return -1;}
    h->entry=entry;h->trampoline=tr;h->allocation=(size_t)ps;
    uint32_t patch[2]={0xe51ff004u,(uint32_t)(uintptr_t)replacement};
    memcpy(entry,patch,sizeof(patch));flush(entry,8);
    if(protect_entry(entry,PROT_READ|PROT_EXEC)) {
        int saved=errno;memcpy(entry,h->original,8);flush(entry,8);
        (void)protect_entry(entry,PROT_READ|PROT_EXEC);
        munmap(tr,(size_t)ps);memset(h,0,sizeof(*h));errno=saved;return -1;
    }
    h->installed=1;return 0;
}
int arm_hook_remove(struct arm_hook *h) {
    if(!h || !h->installed){errno=EINVAL;return -1;}
    if(protect_entry(h->entry,PROT_READ|PROT_WRITE|PROT_EXEC))return -1;
    memcpy(h->entry,h->original,8);flush(h->entry,8);
    if(protect_entry(h->entry,PROT_READ|PROT_EXEC))return -1;
    munmap(h->trampoline,h->allocation);memset(h,0,sizeof(*h));return 0;
}

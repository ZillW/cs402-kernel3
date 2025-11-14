/******************************************************************************/
/* Important Fall 2025 CSCI 402 usage information:                            */
/*                                                                            */
/* This fils is part of CSCI 402 kernel programming assignments at USC.       */
/*         53616c7465645f5fd1e93dbf35cbffa3aef28f8c01d8cf2ffc51ef62b26a       */
/*         f9bda5a68e5ed8c972b17bab0f42e24b19daa7bd408305b1f7bd6c7208c1       */
/*         0e36230e913039b3046dd5fd0ba706a624d33dbaa4d6aab02c82fe09f561       */
/*         01b0fd977b0051f0b0ce0c69f7db857b1b5e007be2db6d42894bf93de848       */
/*         806d9152bd5715e9                                                   */
/* Please understand that you are NOT permitted to distribute or publically   */
/*         display a copy of this file (or ANY PART of it) for any reason.    */
/* If anyone (including your prospective employer) asks you to post the code, */
/*         you must inform them that you do NOT have permissions to do so.    */
/* You are also NOT permitted to remove or alter this comment block.          */
/* If this comment block is removed or altered in a submitted file, 20 points */
/*         will be deducted.                                                  */
/******************************************************************************/

#include "globals.h"
#include "errno.h"
#include "types.h"

#include "mm/mm.h"
#include "mm/tlb.h"
#include "mm/mman.h"
#include "mm/page.h"

#include "proc/proc.h"

#include "util/string.h"
#include "util/debug.h"

#include "fs/vnode.h"
#include "fs/vfs.h"
#include "fs/file.h"

#include "vm/vmmap.h"
#include "vm/mmap.h"

/*
 * This function implements the mmap(2) syscall, but only
 * supports the MAP_SHARED, MAP_PRIVATE, MAP_FIXED, and
 * MAP_ANON flags.
 *
 * Add a mapping to the current process's address space.
 * You need to do some error checking; see the ERRORS section
 * of the manpage for the problems you should anticipate.
 * After error checking most of the work of this function is
 * done by vmmap_map(), but remember to clear the TLB.
 */
int
do_mmap(void *addr, size_t len, int prot, int flags,
        int fd, off_t off, void **ret)
{

if (len == 0) {
        return -EINVAL;
    }
    if (!PAGE_ALIGNED(off)) {
        return -EINVAL;
    }
    if (!((flags & MAP_SHARED) || (flags & MAP_PRIVATE))) {
        return -EINVAL;
    }
    if ((flags & MAP_FIXED) && !PAGE_ALIGNED(addr)) {
        return -EINVAL;
    }

    uint32_t page_count = (len + PAGE_SIZE - 1) / PAGE_SIZE;
    uint32_t start_vfn = 0;

    if (addr != NULL) {
        start_vfn = ADDR_TO_PN(addr);
    }

    if (!(flags & MAP_FIXED)) {
        if (addr == NULL) {
             start_vfn = 0;
        } else {
             start_vfn = ADDR_TO_PN(addr);
        }
    }

    file_t *source_file = NULL;
    vnode_t *source_vnode = NULL;

    if (!(flags & MAP_ANON)) {
        if (fd < 0 || fd >= NFILES) {
            return -EBADF;
        }
        source_file = fget(fd);
        if (source_file == NULL) {
            return -EBADF;
        }

        if ((prot & PROT_READ) && !(source_file->f_mode & FMODE_READ)) {
            fput(source_file);
            return -EACCES;
        }
        if ((prot & PROT_WRITE) && !(source_file->f_mode & FMODE_WRITE)) {
            fput(source_file);
            return -EACCES;
        }
        if ((prot & PROT_EXEC) && !(source_file->f_mode & FMODE_READ)) {
            fput(source_file);
            return -EACCES;
        }

        source_vnode = source_file->f_vnode;
    }

    vmarea_t *new_area = NULL;
    int result_code = vmmap_map(curproc->p_vmmap, source_vnode, start_vfn, page_count,
                                prot, flags, off, VMMAP_DIR_HILO, &new_area);

    if (source_file != NULL) {
        fput(source_file);
    }

    if (result_code < 0) {
        return result_code;
    }

    *ret = PN_TO_ADDR(new_area->vma_start);

    tlb_flush_all();

    return 0;

//NOT_YET_IMPLEMENTED("VM: do mmap");

}


/*
 * This function implements the munmap(2) syscall.
 *
 * As with do_mmap() it should perform the required error checking,
 * before calling upon vmmap_remove() to do most of the work.
 * Remember to clear the TLB.
 */
int
do_munmap(void *addr, size_t len)
{
if (!PAGE_ALIGNED(addr)) {
        return -EINVAL;
    }
    if (len == 0) {
        return -EINVAL;
    }

    uint32_t start_vfn = ADDR_TO_PN(addr);
    uint32_t page_count = (len + PAGE_SIZE - 1) / PAGE_SIZE;

    int result_code = vmmap_remove(curproc->p_vmmap, start_vfn, page_count);

    if (result_code < 0) {
        return result_code;
    }

    return 0;

//NOT_YET_IMPLEMENTED("VM: do munmap");
}


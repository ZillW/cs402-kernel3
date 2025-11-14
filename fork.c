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

#include "types.h"
#include "globals.h"
#include "errno.h"

#include "util/debug.h"
#include "util/string.h"

#include "proc/proc.h"
#include "proc/kthread.h"

#include "mm/mm.h"
#include "mm/mman.h"
#include "mm/page.h"
#include "mm/pframe.h"
#include "mm/mmobj.h"
#include "mm/pagetable.h"
#include "mm/tlb.h"

#include "fs/file.h"
#include "fs/vnode.h"

#include "vm/shadow.h"
#include "vm/vmmap.h"

#include "api/exec.h"

#include "main/interrupt.h"

/* Pushes the appropriate things onto the kernel stack of a newly forked thread
 * so that it can begin execution in userland_entry.
 * regs: registers the new thread should have on execution
 * kstack: location of the new thread's kernel stack
 * Returns the new stack pointer on success. */
static uint32_t
fork_setup_stack(const regs_t *regs, void *kstack)
{
        /* Pointer argument and dummy return address, and userland dummy return
         * address */
        //uint32_t esp = ((uint32_t) kstack) + DEFAULT_STACK_SIZE - (sizeof(regs_t) + 12);
	uint32_t usable_stack_top = ((uint32_t) kstack) + DEFAULT_STACK_SIZE - PAGE_SIZE;
        uint32_t esp = usable_stack_top - (sizeof(regs_t) + 12);
        *(void **)(esp + 4) = (void *)(esp + 8); /* Set the argument to point to location of struct on stack */
        memcpy((void *)(esp + 8), regs, sizeof(regs_t)); /* Copy over struct */
        return esp;
}


/*
 * The implementation of fork(2). Once this works,
 * you're practically home free. This is what the
 * entirety of Weenix has been leading up to.
 * Go forth and conquer.
 */
int
do_fork(struct regs *regs)
{

KASSERT(regs != NULL);
        dbg(DBG_PRINT, "(GRADING3A 7.a)\n");
        KASSERT(curproc != NULL);
        dbg(DBG_PRINT, "(GRADING3A 7.a)\n");
        KASSERT(curproc->p_state == PROC_RUNNING);
        dbg(DBG_PRINT, "(GRADING3A 7.a)\n");

        dbg(DBG_PRINT, "(GRADING3B)\n");

        proc_t *child_proc = proc_create("child_proc");
        KASSERT(child_proc != NULL);

        vmmap_t *child_vmmap = vmmap_clone(curproc->p_vmmap);
        KASSERT(child_vmmap != NULL);

        child_proc->p_vmmap = child_vmmap;
        child_proc->p_vmmap->vmm_proc = child_proc;

        vmarea_t *curr_vmarea = NULL;
        list_iterate_begin(&child_vmmap->vmm_list, curr_vmarea, vmarea_t, vma_plink) {
            vmarea_t *temp = vmmap_lookup(curproc->p_vmmap, curr_vmarea->vma_start);
            KASSERT(temp != NULL);

            if ((curr_vmarea->vma_flags & MAP_TYPE) == MAP_PRIVATE) {
                dbg(DBG_PRINT, "(GRADING3B)\n");

                mmobj_t *bottom_obj = mmobj_bottom_obj(temp->vma_obj);
                bottom_obj->mmo_ops->ref(bottom_obj);

                mmobj_t *parent_shadow = shadow_create();
                parent_shadow->mmo_shadowed = temp->vma_obj;
                parent_shadow->mmo_un.mmo_bottom_obj = bottom_obj;

                mmobj_t *child_shadow = shadow_create();
                child_shadow->mmo_shadowed = temp->vma_obj;
                child_shadow->mmo_un.mmo_bottom_obj = bottom_obj;

		temp->vma_obj->mmo_ops->ref(temp->vma_obj);

                list_remove(&temp->vma_olink);
                list_insert_tail(mmobj_bottom_vmas(bottom_obj), &temp->vma_olink);

		list_insert_tail(mmobj_bottom_vmas(bottom_obj), &curr_vmarea->vma_olink);

                temp->vma_obj = parent_shadow;
                curr_vmarea->vma_obj = child_shadow;
            } else {
                dbg(DBG_ANON, "(GRADING3B)\n");
                curr_vmarea->vma_obj = temp->vma_obj;
                curr_vmarea->vma_obj->mmo_ops->ref(curr_vmarea->vma_obj);
            }
            list_insert_tail(mmobj_bottom_vmas(curr_vmarea->vma_obj), &curr_vmarea->vma_olink);
        } list_iterate_end();

        kthread_t *child_thread = kthread_clone(curthr);
        KASSERT(child_thread != NULL);
        
        child_thread->kt_proc = child_proc;

        KASSERT(child_proc->p_state == PROC_RUNNING);
        dbg(DBG_PRINT, "(GRADING3A 7.a)\n");
        KASSERT(child_proc->p_pagedir != NULL);
        dbg(DBG_PRINT, "(GRADING3A 7.a)\n");
        KASSERT(child_thread->kt_kstack != NULL);
        dbg(DBG_PRINT, "(GRADING3A 7.a)\n");

        list_insert_tail(&child_proc->p_threads, &child_thread->kt_plink);

        regs_t child_regs = *regs;
        child_regs.r_eax = 0;

        child_thread->kt_ctx.c_pdptr = child_proc->p_pagedir;
        child_thread->kt_ctx.c_eip = (uint32_t) userland_entry;
        child_thread->kt_ctx.c_esp = fork_setup_stack(&child_regs, child_thread->kt_kstack);
        child_thread->kt_ctx.c_kstack = (uintptr_t) child_thread->kt_kstack;
        child_thread->kt_ctx.c_kstacksz = DEFAULT_STACK_SIZE;

        int i;
        for (i = 0; i < NFILES; i++){
                child_proc->p_files[i] = curproc->p_files[i];
                if (child_proc->p_files[i] != NULL){
                        dbg(DBG_PRINT, "(GRADING3B)\n");
                        fref(child_proc->p_files[i]);
                }
                else {
                        dbg(DBG_PRINT, "(GRADING3B)\n");
                }
        }

       pt_unmap_range(curproc->p_pagedir, USER_MEM_LOW, USER_MEM_HIGH);
       tlb_flush_all();

        child_proc->p_brk = curproc->p_brk;
        child_proc->p_start_brk = curproc->p_start_brk;

        sched_make_runnable(child_thread);
        regs->r_eax = child_proc->p_pid;
        return child_proc->p_pid;

//NOT_YET_IMPLEMENTED("VM: do fork");
}

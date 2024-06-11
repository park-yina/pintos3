/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "threads/vaddr.h"
#include "threads/mmu.h"
#include "lib/string.h"
#include "userprog/process.h"

static bool file_backed_swap_in(struct page *page, void *kva);
static bool file_backed_swap_out(struct page *page);
static void file_backed_destroy(struct page *page);

static bool lazy_load_file(struct page *page, void *aux);

/* DO NOT MODIFY this struct */
static const struct page_operations file_ops = {
    .swap_in = file_backed_swap_in,
    .swap_out = file_backed_swap_out,
    .destroy = file_backed_destroy,
    .type = VM_FILE,
};

struct file
{
    struct inode *inode; /* File's inode. */
    off_t pos;           /* Current position. */
    bool deny_write;     /* Has file_deny_write() been called? */
};

/* The initializer of file vm */
void vm_file_init(void)
{
    lock_init(&file_lock);
}

/* Initialize the file backed page */
bool file_backed_initializer(struct page *page, enum vm_type type, void *kva)
{
    /* Set up the handler */
    page->operations = &file_ops;

    struct file_page *file_page = &page->file;

    file_page->aux = page->uninit.aux;

    return true;
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in(struct page *page, void *kva)
{
    struct file_page *file_page = &page->file;
    struct necessary_info *nec = (struct necessary_info *)file_page->aux;

    file_seek(nec->file, nec->ofs);
    lock_acquire(&file_lock);
    file_read(nec->file, kva, nec->read_byte);
    lock_release(&file_lock);

    memset(kva + nec->read_byte, 0, nec->zero_byte);

    return true;
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out(struct page *page)
{
    struct file_page *file_page UNUSED = &page->file;
    if (page == NULL)
    {
        return false;
    }
    struct necessary_info *nec = file_page->aux;
    struct file *file = nec->file;
    lock_acquire(&file_lock);
    if (pml4_is_dirty(thread_current()->pml4, page->va))
    {
        file_write_at(file, page->va, nec->read_byte, nec->ofs);
        pml4_set_dirty(thread_current()->pml4, page->va, false);
    }
    pml4_clear_page(thread_current()->pml4, page->va);
    lock_release(&file_lock);
    return true;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy(struct page *page)
{
    struct file_page *file_page UNUSED = &page->file;
    struct necessary_info *nec = file_page->aux;
    struct thread *curr = thread_current();

    if (pml4_is_dirty(curr->pml4, page->va))
    {
        file_write_at(nec->file, page->va, nec->read_byte, nec->ofs);
        pml4_set_dirty(curr->pml4, page->va, 0);
    }
    pml4_clear_page(curr->pml4, page->va);
}
/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "userprog/syscall.h"
#include "threads/mmu.h"
#include "vm/vm.h"
static bool file_backed_swap_in (struct page *page, void *kva);
static bool file_backed_swap_out (struct page *page);
static void file_backed_destroy (struct page *page);
extern struct lock filesys_lock;
extern struct lock lru_lock;

/* DO NOT MODIFY this struct */
static const struct page_operations file_ops = {
	.swap_in = file_backed_swap_in,
	.swap_out = file_backed_swap_out,
	.destroy = file_backed_destroy,
	.type = VM_FILE,
};

/* The initializer of file vm */
void
vm_file_init (void) {
}

/* Initialize the file backed page */
bool file_backed_initializer(struct page *page, enum vm_type type, void *kva)
{
    /* Set up the handler */
    page->operations = &file_ops;

    struct file_page *file_page = &page->file;
}

static bool
file_backed_swap_in(struct page *page, void *kva)
{
    // printf("file_backed_swap_in\n");
    struct file_page *file_page = &page->file;

    lock_acquire(&filesys_lock);
    off_t size = file_read_at(file_page->file, kva, (off_t)file_page->read_bytes, file_page->offset);
    lock_release(&filesys_lock);

    if (size != file_page->read_bytes)
        return false;

    memset(kva + file_page->read_bytes, 0, file_page->zero_bytes);

    return true;
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out(struct page *page)
{
    struct file_page *file_page = &page->file;
    struct thread *curr_thread = thread_current();

    if (pml4_is_dirty(curr_thread->pml4, page->va))
    {
        lock_acquire(&filesys_lock);
        file_write_at(file_page->file, page->va, file_page->read_bytes, file_page->offset);
        lock_release(&filesys_lock);

        pml4_set_dirty(curr_thread->pml4, page->va, false);
    }
    pml4_clear_page(curr_thread->pml4, page->va);
    page->frame = NULL;

    return true;
}
/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy(struct page *page)
{
    struct file_page *file_page = &page->file;
    list_remove(&(file_page->file));
    if (page->frame != NULL)
    {
        lock_acquire(&lru_lock);
        list_remove(&(page->frame->frame_elem));
        lock_release(&lru_lock);
        free(page->frame);
    }
}
/* Do the mmap */
/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */

#include "vm/vm.h"
#include "devices/disk.h"
#include "threads/vaddr.h"
#include "userprog/syscall.h"
#include "lib/kernel/bitmap.h"
/* DO NOT MODIFY BELOW LINE */
static struct disk *swap_disk;
static bool anon_swap_in (struct page *page, void *kva);
static bool anon_swap_out (struct page *page);
static void anon_destroy (struct page *page);
struct bitmap *swap_table;
const size_t SECTORS_PER_PAGE = PGSIZE / DISK_SECTOR_SIZE; 
/* DO NOT MODIFY this struct */
static const struct page_operations anon_ops = {
	.swap_in = anon_swap_in,
	.swap_out = anon_swap_out,
	.destroy = anon_destroy,
	.type = VM_ANON,
};

/* Initialize the data for anonymous pages */
void vm_anon_init (void) {
	swap_disk = disk_get(1, 1);
    size_t swap_size = disk_size(swap_disk) / SECTORS_PER_PAGE;  // (size/page)*sector
    swap_table = bitmap_create(swap_size);
}

/* Initialize the file mapping */
bool anon_initializer (struct page *page, enum vm_type type, void *kva) {

	struct uninit_page *uninit = &page->uninit;
	memset(uninit, 0, sizeof(struct uninit_page));

	/* Set up the handler */
	page->operations = &anon_ops;

	struct anon_page *anon_page = &page->anon;
	anon_page->swap_index = -1;

	return true;
}

/* Swap in the page by read contents from the swap disk. */
static bool anon_swap_in (struct page *page, void *kva) {
	struct anon_page *anon_page = &page->anon;
	

	int page_no = anon_page->swap_index;

    if (bitmap_test(swap_table, page_no) == false) {
        return false;
    }

    for (int i = 0; i < SECTORS_PER_PAGE; ++i) {
        disk_read(swap_disk, page_no * SECTORS_PER_PAGE + i, kva + DISK_SECTOR_SIZE * i);
    }

    bitmap_set(swap_table, page_no, false);
    
    return true;
}

/* Swap out the page by writing contents to the swap disk. */
static bool anon_swap_out (struct page *page) {
	struct anon_page *anon_page = &page->anon;

	/* 비트맵을 처음부터 순회해 false 값을 가진 비트를 하나 찾는다.
	   즉, 페이지를 할당받을 수 있는 swap slot을 하나 찾는다. */
	int page_no = bitmap_scan(swap_table, 0, 1, false);

    if (page_no == BITMAP_ERROR) {
        return false;
    }
    for (int i = 0; i < SECTORS_PER_PAGE; ++i) {
        disk_write(swap_disk, page_no * SECTORS_PER_PAGE + i, page->va + DISK_SECTOR_SIZE * i);
    }

    bitmap_set(swap_table, page_no, true);
    pml4_clear_page(thread_current()->pml4, page->va);

    anon_page->swap_index = page_no;

    return true;
}
/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void
anon_destroy (struct page *page) {
	struct anon_page *anon_page = &page->anon;
}

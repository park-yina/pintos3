#ifndef VM_FILE_H
#define VM_FILE_H
#include "filesys/file.h"
#include "vm/vm.h"
void *do_mmap(void *addr, size_t length, int writable,
		struct file *file, off_t offset);
void do_munmap (void *va);
struct page;
enum vm_type;

struct file_page {
    	struct file *file;
    	size_t length;
    	off_t offset;
};


void vm_file_init (void);
bool file_backed_initializer (struct page *page, enum vm_type type, void *kva);

bool lazy_load_segment_for_file(struct page *page, void *aux);
#endif

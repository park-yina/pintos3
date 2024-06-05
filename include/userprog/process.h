#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

tid_t process_create_initd (const char *file_name);
tid_t process_fork (const char *name, struct intr_frame *if_);
int process_exec (void *f_name);
int process_wait (tid_t);
void process_exit (void);
bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
		uint32_t read_bytes, uint32_t zero_bytes,
		bool writable);

void process_activate (struct thread *next);
struct container{
    struct file *file;
    off_t offset;
    size_t page_read_bytes;
};
#define ELF ELF64_hdr
#define Phdr ELF64_PHDR

bool validate_segment(const struct Phdr *phdr, struct file *file);
#endif /* userprog/process.h */
/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "vm/uninit.h"
#include "vm/anon.h"
#include "vm/file.h"
#include "threads/vaddr.h"
#include "lib/kernel/hash.h"
#include "lib/kernel/list.h"

/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
void
vm_init (void) {
	vm_anon_init ();
	vm_file_init ();
#ifdef EFILESYS  /* For project 4 */
	pagecache_init ();
#endif
	register_inspect_intr ();
	/* DO NOT MODIFY UPPER LINES. */
	/* TODO: Your code goes here. */
}
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
		uint32_t read_bytes, uint32_t zero_bytes, bool writable) {
	ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
	ASSERT (pg_ofs (upage) == 0);
	ASSERT (ofs % PGSIZE == 0);

	while (read_bytes > 0 || zero_bytes > 0) {
		/* Do calculate how to fill this page.
		 * We will read PAGE_READ_BYTES bytes from FILE
		 * and zero the final PAGE_ZERO_BYTES bytes. */
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		/* TODO: Set up aux to pass information to the lazy_load_segment. */
		struct segment *seg = calloc(1, sizeof(struct segment));
		seg->file = file;
		seg->offset = ofs;
		seg->page_read_bytes = page_read_bytes;
		seg->page_zero_bytes = page_zero_bytes;

		void *aux = seg;
		if (!vm_alloc_page_with_initializer (VM_ANON, upage, writable, lazy_load_segment, aux)){
			free(seg);
			return false;
		}
		/* Advance. */
		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;		
		ofs += page_read_bytes;
		upage += PGSIZE;
	}
	return true;
}

struct segment {
    struct file *file;
    off_t offset;           
    size_t page_read_bytes;
    size_t page_zero_bytes;
};
/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */
enum vm_type
page_get_type (struct page *page) {
	int ty = VM_TYPE (page->operations->type);
	switch (ty) {
		case VM_UNINIT:
			return VM_TYPE (page->uninit.type);
		default:
			return ty;
	}
}
// bool
// hash_init (struct hash *h, 
// 			hash_hash_func *hash, hash_less_func *less, void *aux) {
// 	h->elem_cnt = 0;
// 	h->bucket_cnt = 4;
// 	h->buckets = malloc (sizeof *h->buckets * h->bucket_cnt);
// 	h->hash = hash;
// 	h->less = less;
// 	h->aux = aux;

// 	if (h->buckets != NULL) {
// 		hash_clear (h, NULL);
// 		return true;
// 	} else
// 		return false;
// }
/* Helpers */
static struct frame *vm_get_victim (void);
static bool vm_do_claim_page (struct page *page);
static struct frame *vm_evict_frame (void);

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
/* Find VA from spt and return page. On error, return NULL. */
struct page *spt_find_page(struct supplemental_page_table *h , void *va ) {
    struct page *page = NULL;
    /* TODO: Fill this function. */

    // 페이지 테이블에서 검색할 때 사용할 더미 페이지 생성
    struct page dummy_page;
    dummy_page.va = pg_round_down(va);  // 주어진 가상 주소를 페이지 경계로 내림

    struct hash_elem *e;

    // 해시 테이블에서 더미 페이지의 해시 요소를 사용해 페이지 검색
    e = hash_find(&h->spt_hash, &dummy_page.hash_elem);

    // 페이지를 찾지 못하면 NULL 반환
    if (e == NULL) {
        return NULL;
    }

    // 찾은 해시 요소를 페이지 구조체로 변환하여 반환
    return page = hash_entry(e, struct page, hash_elem);
}

 bool vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable, vm_initializer *init, void *aux) {
    
    	ASSERT (VM_TYPE(type) != VM_UNINIT)
    
    	struct supplemental_page_table *spt = &thread_current ()->spt;
    
    	/* Check wheter the upage is already occupied or not. */
    	if (spt_find_page (spt, upage) == NULL) {
    		/* TODO: Create the page, fetch the initialier according to the VM type, 
    							and then create "uninit" page struct by calling uninit_new. 
    							You should modify the field after calling the uninit_new. */
    		/* P3 추가 */
    		bool (*initializer)(struct page *, enum vm_type, void *);
    		switch(type){
    			case VM_ANON: case VM_ANON|VM_MARKER_0: // 왜 두번째 케이스에서 저렇게 이중(?)으로 체크하지? 하나 주석처리해도 될 듯. -> 케이스1 or 케이스2 (|:비트연산자or)
    				initializer = anon_initializer;
    				break;
    			case VM_FILE:
    				initializer = file_backed_initializer;
    				break;
    		}
    
    		struct page *new_page = malloc(sizeof(struct page));
    		uninit_new (new_page, upage, init, type, aux, initializer);
    
    		new_page->writable = writable;
    		new_page->page_cnt = -1; // only for file-mapped pages
    
    		/* TODO: Insert the page into the spt. */
    		spt_insert_page(spt, new_page); // should always return true - checked that upage is not in spt
    
    			return true;
    	}
    err:
    	return false;
    }
/* Find VA from spt and return page. On error, return NULL. */

/* Insert PAGE into spt with validation. */
/* Insert PAGE into spt with validation. */
bool delete_page (struct hash *pages, struct page *p) {
	if (hash_delete(pages, &p->hash_elem))
		return false;
	else
		return true;
}

        /* Insert PAGE into spt with validation. */
static bool
insert_page(struct hash *h, struct page *p) {
    if(!hash_insert(h, &p->hash_elem))
        return true;
    else
        return false;
}

bool 
spt_insert_page(struct supplemental_page_table *spt, struct page *page) {
    /* P3 추가 */
    struct hash_elem *e = hash_find(&spt->spt_hash, &page->hash_elem);
    if(e != NULL) // page already in SPT
        return false; // false, fail

    // page not in SPT
    return insert_page(&spt->spt_hash, page);
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	vm_dealloc_page (page);
	return true;
}

/* Get the struct frame, that will be evicted. */
static struct frame 
*vm_get_victim (void) {
	struct frame *victim = NULL;
	 /* TODO: The policy for eviction is up to you. */
	/* P3 추가 */
	victim = list_entry(list_pop_front(&frame_table), struct frame, elem); /
	return victim;
}
/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame (void) {
	struct frame *victim UNUSED = vm_get_victim ();
	/* TODO: swap out the victim and return the evicted frame. */

	return NULL;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
static struct frame *vm_get_frame (void) {
	struct frame *frame = NULL;
	void *kva = palloc_get_page(PAL_USER);
	/* TODO: Fill this function. */

	/* P3 추가 */
	if (kva == NULL){ // NULL이면(사용 가능한 페이지가 없으면) 
		frame = vm_evict_frame(); // 페이지 삭제 후 frame 리턴
	}
	else{ // 사용 가능한 페이지가 있으면
		frame = malloc(sizeof(struct frame)); // 페이지 사이즈만큼 메모리 할당
		frame->kva = kva;
	}

	ASSERT (frame != NULL);
	// ASSERT (frame->page == NULL);
	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth (void *addr UNUSED) {
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

bool
vm_try_handle_fault (struct intr_frame *f, void *addr ,
		bool user , bool write , bool not_present ) {
	struct supplemental_page_table *spt  = &thread_current ()->spt;
	struct page *page = NULL;
	/* TODO: Validate the fault */
	/* TODO: Your code goes here */

	// return vm_do_claim_page (page);

	// Step 1. Locate the page that faulted in the supplemental page table
	void * fpage_uvaddr = pg_round_down(addr); // round down to nearest PGSIZE
	// void * fpage_uvaddr = (uint64_t)addr - ((uint64_t)addr%PGSIZE); // round down to nearest PGSIZE

	struct page *fpage = spt_find_page(spt, fpage_uvaddr);
	
	// Invalid access - Not in SPT (stack growth or abort) / kernel vaddr / write request to read-only page
	if(is_kernel_vaddr(addr)){

		return false;
	}
	else if (fpage == NULL){
		void *rsp = user ? f->rsp : thread_current()->rsp; // a page fault occurs in the kernel
		const int GROWTH_LIMIT = 32; // heuristic
		const int STACK_LIMIT = USER_STACK - (1<<20); // 1MB size limit on stack

		// Check stack size max limit and stack growth request heuristically
		if((uint64_t)addr > STACK_LIMIT && USER_STACK > (uint64_t)addr && (uint64_t)addr > (uint64_t)rsp - GROWTH_LIMIT){
			vm_stack_growth (fpage_uvaddr);
			fpage = spt_find_page(spt, fpage_uvaddr);
		}
		else{
			exit(-1); // mmap-unmap
			//return false;
		}
	}
	else if(write && !fpage->writable){

		exit(-1); // mmap-ro
		// return false;
	}

	ASSERT(fpage != NULL);

	// Step 2~4.
	bool gotFrame = vm_do_claim_page (fpage);

	// if (gotFrame)
		// list_push_back(&frame_table, &fpage->frame->elem);

	return gotFrame;
}
/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void
vm_dealloc_page (struct page *page) {
	destroy (page);
	free (page);
}

/* Claim the page that allocate on VA. */
bool
vm_claim_page (void *va) {
	/* P3 추가 */
	struct supplemental_page_table *spt = &thread_current()->spt;
	struct page *page = spt_find_page(spt, va);
	if (page == NULL) {
		return false;
	}
	return vm_do_claim_page (page);
}

/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame ();
	/* P3 추가 */

	/* Set links */
	frame->page = page;
	page->frame = frame;

	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	struct thread *cur = thread_current();
	bool writable = page->writable;
	pml4_set_page(cur->pml4, page->va, frame->kva, writable);
	// add the mapping from the virtual address to the physical address in the page table.

	bool res = swap_in (page, frame->kva);

	return res;
}
/* Initialize new supplemental page table */
// bool
// hash_init (struct hash *h, 
// 			hash_hash_func *hash, hash_less_func *less, void *aux) {
// 	h->elem_cnt = 0;
// 	h->bucket_cnt = 4;
// 	h->buckets = malloc (sizeof *h->buckets * h->bucket_cnt);
// 	h->hash = hash;
// 	h->less = less;
// 	h->aux = aux;

// 	if (h->buckets != NULL) {
// 		hash_clear (h, NULL);
// 		return true;
// 	} else
// 		return false;
// }
unsigned page_hash(const struct hash_elem *h1,void *aux){
	const struct page *p2=hash_entry(p2, struct page, hash_elem);
	return hash_bytes(&p2->va,sizeof(p2->va));
	//바이트를 구해오기 위해 p2->va의 사이즈를 같이 인자로
}
bool page_less(const struct hash_elem *a1, const struct hash_elem *b1,void *aux UNUSED){
	const struct page *a2= hash_entry(a1,struct page,hash_elem);
	const struct page *b2= hash_entry(b1,struct page, hash_elem);
	return a2->va<b2->va;//작은 것을 기준으로 알아서 return
}
void supplemental_page_table_init (struct supplemental_page_table *spt){
	hash_init(&spt->spt_hash,page_hash,page_less,NULL);
}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst ,
        struct supplemental_page_table *src ) {
    // project 3
    struct hash_iterator i; 
    hash_first(&i, &src->pages);
    while(hash_next(&i)){
        struct page *parent_page = hash_entry(hash_cur(&i),struct page, hash_elem);
        enum vm_type type = page_get_type(parent_page);
        void *upage = parent_page->va;
        bool writable = parent_page->writable;
        vm_initializer *init = parent_page->uninit.init;
        void* aux = parent_page->uninit.aux;

        if(parent_page->uninit.type & VM_MARKER_0){
            setup_stack(&thread_current()->tf);
        }
        else if(parent_page->operations->type == VM_UNINIT){
            if(!vm_alloc_page_with_initializer(type, upage, writable, init, aux))
                return false;
        }
        else{
            if(!vm_alloc_page(type, upage, writable))
                return false;
            if(!vm_claim_page(upage))
                return false;
        }

        if(parent_page->operations->type != VM_UNINIT){
            struct page* child_page = spt_find_page(dst,upage);
            memcpy(child_page->frame->kva, parent_page->frame->kva, PGSIZE);
        }
    }
    return true;

}
void spt_destructor(struct hash_elem *e, void* aux){
    const struct page *p = hash_entry(e, struct page, hash_elem);
    free(p);
}

void
supplemental_page_table_kill (struct supplemental_page_table *spt) {
    /* TODO: Destroy all the supplemental_page_table hold by thread and
     * TODO: writeback all the modified contents to the storage. */
    // project 3
    struct hash_iterator i;

    hash_first(&i, &spt->pages);
    while(hash_next(&i)){
        struct page *page = hash_entry(hash_cur(&i), struct page, hash_elem);

        if(page->operations->type == VM_FILE){
            do_munmap(page->va);
        }
    }
    hash_destroy(&spt->pages, spt_destructor);
}
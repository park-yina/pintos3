/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "userprog/process.h"
#include "vm/inspect.h"
#include "threads/vaddr.h"
#include "vm/file.h"

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
	list_init(&frame_table);
}

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

/* Helpers */
static struct frame *vm_get_victim (void);
static bool vm_do_claim_page (struct page *page);
static struct frame *vm_evict_frame (void);

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */

bool vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable, vm_initializer *init, void *aux) {
	ASSERT (VM_TYPE(type) != VM_UNINIT);

	struct supplemental_page_table *spt = &thread_current ()->spt;

	/* Check whether the upage is already occupied or not. */
	if (spt_find_page (spt, upage) == NULL) {
		struct page *page = malloc(sizeof (struct page));

		switch (VM_TYPE(type)){
			case VM_ANON:
				uninit_new(page, pg_round_down(upage), init, type, aux, anon_initializer);
				break;
			case VM_FILE:
				uninit_new(page, pg_round_down(upage), init, type, aux, file_backed_initializer);
				break;
			default:
				free(page);
				goto err;
		}
		
		page->writable = writable;
		if(!spt_insert_page(spt, page)){
			free(page);
			goto err;
		}
		return true;	
	}
err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct supplemental_page_table *spt UNUSED, void *va UNUSED) {
	struct page *page = NULL;
	/* TODO: Fill this function. */
    struct page dummy_page; dummy_page.va = pg_round_down(va); // dummy for hashing
    struct hash_elem *e;

    e = hash_find(&spt->spt_hash, &dummy_page.hash_elem);

	if(e == NULL)
		return NULL;

    return page = hash_entry(e, struct page, hash_elem);
}

/* Insert PAGE into spt with validation. */
bool spt_insert_page (struct supplemental_page_table *spt UNUSED, struct page *page UNUSED) {
	int succ = false;
	/* P3 추가 */
	struct hash_elem *e = hash_find(&spt->spt_hash, &page->hash_elem);
	if(e != NULL) // page already in SPT
		return succ; // false, fail

	// page not in SPT
	hash_insert (&spt->spt_hash, &page->hash_elem);	
	return succ = true;
}

/* P3 추가 */
bool delete_page (struct hash *pages, struct page *p) {
	if (hash_delete(pages, &p->hash_elem))
		return false;
	else
		return true;
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	vm_dealloc_page (page);
	return true;
}

/* Get the struct frame, that will be evicted. */
static struct frame *vm_get_victim (void) {
	struct frame *victim = NULL;
	 /* TODO: The policy for eviction is up to you. */
	/* P3 추가 */
	victim = list_entry(list_pop_front(&frame_table), struct frame, elem); // FIFO algorithm
	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame (void) {
	struct frame *victim = vm_get_victim();
	/* TODO: swap out the victim and return the evicted frame. */
	#ifdef DBG_swap
		printf("(vm_evict_frame) frame %p(page %p) selected and now swapping out\n", victim->kva, victim->page->va);
	#endif
	if(victim->page != NULL){
		swap_out(victim->page);
	}
	// Manipulate swap table according to its design
	return victim;
}

/* palloc() and get frame. 
	If there is no available page, evict the page and return it. 
	This always return valid address. 
	That is, if the user pool memory is full, 
	this function evicts the frame to get the available memory space.*/
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
static void vm_stack_growth (void *addr) {
	vm_alloc_page(VM_ANON | VM_MARKER_0, addr, true); // Create uninit page for stack; will become anon page
	//bool success = vm_claim_page(addr);
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* Return true on success */
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED,
		bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
	struct supplemental_page_table *spt UNUSED = &thread_current ()->spt;
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

	if (gotFrame)
		list_push_back(&frame_table, &fpage->frame->elem);

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
vm_claim_page (void *va UNUSED) {
	// struct page *page = NULL;
	/* TODO: Fill this function */
	// 페이지를 va에 할당해라..

	// 일단 NULL인 페이지 구조체 포인터 하나를 가져왔고
	// 이 페이지를 va에 할당..
	// va를 ..

	// 여기서 할당 준비해서 do_claim으로 보내서 페이지테이블에 맵핑
	// 이 주제의 목적인 supplemental_page_table에서 찾기 구현을 여기서 하면 됨!!
	ASSERT(is_user_vaddr(va)) // 체크용
	struct supplemental_page_table *spt = &thread_current()->spt; // 이러면 주소를 가리키는건가?
	struct page *page = spt_find_page(spt, va);
	if (page == NULL) {
		return false;
	}
	return vm_do_claim_page (page);
}

/* Claim the PAGE and set up the mmu. */
/* va에서 PT(안의 pa)에 매핑을 추가함. */
static bool
vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame ();
	/* P3 추가 */

	/* Set links */
	frame->page = page;
	page->frame = frame;

	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	struct thread *cur = thread_current();
	bool writable = page->writable; // [vm.h] struct page에 bool writable; 추가
	pml4_set_page(cur->pml4, page->va, frame->kva, writable);
	// add the mapping from the virtual address to the physical address in the page table.

	bool res = swap_in (page, frame->kva);

	return res;
}

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt UNUSED) {
	hash_init(&spt->spt_hash, page_hash, page_less, NULL);
}
// '추가'페이지 테이블 이니까, 일반 페이지 테이블 init처럼 시작하면 되지 않을까? 근데 일반 페이지테이블 함수는 어디있을깡
// struct page의 union sturct 중 하나를 사용해야 하나?
/* 페이지 폴트 및 리소스 관리를 처리하기 위해 각 페이지에 대한 추가 정보를 보유하는 추가 페이지 테이블도 필요합니다. */
/* 추가 페이지 테이블에 사용할 데이터 구조를 선택할 수 있습니다. -> 비트맵 or 해시테이블 구조로 짜면 된다. */
// 자료형 초깃값, 멤버변수들 넣기. struct도 채워야 함


/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
		struct supplemental_page_table *src UNUSED) {
	src->spt_hash.aux = dst; // pass 'dst' as aux to 'hash_apply'
	hash_apply(&src->spt_hash, hash_action_copy);
	return true;
}

void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
	hash_clear(&spt->spt_hash, hash_action_destroy);
}

// hash 함수
uint64_t hash_func (const struct hash_elem *e, void *aux){
	const struct page *p = hash_entry(e,struct page, hash_elem);
	return hash_bytes(&p->va,sizeof(p->va));
}

bool less_func (const struct hash_elem *a, const struct hash_elem *b, void *aux){
	const struct page *p_a = hash_entry(a, struct page, hash_elem);
	const struct page *p_b = hash_entry(b, struct page, hash_elem); 

	return p_a->va < p_b->va;
}

bool page_delete(struct hash *hash, struct page *page) {
    return !hash_delete(hash, &page->hash_elem) ? true : false;
}

void spt_destroy (struct hash_elem *e, void *aux UNUSED) {
    struct page *page = hash_entry(e, struct page, hash_elem);

    free(page);  
}

void hash_action_destroy (struct hash_elem *e, void *aux UNUSED) {
    struct page *page = hash_entry(e, struct page, hash_elem);
    destroy(page);  
    free(page);  
}
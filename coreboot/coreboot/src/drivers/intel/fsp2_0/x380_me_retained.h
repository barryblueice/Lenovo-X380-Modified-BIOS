/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef X380_ME_RETAINED_H
#define X380_ME_RETAINED_H
#include "x380_me_trace.h"

/* Private diagnostic record, separate from the wrapping console buffer. */
#define X380_TRACE_CBMEM_ID 0x54303134u
#define X380_TRACE_MAGIC 0x53415645u
struct x380_me_record {
	uint32_t magic, revision, s3, send_did, init_status, status_lo, status_hi;
	struct x380_me_sample before, after;
};

static inline void x380_me_save(const struct x380_me_sample *before,
	const struct x380_me_sample *after, uint32_t s3, uint32_t send_did,
	uint32_t init_status, uint64_t status)
{
	struct x380_me_record *r = cbmem_add(X380_TRACE_CBMEM_ID, sizeof(*r));
	if (!r) {
		printk(BIOS_EMERG, "TRACE014 save failed: no CBMEM record\n");
		return;
	}
	memset(r, 0, sizeof(*r));
	r->revision = 1;
	r->s3 = s3;
	r->send_did = send_did;
	r->init_status = init_status;
	r->status_lo = (uint32_t)status;
	r->status_hi = (uint32_t)(status >> 32);
	r->before = *before;
	r->after = *after;
	r->magic = X380_TRACE_MAGIC;
}

static inline void x380_me_replay(void)
{
	const struct x380_me_record *r = cbmem_find(X380_TRACE_CBMEM_ID);
	if (!r || r->magic != X380_TRACE_MAGIC || r->revision != 1) {
		printk(BIOS_EMERG, "TRACE014 saved record missing or invalid\n");
		return;
	}
	printk(BIOS_EMERG, "[Test 014] X380 retained early ME/FSP trace\n");
	printk(BIOS_EMERG, "TRACE014 BEGIN revision=1 source=saved s3=%u SendDid=%u DidInitStat=%u fsp_status=%08x%08x\n",
		r->s3, r->send_did, r->init_status, r->status_hi, r->status_lo);
	x380_me_print("before", &r->before);
	x380_me_print("after", &r->after);
	printk(BIOS_EMERG, "TRACE014 END\n");
}
#endif

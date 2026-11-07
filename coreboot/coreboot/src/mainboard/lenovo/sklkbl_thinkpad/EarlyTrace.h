/* SPDX-License-Identifier: GPL-2.0-only */
/* BSP-only, before native MP/SMM setup. Intel hardware READ/WRITE cycles only.
 * No erase, opcode menu, BAR relocation, unhide, protection unlock or retry.
 * Compiled unchanged into both the x64 bridge and the x86 ramstage. */
#ifndef X380_EARLY_TRACE_H
#define X380_EARLY_TRACE_H
typedef unsigned int xt_u32;
typedef unsigned short xt_u16;
typedef unsigned char xt_u8;
typedef __UINTPTR_TYPE__ xt_uptr;
#define XT_BASE 0xfe010000U
#define XT_START 0x7bf000U
#define XT_SIZE 0x1000U
#define XT_POLLS 200000U
static xt_u32 xt_next;
static int xt_scanned, xt_failed;

static xt_u32 xt_in32(xt_u16 port)
{
    xt_u32 value;
    __asm__ volatile("inl %1,%0" : "=a"(value) : "Nd"(port));
    return value;
}
static void xt_out32(xt_u16 port, xt_u32 value)
{
    __asm__ volatile("outl %0,%1" : : "a"(value), "Nd"(port));
}
static xt_u32 xt_cfg(xt_u32 address)
{
    xt_out32(0xcf8, address);
    return xt_in32(0xcfc);
}
static void xt_control(xt_u32 value)
{
    xt_out32(0xcf8, 0x8000fddcU);
    __asm__ volatile("outb %0,%1" : : "a"((xt_u8)value), "Nd"((xt_u16)0xcfc));
}
static xt_u32 xt_read(xt_u32 offset)
{
    return *(volatile xt_u32 *)(xt_uptr)(XT_BASE + offset);
}
static void xt_write(xt_u32 offset, xt_u32 value)
{
    *(volatile xt_u32 *)(xt_uptr)(XT_BASE + offset) = value;
}
static xt_u32 xt_le32(const xt_u8 *p)
{
    return p[0] | ((xt_u32)p[1] << 8) | ((xt_u32)p[2] << 16) | ((xt_u32)p[3] << 24);
}
static int xt_equal(const xt_u8 *p, const char *name, xt_u32 size)
{
    for (xt_u32 i = 0; i < size; ++i)
        if (p[i] != (xt_u8)name[i]) return 0;
    return 1;
}
static xt_u32 xt_hash(const void *data, xt_u32 bytes)
{
    const xt_u8 *p = data;
    xt_u32 hash = 2166136261U;
    while (bytes--) hash = (hash ^ *p++) * 16777619U;
    return hash;
}

/* Each transfer is bounded to the fixed FMAP read or EARLY_TRACE itself. */
static int xt_transfer(xt_u32 address, xt_u32 *data, xt_u32 bytes, int writing)
{
    if (!bytes || bytes > 64 || (bytes & 3) || (address & 3)) return -1;
    if (writing) {
        if (bytes != 32 || address < XT_START || address > XT_START + XT_SIZE - bytes ||
            ((address - XT_START) & 31)) return -1;
    } else if (!((address >= 0x790000U && address <= 0x790200U - bytes) ||
                 (address >= XT_START && address <= XT_START + XT_SIZE - bytes))) return -1;
    xt_u32 status = 0;
    for (xt_u32 n = 0; n < XT_POLLS; ++n) {
        status = xt_read(4);
        if (!(status & (1U << 5))) break;
        if (n + 1 == XT_POLLS) return -1;
    }
    if (writing)
        for (xt_u32 i = 0; i < bytes / 4; ++i) xt_write(0x10 + i * 4, data[i]);
    xt_write(8, address);
    /* Match coreboot fast_spi_def.h: FCYCLE WRITE=2, READ=0; FDBC at bit24.
     * Clear only FDONE/FCERR/AEL W1C, preserving descriptor/lock policy bits. */
    xt_write(4, 7U | (1U << 16) | ((writing ? 2U : 0U) << 17) | ((bytes - 1) << 24));
    for (xt_u32 n = 0; n < XT_POLLS; ++n) {
        status = xt_read(4);
        if (status & 6U) return -1;
        if ((status & 1U) && !(status & (1U << 5))) {
            if (!writing)
                for (xt_u32 i = 0; i < bytes / 4; ++i) data[i] = xt_read(0x10 + i * 4);
            return 0;
        }
    }
    return -1;
}

/* Returns 0 after read-verified append, -1 on guard refusal, -2 on I/O error,
 * -3 when full, -4 after an earlier partial/failed transaction this boot. */
static int x380_early_event(xt_u32 stage, xt_u32 detail, xt_u32 arg0, xt_u32 arg1)
{
    xt_uptr flags;
    __asm__ volatile("pushf; pop %0; cli" : "=r"(flags) : : "memory");
    xt_u32 saved_cf8 = xt_in32(0xcf8), control = 0;
    int result = -1, wpd_changed = 0;
    xt_u32 buffer[16], record[8];
    if (xt_failed) { result = -4; goto out; }
    if (xt_cfg(0x80000000) != 0x59148086U ||
        xt_cfg(0x8000f800) != 0x9d4e8086U || xt_cfg(0x8000fa00) != 0x9d218086U ||
        xt_cfg(0x8000fd00) != 0x9d248086U || !(xt_cfg(0x8000fd04) & 2U) ||
        (xt_cfg(0x8000fd10) & ~0xfffU) != XT_BASE) goto out;
    control = xt_cfg(0x8000fddc);
    if ((control & (1U << 5)) || (!(control & 1U) && (control & 2U))) goto out;
    xt_u32 status = xt_read(4);
    if (!(status & (1U << 14)) || (status & (1U << 7)) ||
        (xt_read(0x50) & 0x202U) != 0x202U || xt_read(0x58) != 0x0fff0700U) goto out;
    for (xt_u32 i = 0; i < 5; ++i) {
        xt_u32 pr = xt_read(0x84 + i * 4);
        xt_u32 low = (pr & 0x7fffU) << 12;
        xt_u32 high = (((pr >> 16) & 0x7fffU) << 12) | 0xfffU;
        if ((pr & 0x80008000U) && low <= high && low < XT_START + XT_SIZE && high >= XT_START) goto out;
    }
    if (!xt_scanned) {
        /* Confirm the fixed destination exists exactly once in this image's
         * on-chip FMAP. Do not infer layout from the bridge's build alone. */
        xt_u8 fmap[512];
        for (xt_u32 i = 0; i < sizeof(fmap); i += 64) {
            if (xt_transfer(0x790000U + i, buffer, 64, 0)) goto io_error;
            for (xt_u32 j = 0; j < 64; ++j) fmap[i + j] = ((xt_u8 *)buffer)[j];
        }
        if (!xt_equal(fmap, "__FMAP__", 8) || fmap[8] != 1 ||
            xt_le32(fmap + 18) != 0x1000000U) goto out;
        xt_u32 count = fmap[54] | ((xt_u32)fmap[55] << 8), matches = 0;
        if (!count || count > 10) goto out;
        for (xt_u32 i = 0; i < count; ++i) {
            const xt_u8 *area = fmap + 56 + i * 42;
            if (!xt_equal(area + 8, "EARLY_TRACE", 12)) continue;
            if (xt_le32(area) != XT_START || xt_le32(area + 4) != XT_SIZE) goto out;
            ++matches;
        }
        if (matches != 1) goto out;
        xt_next = 0;
        for (xt_u32 at = 0; at < XT_SIZE; at += 64) {
            if (xt_transfer(XT_START + at, buffer, 64, 0)) goto io_error;
            for (xt_u32 j = 0; j < 64; ++j)
                if (((xt_u8 *)buffer)[j] != 0xff) xt_next = (at + j + 32) & ~31U;
        }
        xt_scanned = 1;
    }
    if (xt_next > XT_SIZE - 32) { result = -3; goto out; }
    /* Recheck the chosen slot immediately before writing; no NOR overwrite. */
    if (xt_transfer(XT_START + xt_next, buffer, 32, 0)) goto io_error;
    for (xt_u32 j = 0; j < 8; ++j) if (buffer[j] != 0xffffffffU) goto io_error;
    record[0] = 0x31525458U; record[1] = xt_next / 32; record[2] = stage;
    record[3] = detail; record[4] = arg0; record[5] = arg1; record[6] = control;
    record[7] = xt_hash(record, 28);
    if (!(control & 1U)) {
        wpd_changed = 1;
        xt_control(control | 1U);
        if (!(xt_cfg(0x8000fddc) & 1U)) goto io_error;
    }
    if (xt_transfer(XT_START + xt_next, record, 32, 1) ||
        xt_transfer(XT_START + xt_next, buffer, 32, 0)) goto io_error;
    for (xt_u32 j = 0; j < 8; ++j) if (buffer[j] != record[j]) goto io_error;
    xt_next += 32;
    result = 0;
    goto out;
io_error:
    xt_failed = 1;
    result = -2;
out:
    /* Restore only WPD if it was temporarily enabled, then original CF8/IF. */
    if (wpd_changed) xt_control(control);
    xt_out32(0xcf8, saved_cf8);
    __asm__ volatile("push %0; popf" : : "r"(flags) : "memory");
    return result;
}
#endif

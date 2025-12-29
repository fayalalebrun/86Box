/*
 * 86Box    A hypervisor and IBM PC system emulator that specializes in
 *          running old operating systems and software designed for IBM
 *          PC systems and compatibles from 1981 through fairly recent
 *          system designs based on the PCI bus.
 *
 *          This file is part of the 86Box distribution.
 *
 *          Voodoo command trace capture implementation.
 *
 * Authors: Claude Code Assistant
 *
 *          Copyright 2025.
 */

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <errno.h>
#define HAVE_STDARG_H
#include <86box/86box.h>
#include <86box/device.h>
#include <86box/mem.h>
#include <86box/timer.h>
#include <86box/thread.h>
#include <86box/video.h>
#include <86box/vid_svga.h>
#include <86box/vid_voodoo_common.h>
#include <86box/vid_voodoo_trace.h>

#ifdef ENABLE_VOODOO_TRACE_LOG
int voodoo_trace_do_log = ENABLE_VOODOO_TRACE_LOG;

static void
voodoo_trace_log(const char *fmt, ...)
{
    va_list ap;

    if (voodoo_trace_do_log) {
        va_start(ap, fmt);
        pclog_ex(fmt, ap);
        va_end(ap);
    }
}
#else
#    define voodoo_trace_log(fmt, ...)
#endif

/* Command type names for text output */
static const char *cmd_names[] = {
    "WREG_L",   /* 0: VOODOO_TRACE_WRITE_REG_L */
    "WREG_W",   /* 1: VOODOO_TRACE_WRITE_REG_W */
    "WFB_L",    /* 2: VOODOO_TRACE_WRITE_FB_L */
    "WFB_W",    /* 3: VOODOO_TRACE_WRITE_FB_W */
    "WTEX_L",   /* 4: VOODOO_TRACE_WRITE_TEX_L */
    "WCMD",     /* 5: VOODOO_TRACE_WRITE_CMDFIFO */
    "RREG_L",   /* 6: VOODOO_TRACE_READ_REG_L */
    "RREG_W",   /* 7: VOODOO_TRACE_READ_REG_W */
    "RFB_L",    /* 8: VOODOO_TRACE_READ_FB_L */
    "RFB_W",    /* 9: VOODOO_TRACE_READ_FB_W */
    "VSYNC",    /* 10: VOODOO_TRACE_VSYNC */
    "SWAP",     /* 11: VOODOO_TRACE_SWAP */
    "CONFIG",   /* 12: VOODOO_TRACE_CONFIG */
    "FRAME_END" /* 13: VOODOO_TRACE_FRAME_END */
};

/* Register name lookup table */
typedef struct {
    uint32_t addr;
    const char *name;
} reg_name_t;

static const reg_name_t reg_names[] = {
    {0x000, "status"}, {0x004, "intrCtrl"},
    {0x008, "vertexAx"}, {0x00c, "vertexAy"}, {0x010, "vertexBx"}, {0x014, "vertexBy"},
    {0x018, "vertexCx"}, {0x01c, "vertexCy"},
    {0x020, "startR"}, {0x024, "startG"}, {0x028, "startB"}, {0x02c, "startZ"},
    {0x030, "startA"}, {0x034, "startS"}, {0x038, "startT"}, {0x03c, "startW"},
    {0x040, "dRdX"}, {0x044, "dGdX"}, {0x048, "dBdX"}, {0x04c, "dZdX"},
    {0x050, "dAdX"}, {0x054, "dSdX"}, {0x058, "dTdX"}, {0x05c, "dWdX"},
    {0x060, "dRdY"}, {0x064, "dGdY"}, {0x068, "dBdY"}, {0x06c, "dZdY"},
    {0x070, "dAdY"}, {0x074, "dSdY"}, {0x078, "dTdY"}, {0x07c, "dWdY"},
    {0x080, "triangleCMD"},
    {0x088, "fvertexAx"}, {0x08c, "fvertexAy"}, {0x090, "fvertexBx"}, {0x094, "fvertexBy"},
    {0x098, "fvertexCx"}, {0x09c, "fvertexCy"},
    {0x0a0, "fstartR"}, {0x0a4, "fstartG"}, {0x0a8, "fstartB"}, {0x0ac, "fstartZ"},
    {0x0b0, "fstartA"}, {0x0b4, "fstartS"}, {0x0b8, "fstartT"}, {0x0bc, "fstartW"},
    {0x0c0, "fdRdX"}, {0x0c4, "fdGdX"}, {0x0c8, "fdBdX"}, {0x0cc, "fdZdX"},
    {0x0d0, "fdAdX"}, {0x0d4, "fdSdX"}, {0x0d8, "fdTdX"}, {0x0dc, "fdWdX"},
    {0x0e0, "fdRdY"}, {0x0e4, "fdGdY"}, {0x0e8, "fdBdY"}, {0x0ec, "fdZdY"},
    {0x0f0, "fdAdY"}, {0x0f4, "fdSdY"}, {0x0f8, "fdTdY"}, {0x0fc, "fdWdY"},
    {0x100, "ftriangleCMD"},
    {0x104, "fbzColorPath"}, {0x108, "fogMode"}, {0x10c, "alphaMode"},
    {0x110, "fbzMode"}, {0x114, "lfbMode"},
    {0x118, "clipLeftRight"}, {0x11c, "clipLowYHighY"},
    {0x120, "nopCMD"}, {0x124, "fastfillCMD"}, {0x128, "swapbufferCMD"},
    {0x12c, "fogColor"}, {0x130, "zaColor"}, {0x134, "chromaKey"},
    {0x13c, "userIntrCMD"}, {0x140, "stipple"}, {0x144, "color0"}, {0x148, "color1"},
    {0x14c, "fbiPixelsIn"}, {0x150, "fbiChromaFail"}, {0x154, "fbiZFuncFail"},
    {0x158, "fbiAFuncFail"}, {0x15c, "fbiPixelsOut"},
    {0x200, "fbiInit4"}, {0x204, "vRetrace"}, {0x208, "backPorch"}, {0x20c, "videoDimensions"},
    {0x210, "fbiInit0"}, {0x214, "fbiInit1"}, {0x218, "fbiInit2"}, {0x21c, "fbiInit3"},
    {0x220, "hSync"}, {0x224, "vSync"}, {0x228, "clutData"}, {0x22c, "dacData"},
    {0x230, "scrFilter"}, {0x240, "hvRetrace"},
    {0x244, "fbiInit5"}, {0x248, "fbiInit6"}, {0x24c, "fbiInit7"},
    {0x260, "sSetupMode"}, {0x264, "sVx"}, {0x268, "sVy"}, {0x26c, "sARGB"},
    {0x270, "sRed"}, {0x274, "sGreen"}, {0x278, "sBlue"}, {0x27c, "sAlpha"},
    {0x280, "sVz"}, {0x284, "sWb"}, {0x288, "sW0"}, {0x28c, "sS0"},
    {0x290, "sT0"}, {0x294, "sW1"}, {0x298, "sS1"}, {0x29c, "sT1"},
    {0x2a0, "sDrawTriCMD"}, {0x2a4, "sBeginTriCMD"},
    /* Blitter registers */
    {0x2c0, "bltSrcBaseAddr"}, {0x2c4, "bltDstBaseAddr"},
    {0x2c8, "bltXYStride"}, {0x2cc, "bltSrcChromaRange"},
    {0x2d0, "bltDstChromaRange"}, {0x2d4, "bltClipX"}, {0x2d8, "bltClipY"},
    {0x2e0, "bltSrcXY"}, {0x2e4, "bltDstXY"}, {0x2e8, "bltSize"},
    {0x2ec, "bltRop"}, {0x2f0, "bltColor"}, {0x2f8, "bltCommand"}, {0x2fc, "bltData"},
    /* TMU registers (TMU0 base, TMU1 is at +0x400 or selected via chip select) */
    {0x300, "textureMode"}, {0x304, "tLOD"}, {0x308, "tDetail"},
    {0x30c, "texBaseAddr"}, {0x310, "texBaseAddr1"}, {0x314, "texBaseAddr2"}, {0x318, "texBaseAddr38"},
    {0x320, "trexInit1"},
    /* NCC tables */
    {0x324, "nccTable0_Y0"}, {0x328, "nccTable0_Y1"}, {0x32c, "nccTable0_Y2"}, {0x330, "nccTable0_Y3"},
    {0x334, "nccTable0_I0"}, {0x338, "nccTable0_I1"}, {0x33c, "nccTable0_I2"}, {0x340, "nccTable0_I3"},
    {0x344, "nccTable0_Q0"}, {0x348, "nccTable0_Q1"}, {0x34c, "nccTable0_Q2"}, {0x350, "nccTable0_Q3"},
    {0x354, "nccTable1_Y0"}, {0x358, "nccTable1_Y1"}, {0x35c, "nccTable1_Y2"}, {0x360, "nccTable1_Y3"},
    {0x364, "nccTable1_I0"}, {0x368, "nccTable1_I1"}, {0x36c, "nccTable1_I2"}, {0x370, "nccTable1_I3"},
    {0x374, "nccTable1_Q0"}, {0x378, "nccTable1_Q1"}, {0x37c, "nccTable1_Q2"}, {0x380, "nccTable1_Q3"},
    {0, NULL}
};

/* Remapped register names when fbiInit3(0)=1 and pci_ad[21]=1
 * Layout groups each parameter with its gradients: start, dX, dY
 * Order is RGBZASTW as documented in datasheet v1.40 */
static const reg_name_t remap_reg_names[] = {
    /* Integer registers (0x00-0x7c remapped) */
    {0x008, "vertexAx"}, {0x00c, "vertexAy"},
    {0x010, "vertexBx"}, {0x014, "vertexBy"},
    {0x018, "vertexCx"}, {0x01c, "vertexCy"},
    {0x020, "startR"}, {0x024, "dRdX"}, {0x028, "dRdY"},
    {0x02c, "startG"}, {0x030, "dGdX"}, {0x034, "dGdY"},
    {0x038, "startB"}, {0x03c, "dBdX"}, {0x040, "dBdY"},
    {0x044, "startZ"}, {0x048, "dZdX"}, {0x04c, "dZdY"},
    {0x050, "startA"}, {0x054, "dAdX"}, {0x058, "dAdY"},
    {0x05c, "startS"}, {0x060, "dSdX"}, {0x064, "dSdY"},
    {0x068, "startT"}, {0x06c, "dTdX"}, {0x070, "dTdY"},
    {0x074, "startW"}, {0x078, "dWdX"}, {0x07c, "dWdY"},
    {0x080, "triangleCMD"},
    /* Float registers (0x88-0xfc remapped) */
    {0x088, "fvertexAx"}, {0x08c, "fvertexAy"},
    {0x090, "fvertexBx"}, {0x094, "fvertexBy"},
    {0x098, "fvertexCx"}, {0x09c, "fvertexCy"},
    {0x0a0, "fstartR"}, {0x0a4, "fdRdX"}, {0x0a8, "fdRdY"},
    {0x0ac, "fstartG"}, {0x0b0, "fdGdX"}, {0x0b4, "fdGdY"},
    {0x0b8, "fstartB"}, {0x0bc, "fdBdX"}, {0x0c0, "fdBdY"},
    {0x0c4, "fstartZ"}, {0x0c8, "fdZdX"}, {0x0cc, "fdZdY"},
    {0x0d0, "fstartA"}, {0x0d4, "fdAdX"}, {0x0d8, "fdAdY"},
    {0x0dc, "fstartS"}, {0x0e0, "fdSdX"}, {0x0e4, "fdSdY"},
    {0x0e8, "fstartT"}, {0x0ec, "fdTdX"}, {0x0f0, "fdTdY"},
    {0x0f4, "fstartW"}, {0x0f8, "fdWdX"}, {0x0fc, "fdWdY"},
    {0, NULL}
};

static const char *
get_register_name(uint32_t addr)
{
    static char unknown[16];
    uint32_t reg_addr = addr & 0x3fc;
    int use_remap = (addr & 0x200000) && (reg_addr < 0x100);
    const reg_name_t *table = use_remap ? remap_reg_names : reg_names;

    for (int i = 0; table[i].name != NULL; i++) {
        if (table[i].addr == reg_addr)
            return table[i].name;
    }

    snprintf(unknown, sizeof(unknown), "0x%03x", reg_addr);
    return unknown;
}

/* Fixed-point format types */
typedef enum {
    FP_NONE = 0,      /* Not a fixed-point register */
    FP_S12_4,         /* Signed 12.4 (vertex coords) */
    FP_S18_14,        /* Signed 18.14 (color values) - actually uses full 32-bit range */
    FP_S20_12,        /* Signed 20.12 (depth) */
    FP_S14_18,        /* Signed 14.18 (texture S/T) */
    FP_S2_30,         /* Signed 2.30 (W value) */
    FP_IEEE754        /* IEEE 754 single precision float */
} fixed_point_type_t;

/* Get fixed-point format for a remapped register address */
static fixed_point_type_t
get_fixed_point_format(uint32_t addr)
{
    uint32_t reg_addr = addr & 0x3fc;
    int use_remap = (addr & 0x200000) && (reg_addr < 0x100);

    if (!use_remap)
        return FP_NONE;

    /* Integer fixed-point registers (0x008-0x07c) */
    if (reg_addr >= 0x008 && reg_addr <= 0x01c)
        return FP_S12_4;   /* vertexAx/Ay, vertexBx/By, vertexCx/Cy */
    if (reg_addr >= 0x020 && reg_addr <= 0x028)
        return FP_S18_14;  /* startR, dRdX, dRdY */
    if (reg_addr >= 0x02c && reg_addr <= 0x034)
        return FP_S18_14;  /* startG, dGdX, dGdY */
    if (reg_addr >= 0x038 && reg_addr <= 0x040)
        return FP_S18_14;  /* startB, dBdX, dBdY */
    if (reg_addr >= 0x044 && reg_addr <= 0x04c)
        return FP_S20_12;  /* startZ, dZdX, dZdY */
    if (reg_addr >= 0x050 && reg_addr <= 0x058)
        return FP_S18_14;  /* startA, dAdX, dAdY */
    if (reg_addr >= 0x05c && reg_addr <= 0x064)
        return FP_S14_18;  /* startS, dSdX, dSdY */
    if (reg_addr >= 0x068 && reg_addr <= 0x070)
        return FP_S14_18;  /* startT, dTdX, dTdY */
    if (reg_addr >= 0x074 && reg_addr <= 0x07c)
        return FP_S2_30;   /* startW, dWdX, dWdY */

    /* IEEE 754 float registers (0x088-0x0fc) */
    if (reg_addr >= 0x088 && reg_addr <= 0x0fc)
        return FP_IEEE754;

    return FP_NONE;
}

/* Convert fixed-point value to float, returns 1 if conversion was done */
static int
fixed_point_to_float(uint32_t value, fixed_point_type_t type, double *result)
{
    int32_t signed_val = (int32_t)value;

    switch (type) {
        case FP_S12_4:
            *result = signed_val / 16.0;
            return 1;
        case FP_S18_14:
            *result = signed_val / 16384.0;  /* 2^14 */
            return 1;
        case FP_S20_12:
            *result = signed_val / 4096.0;   /* 2^12 */
            return 1;
        case FP_S14_18:
            *result = signed_val / 262144.0; /* 2^18 */
            return 1;
        case FP_S2_30:
            *result = signed_val / 1073741824.0; /* 2^30 */
            return 1;
        case FP_IEEE754: {
            /* Interpret as IEEE 754 single precision */
            union {
                uint32_t u;
                float f;
            } conv;
            conv.u = value;
            *result = conv.f;
            return 1;
        }
        default:
            return 0;
    }
}

/* Helper to open a new text trace file for a specific frame */
static void
voodoo_trace_open_text_file(voodoo_trace_t *trace, uint32_t frame_num)
{
    char txt_filename[512];

    if (!trace || !trace->text_mode)
        return;

    /* Close previous text file if open */
    if (trace->txt_file) {
        fprintf(trace->txt_file, "# End of frame %u\n", trace->current_text_frame);
        fclose(trace->txt_file);
        trace->txt_file = NULL;
    }

    /* Open new text file for this frame */
    snprintf(txt_filename, sizeof(txt_filename), "%s/frame_%04u.txt", trace->trace_dir, frame_num);
    trace->txt_file = fopen(txt_filename, "w");
    if (trace->txt_file) {
        fprintf(trace->txt_file, "# Voodoo Trace - Frame %u\n", frame_num);
        fprintf(trace->txt_file, "# Format: <CMD> <timestamp> <addr> [register_name] <data> [(float_value)] [xN]\n");
        fprintf(trace->txt_file, "# Fixed-point formats: S12.4 (vertices), S18.14 (RGBA), S20.12 (Z), S14.18 (S/T), S2.30 (W)\n");
        fprintf(trace->txt_file, "#\n");
        trace->current_text_frame = frame_num;
    }
}

void
voodoo_trace_init(voodoo_trace_t *trace, const char *filename,
                  const voodoo_trace_header_t *header, int text_mode,
                  voodoo_t *voodoo)
{
    char bin_filename[512];
    char idx_filename[512];
    time_t now;
    const char *base_name;
    char *last_slash;

    if (!trace || !filename)
        return;

    memset(trace, 0, sizeof(voodoo_trace_t));

    /* Store voodoo pointer for state dumps */
    trace->voodoo = voodoo;

    /* Extract base name from filename (strip path and .bin extension) */
    last_slash = strrchr(filename, '/');
    if (!last_slash)
        last_slash = strrchr(filename, '\\');
    base_name = last_slash ? last_slash + 1 : filename;

    /* Create trace directory name from base filename */
    strncpy(trace->trace_dir, filename, sizeof(trace->trace_dir) - 1);
    trace->trace_dir[sizeof(trace->trace_dir) - 1] = '\0';
    char *ext = strrchr(trace->trace_dir, '.');
    if (ext && strcmp(ext, ".bin") == 0)
        *ext = '\0';
    strncat(trace->trace_dir, ".trace", sizeof(trace->trace_dir) - strlen(trace->trace_dir) - 1);

    /* Create trace directory */
#ifdef _WIN32
    mkdir(trace->trace_dir);
#else
    if (mkdir(trace->trace_dir, 0755) != 0 && errno != EEXIST) {
        pclog("voodoo_trace: Failed to create trace directory: %s\n", trace->trace_dir);
        return;
    }
#endif
    pclog("voodoo_trace: Using trace directory: %s\n", trace->trace_dir);

    /* Store base path for frame dumps */
    snprintf(trace->base_filename, sizeof(trace->base_filename), "%s/frame", trace->trace_dir);

    /* Build filenames inside the trace directory */
    snprintf(bin_filename, sizeof(bin_filename), "%s/trace.bin", trace->trace_dir);
    snprintf(idx_filename, sizeof(idx_filename), "%s/index.csv", trace->trace_dir);

    /* Open binary trace file */
    trace->bin_file = fopen(bin_filename, "wb");
    if (!trace->bin_file) {
        pclog("voodoo_trace: Failed to open binary trace file: %s\n", bin_filename);
        return;
    }

    /* Write header */
    if (fwrite(header, sizeof(voodoo_trace_header_t), 1, trace->bin_file) != 1) {
        pclog("voodoo_trace: Failed to write header\n");
        fclose(trace->bin_file);
        trace->bin_file = NULL;
        return;
    }

    /* Store text mode flag and open first frame's text file if requested */
    trace->text_mode = text_mode;
    if (text_mode) {
        voodoo_trace_open_text_file(trace, 0);
    }

    trace->start_time = (uint32_t)tsc;
    trace->entry_count = 0;
    trace->last_frame = 0;
    trace->max_frames = voodoo_trace_max_frames;
    trace->capture_reads = 1;  /* Can be configured */
    trace->capture_fb = 1;     /* Can be configured */
    trace->capture_vsync = 0;  /* Disabled by default to reduce spam */
    trace->capture_enabled = 1;
    trace->has_pending = 0;

    /* Initialize frame index tracking - CSV format */
    trace->idx_file = fopen(idx_filename, "w");
    if (trace->idx_file) {
        fprintf(trace->idx_file, "frame,start_cycle,end_cycle,start_offset,end_offset,commands,triangles,h_disp,v_disp,duration\n");
    } else {
        pclog("voodoo_trace: Failed to open frame index file: %s\n", idx_filename);
    }

    trace->frame_start_offset = ftell(trace->bin_file);
    trace->frame_start_cycle = 0;
    trace->frame_cmd_count = 0;

    trace->dump_frames = 1;  /* Enable frame dumps by default */
    trace->dump_state = 1;   /* Enable state dumps by default */

    /* Dump initial state for frame 0 */
    if (trace->dump_state && trace->voodoo) {
        voodoo_trace_dump_state(trace, 0);
    }

    voodoo_trace_log("Voodoo trace initialized: %s (text=%d, max_frames=%u)\n",
                     trace->trace_dir, text_mode, trace->max_frames);
}

static void
voodoo_trace_flush_pending(voodoo_trace_t *trace)
{
    if (!trace || !trace->has_pending)
        return;

    /* Write binary entry */
    if (trace->bin_file) {
        if (fwrite(&trace->pending_entry, sizeof(voodoo_trace_entry_t), 1, trace->bin_file) != 1) {
            pclog("voodoo_trace: Failed to write entry\n");
            return;
        }
    }

    /* Write text entry if enabled */
    if (trace->txt_file && trace->pending_entry.cmd_type < (sizeof(cmd_names) / sizeof(cmd_names[0]))) {
        const char *reg_name = NULL;
        fixed_point_type_t fp_type = FP_NONE;
        double fp_value = 0.0;
        int has_fp = 0;

        /* Add register name for register accesses */
        if (trace->pending_entry.cmd_type == VOODOO_TRACE_WRITE_REG_L ||
            trace->pending_entry.cmd_type == VOODOO_TRACE_WRITE_REG_W ||
            trace->pending_entry.cmd_type == VOODOO_TRACE_READ_REG_L ||
            trace->pending_entry.cmd_type == VOODOO_TRACE_READ_REG_W) {
            reg_name = get_register_name(trace->pending_entry.addr);
            /* Check if this register has a fixed-point format */
            fp_type = get_fixed_point_format(trace->pending_entry.addr);
            has_fp = fixed_point_to_float(trace->pending_entry.data, fp_type, &fp_value);
        }

        if (trace->pending_entry.count > 1) {
            if (reg_name) {
                if (has_fp) {
                    fprintf(trace->txt_file, "%-10s %12llu-%12llu 0x%06x %-16s 0x%08x (%.6f) x%u\n",
                            cmd_names[trace->pending_entry.cmd_type],
                            (unsigned long long)trace->pending_entry.timestamp,
                            (unsigned long long)trace->pending_entry.timestamp_end,
                            trace->pending_entry.addr,
                            reg_name,
                            trace->pending_entry.data,
                            fp_value,
                            trace->pending_entry.count);
                } else {
                    fprintf(trace->txt_file, "%-10s %12llu-%12llu 0x%06x %-16s 0x%08x x%u\n",
                            cmd_names[trace->pending_entry.cmd_type],
                            (unsigned long long)trace->pending_entry.timestamp,
                            (unsigned long long)trace->pending_entry.timestamp_end,
                            trace->pending_entry.addr,
                            reg_name,
                            trace->pending_entry.data,
                            trace->pending_entry.count);
                }
            } else {
                fprintf(trace->txt_file, "%-10s %12llu-%12llu 0x%06x 0x%08x x%u\n",
                        cmd_names[trace->pending_entry.cmd_type],
                        (unsigned long long)trace->pending_entry.timestamp,
                        (unsigned long long)trace->pending_entry.timestamp_end,
                        trace->pending_entry.addr,
                        trace->pending_entry.data,
                        trace->pending_entry.count);
            }
        } else {
            if (reg_name) {
                if (has_fp) {
                    fprintf(trace->txt_file, "%-10s %12llu 0x%06x %-16s 0x%08x (%.6f)\n",
                            cmd_names[trace->pending_entry.cmd_type],
                            (unsigned long long)trace->pending_entry.timestamp,
                            trace->pending_entry.addr,
                            reg_name,
                            trace->pending_entry.data,
                            fp_value);
                } else {
                    fprintf(trace->txt_file, "%-10s %12llu 0x%06x %-16s 0x%08x\n",
                            cmd_names[trace->pending_entry.cmd_type],
                            (unsigned long long)trace->pending_entry.timestamp,
                            trace->pending_entry.addr,
                            reg_name,
                            trace->pending_entry.data);
                }
            } else {
                fprintf(trace->txt_file, "%-10s %12llu 0x%06x 0x%08x\n",
                        cmd_names[trace->pending_entry.cmd_type],
                        (unsigned long long)trace->pending_entry.timestamp,
                        trace->pending_entry.addr,
                        trace->pending_entry.data);
            }
        }
    }

    trace->entry_count++;
    trace->has_pending = 0;
}

void
voodoo_trace_close(voodoo_trace_t *trace)
{
    if (!trace)
        return;

    /* Flush any pending entry */
    voodoo_trace_flush_pending(trace);

    if (trace->bin_file) {
        /* Update entry count in header */
        fseek(trace->bin_file, offsetof(voodoo_trace_header_t, entry_count), SEEK_SET);
        fwrite(&trace->entry_count, sizeof(uint32_t), 1, trace->bin_file);

        fclose(trace->bin_file);
        trace->bin_file = NULL;
    }

    if (trace->txt_file) {
        fprintf(trace->txt_file, "\n# Total entries: %llu\n",
                (unsigned long long)trace->entry_count);
        fclose(trace->txt_file);
        trace->txt_file = NULL;
    }

    /* Close frame index CSV file */
    if (trace->idx_file) {
        fclose(trace->idx_file);
        trace->idx_file = NULL;
    }

    voodoo_trace_log("Voodoo trace closed: %llu entries\n",
                     (unsigned long long)trace->entry_count);
}

static void
voodoo_trace_write_entry(voodoo_trace_t *trace, const voodoo_trace_entry_t *entry)
{
    if (!trace || !trace->bin_file)
        return;

    /* Count commands for frame index */
    if (trace->capture_enabled)
        trace->frame_cmd_count++;

    /* Normal trace path (only if capture enabled) */
    if (!trace->capture_enabled)
        return;

    /* Check if this entry can be coalesced with the pending one */
    if (trace->has_pending &&
        trace->pending_entry.cmd_type == entry->cmd_type &&
        trace->pending_entry.addr == entry->addr &&
        trace->pending_entry.data == entry->data) {
        /* Same command - increment count and update end timestamp */
        trace->pending_entry.count++;
        trace->pending_entry.timestamp_end = entry->timestamp;
        return;
    }

    /* Different command - flush pending and start new one */
    voodoo_trace_flush_pending(trace);

    /* Buffer this entry as pending */
    trace->pending_entry = *entry;
    trace->pending_entry.timestamp_end = entry->timestamp;
    trace->pending_entry.count = 1;
    trace->has_pending = 1;

    /* Periodic flush for debugging */
    if ((trace->entry_count % 1000) == 0) {
        voodoo_trace_flush_pending(trace);
        fflush(trace->bin_file);
        if (trace->txt_file)
            fflush(trace->txt_file);
    }
}

void
voodoo_trace_write(voodoo_trace_t *trace, voodoo_trace_cmd_t cmd_type,
                   uint32_t addr, uint32_t data)
{
    voodoo_trace_entry_t entry = {0};

    if (!trace || !trace->bin_file)
        return;

    /* Skip if capture disabled (frame limit reached) */
    if (!trace->capture_enabled)
        return;

    /* Skip framebuffer writes if not capturing */
    if (!trace->capture_fb &&
        (cmd_type == VOODOO_TRACE_WRITE_FB_L ||
         cmd_type == VOODOO_TRACE_WRITE_FB_W))
        return;

    entry.timestamp = (uint32_t)tsc - trace->start_time;
    entry.timestamp_end = 0;
    entry.cmd_type = cmd_type;
    entry.reserved[0] = 0;
    entry.reserved[1] = 0;
    entry.reserved[2] = 0;
    entry.addr = addr;
    entry.data = data;
    entry.count = 1;

    voodoo_trace_write_entry(trace, &entry);
}

void
voodoo_trace_read(voodoo_trace_t *trace, voodoo_trace_cmd_t cmd_type,
                  uint32_t addr, uint32_t data)
{
    voodoo_trace_entry_t entry = {0};

    if (!trace || !trace->bin_file || !trace->capture_reads)
        return;

    /* Skip if capture disabled (frame limit reached) */
    if (!trace->capture_enabled)
        return;

    /* Skip framebuffer reads if not capturing */
    if (!trace->capture_fb &&
        (cmd_type == VOODOO_TRACE_READ_FB_L ||
         cmd_type == VOODOO_TRACE_READ_FB_W))
        return;

    entry.timestamp = (uint32_t)tsc - trace->start_time;
    entry.timestamp_end = 0;
    entry.cmd_type = cmd_type;
    entry.reserved[0] = 0;
    entry.reserved[1] = 0;
    entry.reserved[2] = 0;
    entry.addr = addr;
    entry.data = data;  /* Store actual read result for validation */
    entry.count = 1;

    voodoo_trace_write_entry(trace, &entry);
}

void
voodoo_trace_vsync(voodoo_trace_t *trace, uint32_t frame_num, uint32_t resolution)
{
    voodoo_trace_entry_t entry = {0};

    if (!trace || !trace->bin_file)
        return;

    /* VSync events are just informational, no frame tracking here */
    if (!trace->capture_vsync || !trace->capture_enabled)
        return;

    entry.timestamp = (uint32_t)tsc - trace->start_time;
    entry.timestamp_end = 0;
    entry.cmd_type = VOODOO_TRACE_VSYNC;
    entry.reserved[0] = 0;
    entry.reserved[1] = 0;
    entry.reserved[2] = 0;
    entry.addr = frame_num;
    entry.data = resolution;  /* (h_disp << 16) | v_disp */
    entry.count = 1;

    voodoo_trace_write_entry(trace, &entry);
    trace->last_frame = frame_num;

    voodoo_trace_log("Voodoo trace: Frame %u @ %ux%u\n",
                     frame_num, resolution >> 16, resolution & 0xFFFF);
}

void
voodoo_trace_swap(voodoo_trace_t *trace, uint32_t swap_offset, uint32_t frame_num, uint32_t resolution)
{
    voodoo_trace_entry_t entry = {0};
    extern int tris;  /* Global triangle counter from vid_voodoo.c */

    if (!trace || !trace->bin_file)
        return;

    /* Write frame index CSV entry for any frame with PCI communication */
    if (trace->idx_file && trace->capture_enabled) {
        uint64_t current_offset = ftell(trace->bin_file);

        /* Only record if there was any PCI activity (file position changed) */
        if (current_offset > trace->frame_start_offset) {
            uint64_t start_cycle = trace->frame_start_cycle;
            uint64_t end_cycle = (uint32_t)tsc - trace->start_time;
            uint64_t duration = end_cycle - start_cycle;
            uint32_t h_disp = (resolution >> 16) & 0xFFFF;
            uint32_t v_disp = resolution & 0xFFFF;

            fprintf(trace->idx_file, "%u,%llu,%llu,%llu,%llu,%u,%u,%u,%u,%llu\n",
                   frame_num,
                   (unsigned long long)start_cycle,
                   (unsigned long long)end_cycle,
                   (unsigned long long)trace->frame_start_offset,
                   (unsigned long long)current_offset,
                   trace->frame_cmd_count,
                   tris,
                   h_disp,
                   v_disp,
                   (unsigned long long)duration);
            fflush(trace->idx_file);
        }
    }

    /* Reset for next frame */
    if (trace->capture_enabled) {
        trace->frame_start_offset = ftell(trace->bin_file);
        trace->frame_start_cycle = (uint32_t)tsc - trace->start_time;
        trace->frame_cmd_count = 0;
    }

    /* Check frame limit */
    if (trace->max_frames > 0 && frame_num >= trace->max_frames && trace->capture_enabled) {
        trace->capture_enabled = 0;
        voodoo_trace_flush(trace);
        pclog("Voodoo trace: Frame limit reached (%u frames), capture stopped\n", frame_num);
    }

    /* Write swap event to trace */
    entry.timestamp = (uint32_t)tsc - trace->start_time;
    entry.timestamp_end = 0;
    entry.cmd_type = VOODOO_TRACE_SWAP;
    entry.reserved[0] = 0;
    entry.reserved[1] = 0;
    entry.reserved[2] = 0;
    entry.addr = swap_offset;
    entry.data = 0;
    entry.count = 1;

    voodoo_trace_write_entry(trace, &entry);
    trace->last_frame = frame_num;

    /* Open new text file for next frame (flush pending entries first) */
    if (trace->text_mode && trace->capture_enabled) {
        voodoo_trace_flush_pending(trace);
        voodoo_trace_open_text_file(trace, frame_num + 1);
    }

    /* Dump state for next frame (state at frame start) */
    if (trace->dump_state && trace->capture_enabled) {
        voodoo_trace_dump_state(trace, frame_num + 1);
    }
}

void
voodoo_trace_config(voodoo_trace_t *trace,
                    uint32_t h_disp, uint32_t v_disp,
                    uint32_t refresh_hz, uint32_t pixel_clock_hz)
{
    voodoo_trace_config_t config;

    if (!trace || !trace->bin_file)
        return;

    memset(&config, 0, sizeof(config));
    config.timestamp = (uint32_t)tsc - trace->start_time;
    config.cmd_type = VOODOO_TRACE_CONFIG;
    config.h_disp = h_disp;
    config.v_disp = v_disp;
    config.refresh_hz = refresh_hz;
    config.pixel_clock_hz = pixel_clock_hz;

    /* Write extended config entry */
    if (fwrite(&config, sizeof(voodoo_trace_config_t), 1, trace->bin_file) != 1) {
        pclog("voodoo_trace: Failed to write config entry\n");
        return;
    }

    if (trace->txt_file) {
        fprintf(trace->txt_file, "%-10s %12llu %ux%u@%uHz (%u Hz pixel clock)\n",
                "CONFIG",
                (unsigned long long)config.timestamp,
                h_disp, v_disp, refresh_hz, pixel_clock_hz);
    }

    trace->entry_count++;

    voodoo_trace_log("Voodoo trace: Config %ux%u@%uHz\n", h_disp, v_disp, refresh_hz);
}

void
voodoo_trace_flush(voodoo_trace_t *trace)
{
    if (!trace)
        return;

    voodoo_trace_flush_pending(trace);

    if (trace->bin_file)
        fflush(trace->bin_file);

    if (trace->txt_file)
        fflush(trace->txt_file);
}

void
voodoo_trace_dump_state(voodoo_trace_t *trace, uint32_t frame_num)
{
    char filename[512];
    FILE *fp;
    voodoo_state_header_t header;
    voodoo_state_reg_t reg;
    voodoo_t *voodoo;
    uint32_t fb_size, tex_size;
    int num_tmus;

    if (!trace || !trace->voodoo || !trace->dump_state)
        return;

    /* Skip if capture disabled (frame limit reached) */
    if (!trace->capture_enabled)
        return;

    voodoo = trace->voodoo;

    /* Create state dump filename */
    snprintf(filename, sizeof(filename), "%s/state_%04u.bin", trace->trace_dir, frame_num);

    fp = fopen(filename, "wb");
    if (!fp) {
        pclog("voodoo_trace: Failed to open state dump file: %s\n", filename);
        return;
    }

    /* Calculate sizes */
    fb_size = voodoo->fb_size * 1024 * 1024;
    tex_size = voodoo->texture_size * 1024 * 1024;
    num_tmus = voodoo->dual_tmus ? 2 : 1;

    /* Write header */
    memset(&header, 0, sizeof(header));
    header.magic = VOODOO_STATE_MAGIC;
    header.version = VOODOO_STATE_VERSION;
    header.frame_num = frame_num;
    header.voodoo_type = voodoo->type;
    header.fb_size = fb_size;
    header.tex_size = tex_size;
    header.num_tmus = num_tmus;
    header.reg_count = 0;  /* Will update after writing registers */
    header.flags = 0;
    /* Framebuffer layout info for conversion */
    header.row_width = (uint32_t)voodoo->row_width;
    header.draw_offset = voodoo->back_offset;
    header.aux_offset = voodoo->params.aux_offset;
    header.h_disp = (uint32_t)voodoo->h_disp;
    header.v_disp = (uint32_t)voodoo->v_disp;

    /* Reserve space for header, will rewrite after counting registers */
    long header_pos = ftell(fp);
    fwrite(&header, sizeof(header), 1, fp);

    /* Helper to write a register entry */
    #define WRITE_STATE_REG(a, v) do { \
        reg.addr = (a); reg.value = (v); \
        fwrite(&reg, sizeof(reg), 1, fp); \
        header.reg_count++; \
    } while(0)

    /* Write registers - fbiInit0-7 */
    WRITE_STATE_REG(0x210, voodoo->fbiInit0);
    WRITE_STATE_REG(0x214, voodoo->fbiInit1);
    WRITE_STATE_REG(0x218, voodoo->fbiInit2);
    WRITE_STATE_REG(0x21c, voodoo->fbiInit3);
    WRITE_STATE_REG(0x200, voodoo->fbiInit4);
    WRITE_STATE_REG(0x244, voodoo->fbiInit5);
    WRITE_STATE_REG(0x248, voodoo->fbiInit6);
    WRITE_STATE_REG(0x24c, voodoo->fbiInit7);

    /* Other FBI registers */
    WRITE_STATE_REG(0x114, voodoo->lfbMode);
    WRITE_STATE_REG(0x208, voodoo->backPorch);
    WRITE_STATE_REG(0x20c, voodoo->videoDimensions);
    WRITE_STATE_REG(0x220, voodoo->hSync);
    WRITE_STATE_REG(0x224, voodoo->vSync);

    /* Buffer offsets */
    WRITE_STATE_REG(0x300, voodoo->front_offset);
    WRITE_STATE_REG(0x304, voodoo->back_offset);
    WRITE_STATE_REG(0x308, voodoo->fb_read_offset);
    WRITE_STATE_REG(0x30c, voodoo->fb_write_offset);
    WRITE_STATE_REG(0x310, (uint32_t)voodoo->row_width);
    WRITE_STATE_REG(0x314, (uint32_t)voodoo->aux_row_width);

    /* TMU config */
    WRITE_STATE_REG(0x318, voodoo->trexInit1[0]);
    WRITE_STATE_REG(0x31c, voodoo->trexInit1[1]);
    WRITE_STATE_REG(0x320, voodoo->tmuConfig);

    /* Rendering state from params */
    WRITE_STATE_REG(0x110, voodoo->params.fbzMode);
    WRITE_STATE_REG(0x104, voodoo->params.fbzColorPath);
    WRITE_STATE_REG(0x108, voodoo->params.fogMode);
    WRITE_STATE_REG(0x10c, voodoo->params.alphaMode);
    WRITE_STATE_REG(0x187, voodoo->params.textureMode[0]);
    WRITE_STATE_REG(0x188, voodoo->params.textureMode[1]);
    WRITE_STATE_REG(0x190, voodoo->params.texBaseAddr[0]);
    WRITE_STATE_REG(0x191, voodoo->params.texBaseAddr[1]);

    /* Clipping */
    WRITE_STATE_REG(0x118, (uint32_t)((voodoo->params.clipRight << 16) | voodoo->params.clipLeft));
    WRITE_STATE_REG(0x11c, (uint32_t)((voodoo->params.clipHighY << 16) | voodoo->params.clipLowY));

    /* Colors */
    WRITE_STATE_REG(0x130, voodoo->params.zaColor);
    WRITE_STATE_REG(0x134, voodoo->params.chromaKey);
    WRITE_STATE_REG(0x12c, (uint32_t)((voodoo->params.fogColor.r << 16) |
                                       (voodoo->params.fogColor.g << 8) |
                                        voodoo->params.fogColor.b));
    WRITE_STATE_REG(0x144, voodoo->params.color0);
    WRITE_STATE_REG(0x148, voodoo->params.color1);

    #undef WRITE_STATE_REG

    /* Update header with final register count */
    long data_pos = ftell(fp);
    fseek(fp, header_pos, SEEK_SET);
    fwrite(&header, sizeof(header), 1, fp);
    fseek(fp, data_pos, SEEK_SET);

    /* Write framebuffer memory */
    if (voodoo->fb_mem && fb_size > 0) {
        fwrite(voodoo->fb_mem, 1, fb_size, fp);
    }

    /* Write texture memory for each TMU */
    if (voodoo->tex_mem[0] && tex_size > 0) {
        fwrite(voodoo->tex_mem[0], 1, tex_size, fp);
    }
    if (num_tmus > 1 && voodoo->tex_mem[1] && tex_size > 0) {
        fwrite(voodoo->tex_mem[1], 1, tex_size, fp);
    }

    fclose(fp);

    voodoo_trace_log("Voodoo trace: Dumped state for frame %u (%u regs, %u MB FB, %u MB TEX x%d) to %s\n",
                     frame_num, header.reg_count, fb_size / (1024*1024),
                     tex_size / (1024*1024), num_tmus, filename);
}

void
voodoo_trace_dump_framebuffer(voodoo_trace_t *trace,
                              const uint8_t *fb_mem, uint32_t fb_offset,
                              uint32_t h_disp, uint32_t v_disp,
                              uint32_t row_width, uint32_t frame_num)
{
    char filename[512];
    FILE *fp;

    if (!trace || !fb_mem || !trace->dump_frames)
        return;

    /* Skip if capture disabled (frame limit reached) */
    if (!trace->capture_enabled)
        return;

    /* Create frame dump filename: base_NNNN.bmp (base_filename already includes "frame" prefix) */
    snprintf(filename, sizeof(filename), "%s_%04u.bmp", trace->base_filename, frame_num);

    fp = fopen(filename, "wb");
    if (!fp) {
        pclog("voodoo_trace: Failed to open frame dump file: %s\n", filename);
        return;
    }

    /* BMP file format (24-bit RGB, bottom-up) */
    uint32_t row_bytes = ((h_disp * 3 + 3) / 4) * 4;  /* Rows must be 4-byte aligned */
    uint32_t pixel_data_size = row_bytes * v_disp;
    uint32_t file_size = 54 + pixel_data_size;  /* 54 = header size */

    /* BMP file header (14 bytes) */
    uint8_t bmp_file_header[14] = {
        'B', 'M',                           /* Signature */
        (uint8_t)(file_size), (uint8_t)(file_size >> 8),
        (uint8_t)(file_size >> 16), (uint8_t)(file_size >> 24),  /* File size */
        0, 0, 0, 0,                          /* Reserved */
        54, 0, 0, 0                          /* Pixel data offset */
    };
    fwrite(bmp_file_header, 1, 14, fp);

    /* BMP info header (40 bytes) */
    uint8_t bmp_info_header[40] = {
        40, 0, 0, 0,                         /* Header size */
        (uint8_t)(h_disp), (uint8_t)(h_disp >> 8),
        (uint8_t)(h_disp >> 16), (uint8_t)(h_disp >> 24),  /* Width */
        (uint8_t)(v_disp), (uint8_t)(v_disp >> 8),
        (uint8_t)(v_disp >> 16), (uint8_t)(v_disp >> 24),  /* Height (positive = bottom-up) */
        1, 0,                                /* Color planes */
        24, 0,                               /* Bits per pixel */
        0, 0, 0, 0,                          /* Compression (none) */
        (uint8_t)(pixel_data_size), (uint8_t)(pixel_data_size >> 8),
        (uint8_t)(pixel_data_size >> 16), (uint8_t)(pixel_data_size >> 24),  /* Image size */
        0, 0, 0, 0,                          /* X pixels per meter */
        0, 0, 0, 0,                          /* Y pixels per meter */
        0, 0, 0, 0,                          /* Colors in color table */
        0, 0, 0, 0                           /* Important colors */
    };
    fwrite(bmp_info_header, 1, 40, fp);

    /* Write pixel data (bottom-up, BGR order)
     * Voodoo framebuffer is RGB565: RRRRRGGGGGGBBBBB
     * BMP uses BGR888
     * Note: row_width is in bytes, convert to pixels (16-bit) */
    const uint16_t *fb = (const uint16_t *)(fb_mem + fb_offset);
    uint32_t row_stride = row_width / 2;  /* Convert bytes to 16-bit pixel count */
    uint8_t *row_buf = (uint8_t *)malloc(row_bytes);

    for (int32_t y = v_disp - 1; y >= 0; y--) {  /* Bottom-up */
        for (uint32_t x = 0; x < h_disp; x++) {
            uint16_t pixel = fb[y * row_stride + x];
            /* RGB565 to BGR888 */
            uint8_t r = ((pixel >> 11) & 0x1F) * 255 / 31;
            uint8_t g = ((pixel >> 5) & 0x3F) * 255 / 63;
            uint8_t b = (pixel & 0x1F) * 255 / 31;
            row_buf[x * 3 + 0] = b;
            row_buf[x * 3 + 1] = g;
            row_buf[x * 3 + 2] = r;
        }
        /* Zero padding bytes at end of row */
        for (uint32_t p = h_disp * 3; p < row_bytes; p++)
            row_buf[p] = 0;
        fwrite(row_buf, 1, row_bytes, fp);
    }

    free(row_buf);
    fclose(fp);

    voodoo_trace_log("Voodoo trace: Dumped frame %u (%ux%u) to %s\n",
                     frame_num, h_disp, v_disp, filename);
}

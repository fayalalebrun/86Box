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
    {0, NULL}
};

static const char *
get_register_name(uint32_t addr)
{
    static char unknown[16];
    uint32_t reg_addr = addr & 0x3fc;

    for (int i = 0; reg_names[i].name != NULL; i++) {
        if (reg_names[i].addr == reg_addr)
            return reg_names[i].name;
    }

    snprintf(unknown, sizeof(unknown), "0x%03x", reg_addr);
    return unknown;
}

void
voodoo_trace_init(voodoo_trace_t *trace, const char *filename,
                  const voodoo_trace_header_t *header, int text_mode)
{
    char txt_filename[512];
    time_t now;

    if (!trace || !filename)
        return;

    memset(trace, 0, sizeof(voodoo_trace_t));

    /* Open binary trace file */
    trace->bin_file = fopen(filename, "wb");
    if (!trace->bin_file) {
        pclog("voodoo_trace: Failed to open binary trace file: %s\n", filename);
        return;
    }

    /* Write header */
    if (fwrite(header, sizeof(voodoo_trace_header_t), 1, trace->bin_file) != 1) {
        pclog("voodoo_trace: Failed to write header\n");
        fclose(trace->bin_file);
        trace->bin_file = NULL;
        return;
    }

    /* Open text trace file if requested */
    if (text_mode) {
        snprintf(txt_filename, sizeof(txt_filename), "%s.txt", filename);
        trace->txt_file = fopen(txt_filename, "w");
        if (trace->txt_file) {
            now = time(NULL);
            fprintf(trace->txt_file, "# Voodoo Trace v%d\n", header->version);
            fprintf(trace->txt_file, "# Generated: %s", ctime(&now));
            fprintf(trace->txt_file, "# Type: %d, FB: %dMB, TEX: %dMB x%d TMU(s)\n",
                    header->voodoo_type, header->fb_size_mb,
                    header->tex_size_mb, header->num_tmus);
            fprintf(trace->txt_file, "# CPU: %llu Hz, PCI: %u Hz\n",
                    (unsigned long long)header->cpu_speed_hz, header->pci_speed_hz);
            fprintf(trace->txt_file, "# Format: <CMD> <timestamp> <addr> [register_name] <data> [xN]\n");
            fprintf(trace->txt_file, "#\n");
            fflush(trace->txt_file);
        }
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
    char idx_filename[512];
    snprintf(idx_filename, sizeof(idx_filename), "%s.idx.csv", filename);
    trace->idx_file = fopen(idx_filename, "w");
    if (trace->idx_file) {
        fprintf(trace->idx_file, "frame,start_cycle,end_cycle,start_offset,end_offset,commands,triangles,h_disp,v_disp,duration\n");
    } else {
        pclog("voodoo_trace: Failed to open frame index file: %s\n", idx_filename);
    }

    trace->frame_start_offset = ftell(trace->bin_file);
    trace->frame_start_cycle = 0;
    trace->frame_cmd_count = 0;

    voodoo_trace_log("Voodoo trace initialized: %s (text=%d, max_frames=%u)\n",
                     filename, text_mode, trace->max_frames);
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

        /* Add register name for register accesses */
        if (trace->pending_entry.cmd_type == VOODOO_TRACE_WRITE_REG_L ||
            trace->pending_entry.cmd_type == VOODOO_TRACE_WRITE_REG_W ||
            trace->pending_entry.cmd_type == VOODOO_TRACE_READ_REG_L ||
            trace->pending_entry.cmd_type == VOODOO_TRACE_READ_REG_W) {
            reg_name = get_register_name(trace->pending_entry.addr);
        }

        if (trace->pending_entry.count > 1) {
            if (reg_name) {
                fprintf(trace->txt_file, "%-10s %12llu-%12llu 0x%06x %-16s 0x%08x x%u\n",
                        cmd_names[trace->pending_entry.cmd_type],
                        (unsigned long long)trace->pending_entry.timestamp,
                        (unsigned long long)trace->pending_entry.timestamp_end,
                        trace->pending_entry.addr,
                        reg_name,
                        trace->pending_entry.data,
                        trace->pending_entry.count);
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
                fprintf(trace->txt_file, "%-10s %12llu 0x%06x %-16s 0x%08x\n",
                        cmd_names[trace->pending_entry.cmd_type],
                        (unsigned long long)trace->pending_entry.timestamp,
                        trace->pending_entry.addr,
                        reg_name,
                        trace->pending_entry.data);
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

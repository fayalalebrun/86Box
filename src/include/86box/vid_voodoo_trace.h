/*
 * 86Box    A hypervisor and IBM PC system emulator that specializes in
 *          running old operating systems and software designed for IBM
 *          PC systems and compatibles from 1981 through fairly recent
 *          system designs based on the PCI bus.
 *
 *          This file is part of the 86Box distribution.
 *
 *          Voodoo command trace capture.
 *
 * Authors: Claude Code Assistant
 *
 *          Copyright 2025.
 */
#ifndef VIDEO_VOODOO_TRACE_H
#define VIDEO_VOODOO_TRACE_H

#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Trace command types */
typedef enum {
    VOODOO_TRACE_WRITE_REG_L  = 0,  /* 32-bit register write */
    VOODOO_TRACE_WRITE_REG_W  = 1,  /* 16-bit register write */
    VOODOO_TRACE_WRITE_FB_L   = 2,  /* 32-bit framebuffer write */
    VOODOO_TRACE_WRITE_FB_W   = 3,  /* 16-bit framebuffer write */
    VOODOO_TRACE_WRITE_TEX_L  = 4,  /* 32-bit texture write */
    VOODOO_TRACE_WRITE_CMDFIFO= 5,  /* Write to CMDFIFO region */
    VOODOO_TRACE_READ_REG_L   = 6,  /* 32-bit register read */
    VOODOO_TRACE_READ_REG_W   = 7,  /* 16-bit register read */
    VOODOO_TRACE_READ_FB_L    = 8,  /* 32-bit framebuffer read */
    VOODOO_TRACE_READ_FB_W    = 9,  /* 16-bit framebuffer read */
    VOODOO_TRACE_VSYNC        = 10, /* VSync event */
    VOODOO_TRACE_SWAP         = 11, /* Buffer swap */
    VOODOO_TRACE_CONFIG       = 12, /* Display config change */
    VOODOO_TRACE_FRAME_END    = 13, /* Frame end marker */
} voodoo_trace_cmd_t;

/* Trace file header (64 bytes) */
typedef struct {
    uint32_t magic;           /* 0x564F4F44 "VOOD" */
    uint32_t version;         /* Format version (1) */
    uint32_t voodoo_type;     /* VOODOO_1, VOODOO_2, etc */
    uint32_t fb_size_mb;      /* Framebuffer size in MB */
    uint32_t tex_size_mb;     /* Texture size in MB (per TMU) */
    uint32_t num_tmus;        /* Number of TMUs (1 or 2) */
    uint32_t pci_base_addr;   /* PCI BAR base address */
    uint32_t flags;           /* Reserved flags */
    uint64_t cpu_speed_hz;    /* CPU speed in Hz */
    uint32_t pci_speed_hz;    /* PCI bus speed in Hz */
    uint32_t entry_count;     /* Total entries (for validation, 0=unknown) */
    uint32_t reserved[4];     /* Reserved for future use (16 bytes) */
} __attribute__((packed)) voodoo_trace_header_t;

/* Standard trace entry (24 bytes) */
typedef struct {
    uint32_t timestamp;       /* CPU cycles from start (first occurrence) */
    uint32_t timestamp_end;   /* CPU cycles for last occurrence (if count > 1) */
    uint8_t  cmd_type;        /* voodoo_trace_cmd_t */
    uint8_t  reserved[3];     /* Explicit padding for alignment */
    uint32_t addr;            /* Address within 16MB space */
    uint32_t data;            /* Value written/read */
    uint32_t count;           /* Number of times this command was repeated */
} __attribute__((packed)) voodoo_trace_entry_t;

/* Extended config entry (32 bytes) */
typedef struct {
    uint64_t timestamp;
    uint32_t cmd_type;        /* VOODOO_TRACE_CONFIG */
    uint32_t h_disp;
    uint32_t v_disp;
    uint32_t refresh_hz;
    uint32_t pixel_clock_hz;
    uint32_t reserved;        /* Reserved for future use (4 bytes) */
} __attribute__((packed)) voodoo_trace_config_t;

/* Trace context */
typedef struct voodoo_trace_t {
    FILE     *bin_file;       /* Binary trace file */
    FILE     *txt_file;       /* Text trace file (optional) */
    uint64_t  start_time;     /* Reference timestamp */
    uint64_t  entry_count;    /* Number of entries written */
    uint32_t  last_frame;     /* Last frame number */
    uint32_t  max_frames;     /* Maximum frames to capture (0=unlimited) */
    int       capture_reads;  /* Whether to capture reads */
    int       capture_fb;     /* Whether to capture framebuffer ops */
    int       capture_vsync;  /* Whether to capture vsync events */
    int       capture_enabled;/* Frame-limited capture enabled */
    int       has_pending;    /* Whether pending_entry contains data */
    voodoo_trace_entry_t pending_entry;  /* Buffered entry for coalescing */

    /* Frame index tracking */
    FILE     *idx_file;       /* Frame index CSV file */
    uint64_t  frame_start_offset;  /* Byte offset where current frame started */
    uint64_t  frame_start_cycle;   /* Cycle when current frame started */
    uint32_t  frame_cmd_count;     /* Commands in current frame */
} voodoo_trace_t;

/* API Functions */
void voodoo_trace_init(voodoo_trace_t *trace, const char *filename,
                       const voodoo_trace_header_t *header,
                       int text_mode);
void voodoo_trace_close(voodoo_trace_t *trace);

void voodoo_trace_write(voodoo_trace_t *trace,
                       voodoo_trace_cmd_t cmd_type,
                       uint32_t addr, uint32_t data);

void voodoo_trace_read(voodoo_trace_t *trace,
                      voodoo_trace_cmd_t cmd_type,
                      uint32_t addr, uint32_t data);

void voodoo_trace_vsync(voodoo_trace_t *trace,
                       uint32_t frame_num,
                       uint32_t resolution);

void voodoo_trace_swap(voodoo_trace_t *trace,
                      uint32_t swap_offset,
                      uint32_t frame_num,
                      uint32_t resolution);

void voodoo_trace_config(voodoo_trace_t *trace,
                        uint32_t h_disp, uint32_t v_disp,
                        uint32_t refresh_hz, uint32_t pixel_clock_hz);

void voodoo_trace_flush(voodoo_trace_t *trace);

#ifdef __cplusplus
}
#endif

#endif /* VIDEO_VOODOO_TRACE_H */

/* -*- c-basic-offset:4; tab-width:4; indent-tabs-mode:nil -*- */
/*
 * zmodem.c
 *
 * qodem - Qodem Terminal Emulator
 *
 * Written 2003-2017 by Kevin Lamonte
 *
 * To the extent possible under law, the author(s) have dedicated all
 * copyright and related and neighboring rights to this software to the
 * public domain worldwide. This software is distributed without any
 * warranty.
 *
 * You should have received a copy of the CC0 Public Domain Dedication along
 * with this software. If not, see
 * <http://creativecommons.org/publicdomain/zero/1.0/>.
 *
 * Modified 2019- by Araki Ken
 * (Based on qodem-1.0.0)
 */

/*
 * Bugs in the Zmodem protocol:
 *
 * 1.  ZCOMMAND, huge-ass security hole.
 *
 * 2.  Stupid arbitrary decisions about when the argument field is big-
 *     endian and when it's little-endian.
 *
 * 3.  Arbitrary non-control characters CANNOT be escaped making it
 *     impossible to protect against connection closures in telnet, rlogin,
 *     and ssh.
 *
 *
 * Bugs noted in lrzsz implementation:
 *
 * 1.  A spurious ZRQINIT from sz if we use ZCHALLENGE.
 *
 * 2.  core from sz if ZRPOS position > file size.
 *
 * 3.  rz assumes CRC32 on ZSINIT, even if it gets a 16-bit or hex header.
 *
 * 4.  sz requires a hex ZRPOS on error, not sure why.
 *
 * 5.  sz requires a hex ZCRC, not sure why.
 *
 *
 */

#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <unistd.h>
#include <libgen.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <utime.h>
#include "zmodem.h"

#include <pobl/bl_def.h> /* USE_WIN32API */

typedef unsigned int uint32_t;

/**
 * The supported download protocols.
 */
typedef enum Q_PROTOCOLS {
    Q_PROTOCOL_ASCII,           /* ASCII */
    Q_PROTOCOL_KERMIT,          /* Kermit */
    Q_PROTOCOL_XMODEM,          /* Xmodem */
    Q_PROTOCOL_XMODEM_CRC,      /* Xmodem CRC */
    Q_PROTOCOL_XMODEM_RELAXED,  /* Xmodem Relaxed */
    Q_PROTOCOL_XMODEM_1K,       /* Xmodem-1K */
    Q_PROTOCOL_YMODEM,          /* Ymodem Batch */
    Q_PROTOCOL_ZMODEM,          /* Zmodem Batch */
    Q_PROTOCOL_XMODEM_1K_G,     /* Xmodem-1K/G */
    Q_PROTOCOL_YMODEM_G         /* Ymodem/G Batch */
} Q_PROTOCOL;

/**
 * The transfer state as exposed to the user in the file transfer dialog
 * screen.
 */
typedef enum Q_TRANSFER_STATES {
    /**
     * Initial state.
     */
    Q_TRANSFER_STATE_INIT,

    /**
     * Waiting for file information.
     */
    Q_TRANSFER_STATE_FILE_INFO,

    /**
     * Transferrring a file.
     */
    Q_TRANSFER_STATE_TRANSFER,

    /**
     * Completed with a file, maybe waiting for another file info.
     */
    Q_TRANSFER_STATE_FILE_DONE,

    /**
     * Transfer aborted, displaying completion screen.
     */
    Q_TRANSFER_STATE_ABORT,

    /**
     * Displaying completion screen.
     */
    Q_TRANSFER_STATE_END
} Q_TRANSFER_STATE;

/**
 * The data behind the file transfer dialog screen.
 */
struct q_transfer_stats_struct {
    Q_TRANSFER_STATE state;
    Q_PROTOCOL protocol;
    char * protocol_name;
    char * filename;
    char * pathname;
    char * last_message;
    unsigned long bytes_total;
    unsigned long bytes_transfer;
    unsigned long blocks;
    unsigned long block_size;
    unsigned long blocks_transfer;
    unsigned long error_count;

    /**
     * The total bytes to send for a batch.
     */
    unsigned long batch_bytes_total;

    /**
     * The amount of bytes sent so far for a batch.
     */
    unsigned long batch_bytes_transfer;

    time_t batch_start_time;
    time_t file_start_time;
    time_t end_time;
};

/**
 * Transfer statistics.  Lots of places need to peek into this structure.
 */
struct q_transfer_stats_struct q_transfer_stats = {
    Q_TRANSFER_STATE_INIT,
    Q_PROTOCOL_ASCII,
    NULL,
    NULL,
    NULL,
    NULL,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

#if 0
#define DLOG(A) printf A; fflush(NULL)
#else
#define DLOG(A)
#endif
#if 0
#define DLOG2(A) printf A; fflush(NULL)
#else
#define DLOG2(A)
#endif
#define _(A) A

/* Maximum length of any filename */
#define FILENAME_SIZE           256

/*
 * Maximum length of a message generated for a dialog box (e.g. file transfer
 * dialog)
 */
#define DIALOG_MESSAGE_SIZE     128

static Q_BOOL q_screen_dirty = Q_TRUE;

/* Members of q_status in qodem.h */
static Q_BOOL zmodem_escape_ctrl = Q_FALSE;
static Q_BOOL zmodem_zchallenge = Q_FALSE;

#define Xfree(X, Y, Z)                  free(X)

#define C_LF 0x0A
#define C_CR 0x0D
#define C_XON 0x11
#define C_XOFF 0x13
#define C_CAN 0x18

/**
 * strdup() equivalent that plugs into the Hans-Boehm GC if it is enabled, if
 * not it just passes through to the default strdup().
 *
 * @param ptr the string to copy
 * @param file the filename calling this function
 * @param line the line number at the call point.
 */
static char * Xstrdup(const char * ptr, const char * file, const int line) {

    char * local_ptr;
#if defined(Q_GC_BOEHM) && defined(HAVE_LIBGC)
    int length;
    int i;
#endif

    assert(ptr != NULL);

#if defined(Q_GC_BOEHM) && defined(HAVE_LIBGC)
    length = strlen(ptr) + 1;
#ifdef DEBUG
    local_ptr = (char *) GC_debug_malloc(length, file, line);
#else
    local_ptr = (char *) GC_malloc(length);
#endif /* DEBUG */

    if (local_ptr != NULL) {
        for (i = 0; i < length; i++) {
            local_ptr[i] = ptr[i];
        }
    }

#else

    local_ptr = strdup(ptr);

#endif

#if 0
    /* For debugging: */
    fprintf(stderr, "Xstrdup: ptr=%p local_ptr=%p file=%s line=%d\n", ptr,
            local_ptr, file, line);
#endif

    return local_ptr;
}

/**
 * End the file transfer.
 *
 * @param new_state the state to switch to after a brief display pause
 */
static void stop_file_transfer(const Q_TRANSFER_STATE new_state) {
    zmodem_stop(Q_TRUE);

    q_transfer_stats.state = new_state;
    q_screen_dirty = Q_TRUE;
    time(&q_transfer_stats.end_time);
}

/**
 * Set the exposed filename.  Allocates a copy of the string which is freed
 * when the program state is switched to Q_STATE_CONSOLE.
 *
 * @param new_string the new filname
 */
static void set_transfer_stats_filename(const char * new_string) {
#if 0
    /*
     * Do nothing if being set to what it already is.  Note this is a pointer
     * comparison, not a string comparison.
     */
    if (new_string == q_transfer_stats.filename) {
        return;
    }

    if (q_transfer_stats.filename != NULL) {
        Xfree(q_transfer_stats.filename, __FILE__, __LINE__);
    }
    q_transfer_stats.filename = Xstrdup(new_string, __FILE__, __LINE__);
#endif
}

/**
 * Set the exposed path name.  Allocates a copy of the string which is freed
 * when the program state is switched to Q_STATE_CONSOLE.
 *
 * @param new_string the new path name
 */
static void set_transfer_stats_pathname(const char * new_string) {
#if 0
    /*
     * Do nothing if being set to what it already is.  Note this is a pointer
     * comparison, not a string comparison.
     */
    if (new_string == q_transfer_stats.pathname) {
        return;
    }

    if (q_transfer_stats.pathname != NULL) {
        Xfree(q_transfer_stats.pathname, __FILE__, __LINE__);
    }
    q_transfer_stats.pathname = Xstrdup(new_string, __FILE__, __LINE__);
#endif
}

/**
 * Set the exposed message.  Allocates a copy of the string which is freed
 * when the program state is switched to Q_STATE_CONSOLE.
 *
 * @param new_string the new message
 */
static void set_transfer_stats_last_message(const char * format, ...) {
#if 0
    char outbuf[DIALOG_MESSAGE_SIZE];
    va_list arglist;
    memset(outbuf, 0, sizeof(outbuf));
    va_start(arglist, format);
    vsprintf((char *) (outbuf + strlen(outbuf)), format, arglist);
    va_end(arglist);

    if (q_transfer_stats.last_message != NULL) {
        Xfree(q_transfer_stats.last_message, __FILE__, __LINE__);
    }
    q_transfer_stats.last_message = Xstrdup(outbuf, __FILE__, __LINE__);

    /*
     * Report the message immediately
     */
    q_screen_dirty = Q_TRUE;
#endif
}

/*
 * Require an ACK every 32 frames on reliable links.
 */
#define WINDOW_SIZE_RELIABLE 32

/*
 * Require an ACK every 4 frames on unreliable links.
 */
#define WINDOW_SIZE_UNRELIABLE 4

/* Data types ----------------------------------------------- */

/* Used to note the start of a packet */
#define ZPAD                    '*'

/* CRC next, frame ends, header packet follows */
#define ZCRCE                   'h'

/* CRC next, frame continues nonstop */
#define ZCRCG                   'i'

/* CRC next, frame continues, ZACK expected */
#define ZCRCQ                   'j'

/* CRC next, ZACK expected, end of frame */
#define ZCRCW                   'k'

/* Packet types */
#define P_ZRQINIT               0
#define P_ZRINIT                1
#define P_ZSINIT                2
#define P_ZACK                  3
#define P_ZFILE                 4
#define P_ZSKIP                 5
#define P_ZNAK                  6
#define P_ZABORT                7
#define P_ZFIN                  8
#define P_ZRPOS                 9
#define P_ZDATA                 10
#define P_ZEOF                  11
#define P_ZFERR                 12
#define P_ZCRC                  13
#define P_ZCHALLENGE            14
#define P_ZCOMPL                15
#define P_ZCAN                  16
#define P_ZFREECNT              17
#define P_ZCOMMAND              18

/* Transfer capabilities sent in ZRInit packet */

/* Rx can send and receive true FDX */
#define TX_CAN_FULL_DUPLEX      0x00000001

/* Rx can receive data during disk I/O */
#define TX_CAN_OVERLAP_IO       0x00000002

/* Rx can send a break signal */
#define TX_CAN_BREAK            0x00000004

/* Receiver can decrypt */
#define TX_CAN_DECRYPT          0x00000008

/* Receiver can uncompress */
#define TX_CAN_LZW              0x00000010

/* Receiver can use 32 bit Frame Check */
#define TX_CAN_CRC32            0x00000020

/* Receiver expects ctl chars to be escaped */
#define TX_ESCAPE_CTRL          0x00000040

/* Receiver expects 8th bit to be escaped */
#define TX_ESCAPE_8BIT          0x00000080

/**
 * The Zmodem protocol state that can encompass multiple file transfers.
 */
typedef enum {
    /* Before the first byte is sent */
    INIT,
    /* Transfer complete */
    COMPLETE,
    /* Transfer was aborted due to excessive timeouts or ZCAN */
    ABORT,


    /* Collecting data for a ZFILE, ZSINIT, ZDATA, and ZCOMMAND packet. */
    ZDATA,

    /*
     * Receiver side
     */

    /* Send ZRINIT */
    ZRINIT,

    /* Waiting for ZFILE or ZSINIT */
    ZRINIT_WAIT,

    /* Send ZCHALLENGE */
    ZCHALLENGE,

    /* Waiting for ZACK */
    ZCHALLENGE_WAIT,

    /* Send ZRPOS */
    ZRPOS,

    /* Waiting for ZDATA */
    ZRPOS_WAIT,

    /* Send ZSKIP */
    ZSKIP,

    /* Send ZCRC */
    ZCRC,

    /* Waiting for ZCRC */
    ZCRC_WAIT,

    /*
     * Sender side
     */

    /* Send ZRQINIT */
    ZRQINIT,

    /* Waiting for ZRINIT or ZCHALLENGE */
    ZRQINIT_WAIT,

    /* Send ZSINIT */
    ZSINIT,

    /* Waiting for ZACK */
    ZSINIT_WAIT,

    /* Send ZFILE */
    ZFILE,

    /* Waiting for ZSKIP, ZCRC, or ZRPOS */
    ZFILE_WAIT,

    /* Send ZEOF */
    ZEOF,

    /* Waiting for ZRPOS */
    ZEOF_WAIT,

    /* Send ZFIN */
    ZFIN,

    /* Waiting for ZFIN */
    ZFIN_WAIT
} STATE;

/**
 * The single-file-specific protocol state variables.
 */
struct ZMODEM_STATUS {
    /* INIT, COMPLETE, ABORT, etc. */
    STATE state;

    /* State before entering DATA state */
    STATE prior_state;

    /* Send/receive flags */
    unsigned long flags;

    /* If true, use 32-bit CRC */
    Q_BOOL use_crc32;

    /* If true, we are the sender */
    Q_BOOL sending;

    /* Current filename being sent/received */
    char * file_name;

    /* Size of file in bytes */
    unsigned int file_size;

    /* Modification time of file */
    time_t file_modtime;

    /* Current position */
    off_t file_position;

    /* Stream pointer to current file */
    FILE * file_stream;

    /* File CRC32 */
    uint32_t file_crc32;

    /* Block size */
    int block_size;

    /* If true, sent block will ask for ZACK */
    Q_BOOL ack_required;

    /* If true, we are waiting to hear ZACK */
    Q_BOOL waiting_for_ack;

    /*
     * If true, we are continuously streaming the ZDATA "data subpacket" and
     * will not need to generate a new packet header.
     */
    Q_BOOL streaming_zdata;

    /* Timeout normally lasts 10 seconds */
    int timeout_length;

    /* The beginning time for the most recent timeout cycle */
    time_t timeout_begin;

    /* Total number of timeouts before aborting is 5 */
    int timeout_max;

    /* Total number of timeouts so far */
    int timeout_count;

    /* Number of bytes confirmed from the receiver */
    int confirmed_bytes;

    /*
     * Number of bytes confirmed from the receiver when we dropped the block
     * size.
     */
    int last_confirmed_bytes;

    /* True means TCP/IP or error-correcting modem */
    Q_BOOL reliable_link;

    /* File position when the block size was last reduced */
    off_t file_position_downgrade;

    /* When 0, require a ZACK, controls window size */
    unsigned blocks_ack_count;

    /* Number of error blocks */
    int consecutive_errors;

    /* Full pathname to file */
    char file_fullname[FILENAME_SIZE];

};

/**
 * The per-file transfer status.
 */
static struct ZMODEM_STATUS status = {
    ABORT, /* Set ABORT by default for zmodem_is_processing() */
    INIT,
    0,
    Q_TRUE,
    Q_FALSE,
    NULL,
    0,
    0,
    0,
    NULL,
    -1,
    ZMODEM_BLOCK_SIZE,
    Q_FALSE,
    Q_FALSE,
    Q_FALSE,
    10,
    0,
    5,
    0,
    0,
    Q_TRUE,
    0,
    0,
    0
};

/* The list of files to upload */
static struct file_info * upload_file_list;

/* The current entry in upload_file_list being sent */
static int upload_file_list_i;

/*
 * The path to download to.  Note download_path is Xstrdup'd TWICE: once HERE
 * and once more on the progress dialog.  The q_program_state transition to
 * Q_STATE_CONSOLE is what Xfree's the copy in the progress dialog.  This
 * copy is Xfree'd in zmodem_stop().
 */
static char * download_path = NULL;

/* Every bit of Zmodem data goes out as packets */
struct zmodem_packet {
    int type;
    uint32_t argument;
    Q_BOOL use_crc32;
    int crc16;
    uint32_t crc32;
    unsigned char data[ZMODEM_MAX_BLOCK_SIZE];
    unsigned int data_n;

    /*
     * Performance tweak for decode_zdata_bytes to allow it to quickly bail
     * out during CRC check.
     */
    unsigned char crc_buffer[5];
};

/* Needs to persist across calls to zmodem() */
static struct zmodem_packet packet;

/* Internal buffer used to collect a complete packet before processing it */
static unsigned char packet_buffer[ZMODEM_MAX_BLOCK_SIZE];
static unsigned int packet_buffer_n;

/*
 * Internal buffer used to queue a complete outbound packet so that the
 * top-level code can saturate the link.
 */
static unsigned char outbound_packet[ZMODEM_MAX_BLOCK_SIZE];
static unsigned int outbound_packet_n;

/* Return codes from parse_packet() */
typedef enum {
    ZM_PP_INVALID,
    ZM_PP_NODATA,
    ZM_PP_CRCERROR,
    ZM_PP_OK
} ZM_PARSE_PACKET;

#define big_to_little_endian(X) (((X >> 24) & 0xFF) | \
                                ((X >> 8) & 0xFF00) | \
                                ((X << 8) & 0xFF0000) | \
                                ((X << 24) & 0xFF000000))

/* The ZCHALLENGE value we asked for */
static uint32_t zchallenge_value;

static int progress_length;

/* CRC16 CODE ------------------------------------------------------------- */

/*
 * KAL - This CRC16 routine was taken verbatim from XYMODEM.DOC.
 *
 * This function calculates the CRC used by the XMODEM/CRC Protocol
 * The first argument is a pointer to the message block.
 * The second argument is the number of bytes in the message block.
 * The function returns an integer which contains the CRC.
 * The low order 16 bits are the coefficients of the CRC.
 */
static int compute_crc16(int crc, const unsigned char *ptr, int count) {
    int i;

    while (--count >= 0) {
        crc = crc ^ ((int) *ptr++ << 8);
        for (i = 0; i < 8; ++i)
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc = crc << 1;
    }
    return (crc & 0xFFFF);
}

/* CRC32 CODE ------------------------------------------------------------- */

/*
 * The following CRC32 code was posted by Colin Plumb in
 * comp.os.linux.development.system.  Google link:
 * http://groups.google.com/groups?hl=en&lr=&ie=UTF-8&selm=4dr0ab%24o1k%40nyx10.cs.du.edu
 */

/*
 * This uses the CRC-32 from IEEE 802 and the FDDI MAC,
 * x^32+x^26+x^23+x^22+x^16+x^12+x^11+x^10+x^8+x^7+x^5+x^4+x^2+x+1.
 *
 * This is, for a slight efficiency win, used in a little-endian bit
 * order, where the least significant bit of the accumulator (and each
 * byte of input) corresponds to the highest exponent of x.
 * E.g. the byte 0x23 is interpreted as x^7+x^6+x^2.  For the
 * most rational output, the computed 32-bit CRC word should be
 * sent with the low byte (which are the most significant coefficients
 * from the polynomial) first.  If you do this, the CRC of a buffer
 * that includes a trailing CRC will always be zero.  Or, if you
 * use a trailing invert variation, some fixed value.  For this
 * polynomial, that fixed value is 0x2144df1c.  (Leading presets,
 * be they to 0, -1, or something else, don't matter.)
 *
 * Thus, the little-endian hex constant is as follows:
 *           11111111112222222222333
 * 012345678901234567890123456789012
 * 111011011011100010000011001000001
 * \  /\  /\  /\  /\  /\  /\  /\  /\
 *   E   D   B   8   8   3   2   0
 *
 * This technique, while a bit confusing, is widely used in e.g. Zmodem.
 */

#define CRC32 0xedb88320        /* CRC polynomial */

static uint32_t crc_32_tab[256];

/*
 * This computes the CRC table quite efficiently, using the fact that
 * crc_32_tab[i^j] = crc_32_tab[i] ^ crc_32_tab[j].  We start out with
 * crc_32_tab[0] = 0, j = 128, and then set h to the desired value of
 * crc_32_tab[j].  Then for each crc_32_tab[i] which is already set (which
 * includes i = 0, so crc_32_tab[j] will get set), set crc_32_tab[i^j] =
 * crc_32_tab[i] ^ h.
 * Then divide j by 2 and repeat until everything is filled in.
 * The first pass sets crc_32_tab[128].  The second sets crc_32_tab[64]
 * and crc_32_tab[192].  The third sets entries 32, 96, 160 and 224.
 * The eighth and last pass sets all the odd-numbered entries.
 */
static void makecrc(void) {
    int i, j = 128;
    uint32_t h = 1;

    crc_32_tab[0] = 0;
    do {
        if (h & 1)
            h = (h >> 1) ^ CRC32;
        else
            h >>= 1;
        for (i = 0; i < 256; i += j + j)
            crc_32_tab[i + j] = crc_32_tab[i] ^ h;
    } while (j >>= 1);
}

/*
 * Compute a CRC on the given buffer and length using a static CRC
 * accumulator.  If buf is NULL this initializes the accumulator,
 * otherwise it updates it to include the additional data and
 * returns the CRC of the data so far.
 *
 * The CRC is computed using preset to -1 and invert.
 */
static uint32_t compute_crc32(const uint32_t old_crc, const unsigned char *buf,
                              unsigned len) {
    uint32_t crc;

    if (buf) {
        crc = old_crc;
        while (len--)
            crc = (crc >> 8) ^ crc_32_tab[(crc ^ *buf++) & 0xff];
        return crc ^ 0xffffffff;        /* Invert */
    } else {
        return 0xffffffff;      /* Preset to -1 */
    }
}

/* CRC32 CODE ------------------------------------------------------------- */

/* ------------------------------------------------------------------------ */
/* Block size adjustment logic -------------------------------------------- */
/* ------------------------------------------------------------------------ */

/**
 * Move up to a larger block size if things are going better.
 */
static void block_size_up() {
    DLOG(("block_size_up(): block_size = %d\n", status.block_size));

    /*
     * After getting a clean bill of health for 8k, move block size up
     */
    if ((status.confirmed_bytes - status.file_position_downgrade) > 8196) {
        status.block_size *= 2;
        if (status.block_size > ZMODEM_BLOCK_SIZE) {
            status.block_size = ZMODEM_BLOCK_SIZE;
        }
    }
    status.last_confirmed_bytes = status.confirmed_bytes;

    DLOG(("block_size_up(): NEW block size = %d\n", status.block_size));
}

/**
 * Move down to a smaller block size if things are going badly.
 */
static void block_size_down() {
    int outstanding_packets =
        (status.confirmed_bytes - status.last_confirmed_bytes) /
            status.block_size;

    DLOG(("block_size_down(): block_size = %d outstanding_packets = %d\n",
            status.block_size, outstanding_packets));

    if (outstanding_packets >= 3) {
        if (status.block_size > 32) {
            status.block_size /= 2;
            status.file_position_downgrade = status.confirmed_bytes;
        }
    }
    if (outstanding_packets >= 10) {
        if (status.block_size == 32) {
            /*
             * Too much line noise, give up
             */
            status.state = ABORT;
            stop_file_transfer(Q_TRANSFER_STATE_ABORT);
            set_transfer_stats_last_message(_("LINE NOISE, !@#&*%U"));
        }
    }
    status.blocks_ack_count = WINDOW_SIZE_UNRELIABLE;
    status.last_confirmed_bytes = status.confirmed_bytes;

    DLOG(("block_size_down(): NEW block size = %d\n", status.block_size));

}

/* ------------------------------------------------------------------------ */
/* Progress dialog -------------------------------------------------------- */
/* ------------------------------------------------------------------------ */

/**
 * Statistics: reset for a new file.
 *
 * @param filename the file being transferred
 * @param filesize the size of the file (known from either file attributes or
 * because this is an upload)
 */
static void stats_new_file(const char * filename, const int filesize) {
    char * basename_arg;
    char * dirname_arg;

    /*
     * Clear per-file statistics
     */
    q_transfer_stats.blocks_transfer = 0;
    q_transfer_stats.bytes_transfer = 0;
    q_transfer_stats.error_count = 0;
    status.confirmed_bytes = 0;
    status.last_confirmed_bytes = 0;
    set_transfer_stats_last_message("");
    q_transfer_stats.bytes_total = filesize;
    q_transfer_stats.blocks = filesize / ZMODEM_BLOCK_SIZE;
    if ((filesize % ZMODEM_BLOCK_SIZE) > 0) {
        q_transfer_stats.blocks++;
    }

    /*
     * Note that basename and dirname modify the arguments
     */
    basename_arg = Xstrdup(filename, __FILE__, __LINE__);
    dirname_arg = Xstrdup(filename, __FILE__, __LINE__);
    set_transfer_stats_filename(basename(basename_arg));
    set_transfer_stats_pathname(dirname(dirname_arg));
    Xfree(basename_arg, __FILE__, __LINE__);
    Xfree(dirname_arg, __FILE__, __LINE__);

    q_transfer_stats.state = Q_TRANSFER_STATE_TRANSFER;
    q_screen_dirty = Q_TRUE;
    time(&q_transfer_stats.file_start_time);
}

/**
 * Statistics: increment the block count.
 */
static void stats_increment_blocks() {
    q_transfer_stats.block_size = status.block_size;
    q_transfer_stats.blocks_transfer = status.file_position / ZMODEM_BLOCK_SIZE;
    if ((status.file_position % ZMODEM_BLOCK_SIZE) > 0) {
        q_transfer_stats.blocks_transfer++;
    }

    q_screen_dirty = Q_TRUE;

    status.consecutive_errors = 0;

    DLOG(("stats_increment_blocks(): waiting_for_ack = %s ack_required = %s reliable_link = %s confirmed_bytes = %u last_confirmed_bytes = %u block_size = %d blocks_ack_count = %d\n",
            (status.waiting_for_ack == Q_TRUE ? "true" : "false"),
            (status.ack_required == Q_TRUE ? "true" : "false"),
            (status.reliable_link == Q_TRUE ? "true" : "false"),
            status.confirmed_bytes, status.last_confirmed_bytes,
            status.block_size, status.blocks_ack_count));

}

/**
 * Statistics: an error was encountered.
 *
 * @param format a format string + arguments for emitting to the progress
 * dialog
 */
static void stats_increment_errors(const char * format, ...) {
    char outbuf[DIALOG_MESSAGE_SIZE];
    va_list arglist;
    memset(outbuf, 0, sizeof(outbuf));
    va_start(arglist, format);
    vsprintf((char *) (outbuf + strlen(outbuf)), format, arglist);
    va_end(arglist);
    set_transfer_stats_last_message(outbuf);

    q_transfer_stats.error_count++;
    status.consecutive_errors++;
    q_transfer_stats.block_size = status.block_size;

    /*
     * Unreliable link is a one-way ticket until the next call to
     * zmodem_start() .
     */
    status.reliable_link = Q_FALSE;

    DLOG(("stats_increment_errors(): waiting_for_ack = %s ack_required = %s reliable_link = %s confirmed_bytes = %u last_confirmed_bytes = %u block_size = %d blocks_ack_count = %d\n",
            (status.waiting_for_ack == Q_TRUE ? "true" : "false"),
            (status.ack_required == Q_TRUE ? "true" : "false"),
            (status.reliable_link == Q_TRUE ? "true" : "false"),
            status.confirmed_bytes, status.last_confirmed_bytes,
            status.block_size, status.blocks_ack_count));

    /*
     * If too many errors when not in ZDATA, the other end is probably not
     * even running zmodem.  Bail out.
     */
    if ((status.consecutive_errors >= 15) && (status.state != ZDATA)) {
        set_transfer_stats_last_message(_("LINE NOISE, !@#&*%U"));
        stop_file_transfer(Q_TRANSFER_STATE_ABORT);
        status.state = ABORT;
    }

}

/**
 * Initialize a new file to upload.
 *
 * @return true if OK, false if the file could not be opened
 */
static Q_BOOL setup_for_next_file() {
    char * basename_arg;

    /*
     * Reset our dynamic variables
     */
    if (status.file_stream != NULL) {
        fclose(status.file_stream);
    }
    status.file_stream = NULL;
    if (status.file_name != NULL) {
        Xfree(status.file_name, __FILE__, __LINE__);
    }
    status.file_name = NULL;

    if (upload_file_list[upload_file_list_i].name == NULL) {
        /*
         * Special case: the termination/finish packet
         */
        DLOG(("ZMODEM: No more files (name='%s')\n",
                upload_file_list[upload_file_list_i].name));

        /*
         * Let's keep all the information the same, just increase the total
         * bytes.
         */
        q_transfer_stats.batch_bytes_transfer =
            q_transfer_stats.batch_bytes_total;
        q_screen_dirty = Q_TRUE;

        /*
         * We're done
         */
        status.state = ZFIN;
        return Q_TRUE;
    }

    /*
     * Get the file's modification time
     */
    status.file_modtime = upload_file_list[upload_file_list_i].fstats.st_mtime;
    status.file_size = upload_file_list[upload_file_list_i].fstats.st_size;

    /*
     * Open the file
     */
    if ((status.file_stream =
         fopen(upload_file_list[upload_file_list_i].name, "rb")) == NULL) {
        DLOG(("ERROR: Unable to open file %s: %s (%d)\n",
                upload_file_list[upload_file_list_i].name, strerror(errno),
                errno));

        status.state = ABORT;
        stop_file_transfer(Q_TRANSFER_STATE_ABORT);
        set_transfer_stats_last_message(_("DISK I/O ERROR"));
        return Q_FALSE;
    }

    /*
     * Note that basename and dirname modify the arguments
     */
    basename_arg =
        Xstrdup(upload_file_list[upload_file_list_i].name, __FILE__, __LINE__);

    if (status.file_name != NULL) {
        Xfree(status.file_name, __FILE__, __LINE__);
    }
    status.file_name = Xstrdup(basename(basename_arg), __FILE__, __LINE__);

    /*
     * Update the stats
     */
    stats_new_file(upload_file_list[upload_file_list_i].name,
                   upload_file_list[upload_file_list_i].fstats.st_size);

    /*
     * Free the copies passed to basename() and dirname()
     */
    Xfree(basename_arg, __FILE__, __LINE__);

    DLOG(("UPLOAD set up for new file %s (%lu bytes)...\n",
            upload_file_list[upload_file_list_i].name,
            (long int) upload_file_list[upload_file_list_i].fstats.st_size));

    /*
     * Update stuff if this is the second file
     */
    if (status.state != ABORT) {
        /*
         * Update main status state
         */
        q_transfer_stats.state = Q_TRANSFER_STATE_TRANSFER;
        /*
         * We need to send ZFILE now
         */
        status.state = ZFILE;
    }
    return Q_TRUE;
}

/**
 * Reset the timeout counter.
 */
static void reset_timer() {
    time(&status.timeout_begin);
}

/**
 * Check for a timeout.
 *
 * @return true if a timeout has occurred
 */
static Q_BOOL check_timeout() {
    time_t now;
    time(&now);

    DLOG(("check_timeout()\n"));

    if (now - status.timeout_begin >= status.timeout_length) {
        /*
         * Timeout
         */
        status.timeout_count++;
        DLOG(("ZMODEM: Timeout #%d\n", status.timeout_count));

        if (status.timeout_count >= status.timeout_max) {
            stats_increment_errors(_("TOO MANY TIMEOUTS, TRANSFER CANCELLED"));
            stop_file_transfer(Q_TRANSFER_STATE_ABORT);
            status.state = ABORT;
        } else {
            stats_increment_errors(_("TIMEOUT"));
        }

        /*
         * Reset timeout
         */
        reset_timer();
        return Q_TRUE;
    }

    /*
     * No timeout yet
     */
    return Q_FALSE;
}

/* ------------------------------------------------------------------------ */
/* Encoding layer --------------------------------------------------------- */
/* ------------------------------------------------------------------------ */

/**
 * Turn a string into hex.
 *
 * @param input a buffer with 8-bit bytes
 * @param input_n the number of bytes in input
 * @param output the output string of hexademical ASCII characters
 * @param output_max the maximum number of bytes that can be written to
 * output
 */
static void hexify_string(const unsigned char * input,
                          const unsigned int input_n,
                          unsigned char * output,
                          const unsigned int output_max) {

    char digits[] = "0123456789abcdefg";
    unsigned int i;

    assert(output_max >= input_n * 2);

    for (i = 0; i < input_n; i++) {
        output[2 * i] = digits[(input[i] & 0xF0) >> 4];
        output[2 * i + 1] = digits[input[i] & 0x0F];
    }
}

/**
 * Turn a hex string into binary.
 *
 * @param input a buffer with hexadecimal ASCII characters
 * @param input_n the number of bytes in input
 * @param output the output buffer of 8-bit bytes
 * @param output_max the maximum number of bytes that can be written to
 * output
 * @return true if conversion was successful, false if input is not a valid
 * hex string
 */
static Q_BOOL dehexify_string(const unsigned char * input,
                              const unsigned int input_n,
                              unsigned char * output,
                              const unsigned int output_max) {

    unsigned int i;

    assert(output_max >= input_n / 2);

    for (i = 0; i < input_n; i++) {
        int ch = tolower(input[i]);
        int j = i / 2;

        if ((ch >= '0') && (ch <= '9')) {
            output[j] = ch - '0';
        } else if ((ch >= 'a') && (ch <= 'f')) {
            output[j] = ch - 'a' + 0x0A;
        } else {
            /*
             * Invalid hex string
             */
            return Q_FALSE;
        }
        output[j] = output[j] << 4;

        i++;
        ch = tolower(input[i]);

        if ((ch >= '0') && (ch <= '9')) {
            output[j] |= ch - '0';
        } else if ((ch >= 'a') && (ch <= 'f')) {
            output[j] |= ch - 'a' + 0x0A;
        } else {
            /*
             * Invalid hex string
             */
            return Q_FALSE;
        }
    }

    /*
     * All OK
     */
    return Q_TRUE;
}

/* ------------------------------------------------------------------------ */
/* Bytes layer ------------------------------------------------------------ */
/* ------------------------------------------------------------------------ */

/**
 * Turn escaped ZDATA bytes into regular bytes, copying to output.
 *
 * The CRC escape sequence bytes are copied to crc_buffer which must be at
 * least 5 bytes long:  1 byte for CRC escape character, 4 bytes for CRC
 * data.
 *
 * The input buffer will be shifted down to handle multiple packets streaming
 * together.
 *
 * @param input the encoded bytes from the remote side
 * @param input_n the number of bytes in input_n
 * @param output a buffer to contain the decoded bytes
 * @param output_n the number of bytes that this function wrote to output
 * @param output_max the maximum size of the output buffer
 * @param crc_buffer a buffer to contain the CRC bytes
 */
static Q_BOOL decode_zdata_bytes(unsigned char * input,
                                 unsigned int * input_n,
                                 unsigned char * output,
                                 unsigned int * output_n,
                                 const unsigned int output_max,
                                 unsigned char * crc_buffer) {

    int i;                      /* input iterator */
    int j;                      /* for doing_crc case */
    Q_BOOL doing_crc = Q_FALSE;
    Q_BOOL done = Q_FALSE;
    unsigned char crc_type = 0;

    DLOG(("decode_zdata_bytes(): input_n = %d output_n = %d output_max = %d data: ",
            *input_n, *output_n, output_max));
    for (i = 0; i < *input_n; i++) {
        DLOG2(("%02x ", (input[i] & 0xFF)));
    }
    DLOG2(("\n"));

    /*
     * Worst-case scenario:  input is twice the output size
     */
    assert((output_max * 2) >= (*input_n));

    /*
     * We need to quickly scan the input and bail out if it too short.
     *
     * The first check is to look for a CRC escape of some kind, if that is
     * missing we are done.
     */
    for (i = 0; (i < *input_n) && (done == Q_FALSE); i++) {
        if (input[i] == C_CAN) {
            /*
             * Point past the CAN
             */
            i++;
            if (i == *input_n) {
                DLOG(("decode_zdata_bytes: incomplete (C_CAN)\n"));
                return Q_FALSE;
            }
            switch (input[i]) {
            case ZCRCE:
            case ZCRCG:
            case ZCRCQ:
            case ZCRCW:
                /*
                 * The CRC escape is here, we can run the big loop
                 */
                goto decode_zdata_bytes_big_loop;
            }
        }
    }

    /*
     * The CRC escape is missing, so we need to bail out now.
     */
    DLOG(("decode_zdata_bytes: incomplete (no CRC escape)\n"));
    return Q_FALSE;

decode_zdata_bytes_big_loop:

    *output_n = 0;
    j = 0;

    for (i = 0; (i < *input_n) && (done == Q_FALSE); i++) {

        if (input[i] == C_CAN) {

            /*
             * Point past the CAN
             */
            i++;

            if (i == *input_n) {
                /*
                 * Uh-oh, missing last byte
                 */
                DLOG(("decode_zdata_bytes: incomplete (C_CAN)\n"));
                return Q_FALSE;
            }
            if (input[i] == ZCRCE) {
                if (doing_crc == Q_TRUE) {
                    /*
                     * WOAH! CRC escape within a CRC escape
                     */
                    return Q_FALSE;
                }

                /*
                 * CRC escape, switch to crc collection
                 */
                doing_crc = Q_TRUE;
                crc_type = input[i];
                i--;

            } else if (input[i] == ZCRCG) {
                if (doing_crc == Q_TRUE) {
                    /*
                     * WOAH! CRC escape within a CRC escape
                     */
                    return Q_FALSE;
                }

                /*
                 * CRC escape, switch to crc collection
                 */
                doing_crc = Q_TRUE;
                crc_type = input[i];
                i--;

            } else if (input[i] == ZCRCQ) {
                if (doing_crc == Q_TRUE) {
                    /*
                     * WOAH! CRC escape within a CRC escape
                     */
                    return Q_FALSE;
                }

                /*
                 * CRC escape, switch to crc collection
                 */
                doing_crc = Q_TRUE;
                crc_type = input[i];
                i--;

            } else if (input[i] == ZCRCW) {
                if (doing_crc == Q_TRUE) {
                    /*
                     * WOAH! CRC escape within a CRC escape
                     */
                    return Q_FALSE;
                }

                /*
                 * CRC escape, switch to crc collection
                 */
                doing_crc = Q_TRUE;
                crc_type = input[i];
                i--;

            } else if (input[i] == 'l') {
                /*
                 * Escaped control character: 0x7f
                 */
                if (doing_crc == Q_TRUE) {
                    crc_buffer[j] = 0x7F;
                    j++;
                } else {
                    output[*output_n] = 0x7F;
                    *output_n = *output_n + 1;
                }

            } else if (input[i] == 'm') {
                /*
                 * Escaped control character: 0xff
                 */
                if (doing_crc == Q_TRUE) {
                    crc_buffer[j] = 0xFF;
                    j++;
                } else {
                    output[*output_n] = 0xFF;
                    *output_n = *output_n + 1;
                }
            } else if ((input[i] & 0x40) != 0) {
                /*
                 * Escaped control character: CAN m OR 0x40
                 */
                if (doing_crc == Q_TRUE) {
                    crc_buffer[j] = input[i] & 0xBF;
                    j++;
                } else {
                    output[*output_n] = input[i] & 0xBF;
                    *output_n = *output_n + 1;
                }
            } else if (input[i] == C_CAN) {
                /*
                 * Real CAN, cancel the transfer
                 */
                status.state = ABORT;
                set_transfer_stats_last_message(
                    _("TRANSFER CANCELLED BY SENDER"));
                stop_file_transfer(Q_TRANSFER_STATE_ABORT);
                return Q_FALSE;
            } else {
                /*
                 * Should never get here
                 */
            }

        } else {
            /*
             * If we're doing the CRC part, put the data elsewhere
             */
            if (doing_crc == Q_TRUE) {
                crc_buffer[j] = input[i];
                j++;
            } else {
                /*
                 * I ought to ignore any unencoded control characters when
                 * encoding was requested at this point here.  However,
                 * encoding control characters is broken anyway in lrzsz so I
                 * won't bother with a further check.  If you want actually
                 * reliable transfer over not-8-bit-clean links, use Kermit
                 * instead.
                 */
                output[*output_n] = input[i];
                *output_n = *output_n + 1;
            }

        }

        if (doing_crc == Q_TRUE) {
            if ((packet.use_crc32 == Q_TRUE) && (j == 5)) {
                /*
                 * Done
                 */
                done = Q_TRUE;
            }
            if ((packet.use_crc32 == Q_FALSE) && (j == 3)) {
                /*
                 * Done
                 */
                done = Q_TRUE;
            }
        }

    } /* for (i=0; i<input_n; i++) */

    DLOG(("decode_zdata_bytes(): i = %d j = %d done = %s\n", i, j,
            (done == Q_TRUE ? "true" : "false")));

    if (done == Q_TRUE) {

        switch (crc_type) {

        case ZCRCE:
            /*
             * CRC next, frame ends, header packet follows
             */
            DLOG(("decode_zdata_bytes(): ZCRCE\n"));
            break;
        case ZCRCG:
            /*
             * CRC next, frame continues nonstop
             */
            DLOG(("decode_zdata_bytes(): ZCRCG\n"));
            break;
        case ZCRCQ:
            /*
             * CRC next, frame continues, ZACK expected
             */
            DLOG(("decode_zdata_bytes(): ZCRCQ\n"));
            break;
        case ZCRCW:
            /*
             * CRC next, ZACK expected, end of frame
             */
            DLOG(("decode_zdata_bytes(): ZCRCW\n"));
            break;
        default:
            DLOG(("!!!! decode_zdata_bytes(): UNKNOWN CRC ESCAPE TYPE !!!!\n"));
            break;
        }
    }

    if (done == Q_TRUE) {
        if (crc_type == ZCRCW) {
            /*
             * ZCRCW is always followed by XON, so kill it
             */
            if (input[i] == C_XON) {
                i++;
            }
        }

        /*
         * Got a packet and it's CRC, shift input down
         */
        memmove(input, input + i, *input_n - i);
        *input_n -= i;

        /*
         * Return OK
         */
        return Q_TRUE;
    }

    /*
     * Reached the end of a packet before we got to the CRC value
     */
    return Q_FALSE;
}

/**
 * encode_byte is a simple lookup into this map.
 */
static unsigned char encode_byte_map[256];

/**
 * Turn one byte into up to two escaped bytes, copying to output.
 *
 * The output buffer must be big enough to contain all the data.
 */
static void setup_encode_byte_map() {

    int ch;

    for (ch = 0; ch < 256; ch++) {

        Q_BOOL encode_char = Q_FALSE;

        /*
         * Oh boy, do we have another design flaw...  lrzsz does not allow
         * any regular characters to be encoded, so we cannot protect against
         * telnet, ssh, and rlogin sequences from breaking the link.
         */
        switch (ch) {

        case C_CAN:
        case C_XON:
        case C_XOFF:
        case (C_XON | 0x80):
        case (C_XOFF | 0x80):
#if 0
            /*
             * lrzsz breaks if we try to escape extra characters
             */
        case 0x1D:             /* For telnet */
        case '~':              /* For ssh, rlogin */
#endif
            encode_char = Q_TRUE;
            break;
        default:
            if ((ch < 0x20) && (status.flags & TX_ESCAPE_CTRL)) {
                /*
                 * 7bit control char, encode only if requested
                 */
                encode_char = Q_TRUE;
            } else if ((ch >= 0x80) && (ch < 0xA0)) {
                /*
                 * 8bit control char, always encode
                 */
                encode_char = Q_TRUE;
            } else if (((ch & 0x80) != 0) && (status.flags & TX_ESCAPE_8BIT)) {
                /*
                 * 8bit char, encode only if requested
                 */
                encode_char = Q_TRUE;
            }
            break;
        }

        if (encode_char == Q_TRUE) {
            /*
             * Encode
             */
            encode_byte_map[ch] = ch | 0x40;
        } else if (ch == 0x7F) {
            /*
             * Escaped control character: 0x7f
             */
            encode_byte_map[ch] = 'l';
        } else if (ch == 0xFF) {
            /*
             * Escaped control character: 0xff
             */
            encode_byte_map[ch] = 'm';
        } else {
            /*
             * Regular character
             */
            encode_byte_map[ch] = ch;
        }
    }

    DLOG(("setup_encode_byte_map():\n"));
    DLOG(("---- \n"));
    for (ch = 0; ch < 256; ch++) {
        DLOG(("From %02x --> To %02x\n", (int) ch, (int) encode_byte_map[ch]));
    }
    DLOG(("---- \n"));

}

/**
 * Turn one byte into up to two escaped bytes, copying to output.  The output
 * buffer must be big enough to contain all the data.
 *
 * @param ch the byte to convert
 * @param output a buffer to contain the encoded byte
 * @param output_n the number of bytes that this function wrote to output
 * @param output_max the maximum size of the output buffer
 */
static void encode_byte(const unsigned char ch, unsigned char * output,
                        unsigned int * output_n,
                        const unsigned int output_max) {

    unsigned char new_ch = encode_byte_map[ch];

    /*
     * Check for space
     */
    assert(*output_n + 2 <= output_max);

    if (new_ch != ch) {
        /*
         * Encode
         */
        output[*output_n] = C_CAN;
        *output_n = *output_n + 1;
        output[*output_n] = new_ch;
        *output_n = *output_n + 1;
    } else {
        /*
         * Regular character
         */
        output[*output_n] = ch;
        *output_n = *output_n + 1;
    }

}

/**
 * Turn regular bytes into escaped bytes, copying to output.  The output
 * buffer must be big enough to contain all the data.
 *
 * @param output a buffer to contain the encoded byte
 * @param output_n the number of bytes that this function wrote to output
 * @param output_max the maximum size of the output buffer
 * @param crc_type ZCRCE, ZCRCG, ZCRCQ, or ZCRCW
 */
static void encode_zdata_bytes(unsigned char * output,
                               unsigned int * output_n,
                               const unsigned int output_max,
                               const unsigned char crc_type) {

    unsigned int i;             /* input iterator */
    unsigned int j;             /* CRC32 iterator */
    int crc_16;
    uint32_t crc_32;
    Q_BOOL doing_crc = Q_FALSE;
    unsigned int crc_length = 0;
    unsigned char ch;
    unsigned char crc_buffer[4];

    DLOG(("encode_zdata_bytes(): packet.type = %d packet.use_crc32 = %s packet.data_n = %d output_n = %d output_max = %d data: ",
            packet.type, (packet.use_crc32 == Q_TRUE ? "true" : "false"),
            packet.data_n, *output_n, output_max));
    for (i = 0; i < packet.data_n; i++) {
        DLOG2(("%02x ", (packet.data[i] & 0xFF)));
    }
    DLOG2(("\n"));

    for (i = 0; ; i++) {
        if (doing_crc == Q_FALSE) {

            if (i == packet.data_n) {

                /*
                 * Add the link escape sequence
                 */
                output[*output_n] = C_CAN;
                *output_n = *output_n + 1;
                output[*output_n] = crc_type;
                *output_n = *output_n + 1;

                /*
                 * Compute the CRC
                 */
                if ((packet.use_crc32 == Q_TRUE) && (packet.type != P_ZSINIT)) {

                    crc_length = 4;
                    crc_32 = compute_crc32(0, NULL, 0);

                    /*
                     * Another case of *strange* CRC behavior...
                     */
                    for (j = 0; j < packet.data_n; j++) {
                        crc_32 = ~compute_crc32(crc_32, packet.data + j, 1);
                    }
                    crc_32 = ~compute_crc32(crc_32, &crc_type, 1);
                    crc_32 = ~crc_32;

                    DLOG(("encode_zdata_bytes(): DATA CRC32: %08x\n", crc_32));

                    /*
                     * Little-endian
                     */
                    crc_buffer[0] = (unsigned char) ( crc_32        & 0xFF);
                    crc_buffer[1] = (unsigned char) ((crc_32 >>  8) & 0xFF);
                    crc_buffer[2] = (unsigned char) ((crc_32 >> 16) & 0xFF);
                    crc_buffer[3] = (unsigned char) ((crc_32 >> 24) & 0xFF);

                } else {
                    /*
                     * 16-bit CRC
                     */
                    crc_length = 2;
                    crc_16 = 0;
                    crc_16 = compute_crc16(crc_16, packet.data, packet.data_n);
                    crc_16 = compute_crc16(crc_16, &crc_type, 1);

                    DLOG(("encode_zdata_bytes(): DATA CRC16: %04x\n", crc_16));

                    /*
                     * Big-endian
                     */
                    crc_buffer[0] = (unsigned char) ((crc_16 >> 8) & 0xFF);
                    crc_buffer[1] = (unsigned char) ( crc_16       & 0xFF);
                }

                doing_crc = Q_TRUE;
                i = -1;
                continue;
            } else {
                ch = packet.data[i];
            }
        } else {
            if (i >= crc_length) {
                break;
            }
            ch = crc_buffer[i];
        }

        /*
         * Encode the byte
         */
        encode_byte(ch, output, output_n, output_max);

    } /* for (i = 0; i < packet.data_n; i++) */

    /*
     * One type of packet is terminated "special"
     */
    if (crc_type == ZCRCW) {
        output[*output_n] = C_XON;
        *output_n = *output_n + 1;
    }
    DLOG(("encode_zdata_bytes(): i = %d *output_n = %d data: ", i, *output_n));
    for (i = 0; i < *output_n; i++) {
        DLOG2(("%02x ", (output[i] & 0xFF)));
    }
    DLOG2(("\n"));

}

/* ------------------------------------------------------------------------ */
/* Packet layer ----------------------------------------------------------- */
/* ------------------------------------------------------------------------ */

#define HEX_PACKET_LENGTH       20

/**
 * Build a Zmodem packet.
 *
 * @param type the packet type
 * @param argument a 32-bit argument
 * @param data_packet the buffer to write the packet to
 * @param data_packet_n the size of data_packet
 * @param data_packet_max the maximum size of data_packet
 */
static void build_packet(const int type, const long argument,
                         unsigned char * data_packet,
                         unsigned int * data_packet_n,
                         const int data_packet_max) {

    int crc_16;
    unsigned char crc_16_hex[4];
    uint32_t crc_32;
    unsigned char header[10];
    Q_BOOL do_hex;
    int i;
    char * type_string;

    switch (type) {

    case P_ZRQINIT:
        type_string = "ZRQINIT";
        break;
    case P_ZRINIT:
        type_string = "ZRINIT";
        break;
    case P_ZSINIT:
        type_string = "ZSINIT";
        break;
    case P_ZACK:
        type_string = "ZACK";
        break;
    case P_ZFILE:
        type_string = "ZFILE";
        break;
    case P_ZSKIP:
        type_string = "ZSKIP";
        break;
    case P_ZNAK:
        type_string = "ZNAK";
        break;
    case P_ZABORT:
        type_string = "ZABORT";
        break;
    case P_ZFIN:
        type_string = "ZFIN";
        break;
    case P_ZRPOS:
        type_string = "ZRPOS";
        break;
    case P_ZDATA:
        type_string = "ZDATA";
        break;
    case P_ZEOF:
        type_string = "ZEOF";
        break;
    case P_ZFERR:
        type_string = "ZFERR";
        break;
    case P_ZCRC:
        type_string = "ZCRC";
        break;
    case P_ZCHALLENGE:
        type_string = "ZCHALLENGE";
        break;
    case P_ZCOMPL:
        type_string = "ZCOMPL";
        break;
    case P_ZCAN:
        type_string = "ZCAN";
        break;
    case P_ZFREECNT:
        type_string = "ZFREECNT";
        break;
    case P_ZCOMMAND:
        type_string = "ZCOMMAND";
        break;

    default:
        type_string = "Invalid type";
    }

    DLOG(("build_packet(): type = %s (%d) argument = %08lx\n", type_string,
            type, argument));

    /*
     * Initialize the packet
     */
    packet.type = type;
    packet.use_crc32 = status.use_crc32;
    packet.data_n = 0;

    /*
     * Copy type to first header byte
     */
    header[0] = type;

    switch (type) {

    case P_ZRPOS:
    case P_ZEOF:
    case P_ZCRC:
    case P_ZCOMPL:
    case P_ZFREECNT:
    case P_ZSINIT:
        /*
         * Little endian order
         */
        header[4] = (argument >> 24) & 0xFF;
        header[3] = (argument >> 16) & 0xFF;
        header[2] = (argument >> 8) & 0xFF;
        header[1] = argument & 0xFF;
        break;

    default:
        /*
         * Everything else is in big endian order
         */
        header[1] = (argument >> 24) & 0xFF;
        header[2] = (argument >> 16) & 0xFF;
        header[3] = (argument >> 8) & 0xFF;
        header[4] = argument & 0xFF;
        break;
    }

    switch (type) {

    case P_ZRQINIT:
    case P_ZRINIT:
    case P_ZSINIT:
    case P_ZCHALLENGE:
        /*
         * ZCHALLENGE comes before the CRC32 negotiation, so it must use hex
         * packets.  The other packets are defined by the standard to be hex.
         */
    case P_ZRPOS:

        do_hex = Q_TRUE;
        break;

    default:
        if ((status.flags & TX_ESCAPE_CTRL) ||
            (status.flags & TX_ESCAPE_8BIT)
        ) {
            do_hex = Q_TRUE;
        } else {
            do_hex = Q_FALSE;
        }
        break;
    }

    /*
     * OK, so we can get seriously out of sync with rz -- it doesn't bother
     * checking to see if ZSINIT is CRC32 or not.  So we have to see what it
     * expects and encode appropriately.
     */
    if ((type == P_ZSINIT) &&
        (status.sending == Q_TRUE) &&
        (status.use_crc32 == Q_TRUE)
    ) {
        do_hex = Q_FALSE;
    }

    /*
     * A bug in sz: it sometimes loses the ZCRC even though it reads the
     * bytes.
     */
    if ((type == P_ZCRC) && (status.sending == Q_FALSE)) {
        do_hex = Q_TRUE;
    }

    if (do_hex == Q_TRUE) {

        /*
         * Hex must be 16-bit CRC, override the default setting
         */
        packet.use_crc32 = Q_FALSE;

        /*
         * Hex packets
         */
        data_packet[0] = ZPAD;
        data_packet[1] = ZPAD;
        data_packet[2] = C_CAN;
        data_packet[3] = 'B';

        hexify_string(header, 5, &data_packet[4], HEX_PACKET_LENGTH - 10);
        *data_packet_n = *data_packet_n + HEX_PACKET_LENGTH;

        /*
         * Hex packets always use 16-bit CRC
         */
        crc_16 = compute_crc16(0, header, 5);
        crc_16_hex[0] = (crc_16 >> 8) & 0xFF;
        crc_16_hex[1] = crc_16 & 0xFF;
        hexify_string(crc_16_hex, 2, &data_packet[14], HEX_PACKET_LENGTH - 14);

        data_packet[18] = C_CR;
        /*
         * lrzsz flips the high bit here.  Why??
         */
        /* data_packet[19] = C_LF; */
        data_packet[19] = C_LF | 0x80;

        switch (type) {
        case P_ZFIN:
        case P_ZACK:
            break;
        default:
            /*
             * Append XON to most hex packets
             */
            data_packet[(*data_packet_n)] = C_XON;
            *data_packet_n = *data_packet_n + 1;
            break;
        }

    } else {
        Q_BOOL altered_encode_byte_map = Q_FALSE;
        uint32_t old_flags = status.flags;

        if (type == P_ZSINIT) {
            /*
             * Special case: lrzsz needs control characters escaped in the
             * ZSINIT.
             */
            if (!(status.flags & TX_ESCAPE_CTRL)) {
                altered_encode_byte_map = Q_TRUE;

                /*
                 * Update the encode map
                 */
                status.flags |= TX_ESCAPE_CTRL;
                setup_encode_byte_map();
            }
        }

        /*
         * Binary packets
         */

        data_packet[0] = ZPAD;
        data_packet[1] = C_CAN;
        if (status.use_crc32 == Q_TRUE) {
            data_packet[2] = 'C';
        } else {
            data_packet[2] = 'A';
        }

        /*
         * Set initial length
         */
        *data_packet_n = *data_packet_n + 3;

        /*
         * Encode the argument field
         */
        for (i = 0; i < 5; i++) {
            encode_byte((unsigned char) (header[i] & 0xFF), data_packet,
                        data_packet_n, data_packet_max);
        }

        if (packet.use_crc32 == Q_TRUE) {
            crc_32 = compute_crc32(0, NULL, 0);
            crc_32 = compute_crc32(crc_32, header, 5);
            /*
             * Little-endian
             */
            encode_byte((unsigned char) ( crc_32        & 0xFF),
                        data_packet, data_packet_n, data_packet_max);
            encode_byte((unsigned char) ((crc_32 >>  8) & 0xFF),
                        data_packet, data_packet_n, data_packet_max);
            encode_byte((unsigned char) ((crc_32 >> 16) & 0xFF),
                        data_packet, data_packet_n, data_packet_max);
            encode_byte((unsigned char) ((crc_32 >> 24) & 0xFF),
                        data_packet, data_packet_n, data_packet_max);
        } else {
            crc_16 = compute_crc16(0, header, 5);
            encode_byte((unsigned char) ((crc_16 >> 8) & 0xFF),
                        data_packet, data_packet_n, data_packet_max);
            encode_byte((unsigned char) ( crc_16       & 0xFF),
                        data_packet, data_packet_n, data_packet_max);
        }

        if (altered_encode_byte_map == Q_TRUE) {
            /*
             * Restore encode_byte_map and flags
             */
            status.flags = old_flags;
            setup_encode_byte_map();
        }

    }

    /*
     * Make sure we're still OK
     */
    assert(*data_packet_n <= data_packet_max);
}


/**
 * Parse a Zmodem packet.
 *
 * @param input the bytes from the remote side
 * @param input_n the number of bytes in input_n
 * @param discard the number of bytes that were consumed by the input
 * @return a parse packet return code (OK, CRC error, etc.)
 */
static ZM_PARSE_PACKET parse_packet(const unsigned char * input,
                                    const int input_n, int * discard) {

    char * type_string;
    int begin = 0;
    unsigned char crc_header[5];
    uint32_t crc_32;
    int crc_16;
    Q_BOOL has_data = Q_FALSE;
    int i;
    Q_BOOL got_can = Q_FALSE;
    unsigned char ch;
    unsigned char hex_buffer[4];

    DLOG(("parse_packet()\n"));

    /*
     * Clear packet
     */
    memset(&packet, 0, sizeof(struct zmodem_packet));

    /*
     * Find the start of the packet
     */
    while (input[begin] != ZPAD) {
        /*
         * Strip non-ZPAD characters
         */
        begin++;
        if (begin >= input_n) {
            /*
             * Throw away what's here, we're still looking for a packet
             * beginning.
             */
            *discard = begin;
            return ZM_PP_NODATA;
        }
    }

    /*
     * Throw away up to the packet beginning
     */
    *discard = begin;

    while (input[begin] == ZPAD) {
        /*
         * Strip ZPAD characters
         */
        begin++;
        if (begin >= input_n) {
            return ZM_PP_NODATA;
        }
    }

    /*
     * Pull into fields
     */
    if (input[begin] != C_CAN) {
        *discard = *discard + 1;
        return ZM_PP_INVALID;
    }
    begin++;
    if (begin >= input_n) {
        return ZM_PP_NODATA;
    }

    if (input[begin] == 'A') {
        /*
         * CRC-16
         */
        if (input_n - begin < 8) {
            return ZM_PP_NODATA;
        }

        packet.use_crc32 = Q_FALSE;

        packet.argument = 0;
        packet.crc16 = 0;

        begin += 1;
        for (i = 0; i < 7; i++, begin++) {
            if (begin >= input_n) {
                return ZM_PP_NODATA;
            }

            if (input[begin] == C_CAN) {
                /*
                 * Escape control char
                 */
                got_can = Q_TRUE;
                i--;
                continue;
            }

            if (got_can == Q_TRUE) {
                got_can = Q_FALSE;
                if (input[begin] == 'l') {
                    /*
                     * Escaped control character: 0x7f
                     */
                    ch = 0x7F;
                } else if (input[begin] == 'm') {
                    /*
                     * Escaped control character: 0xff
                     */
                    ch = 0xFF;
                } else if ((input[begin] & 0x40) != 0) {
                    /*
                     * Escaped control character: CAN m OR 0x40
                     */
                    ch = input[begin] & 0xBF;
                } else {
                    /*
                     * Should never get here
                     */
                    return ZM_PP_INVALID;
                }
            } else {
                ch = input[begin];
            }

            if (i == 0) {
                /*
                 * Type
                 */
                packet.type = ch;
                crc_header[0] = packet.type;
            } else if (i < 5) {
                /*
                 * Argument
                 */
                packet.argument |= (ch << (32 - (8 * i)));
                crc_header[i] = ch;
            } else {
                /*
                 * CRC
                 */
                packet.crc16 |= (ch << (16 - (8 * (i - 4))));
            }
        }

    } else if (input[begin] == 'B') {

        /*
         * CRC-16 HEX
         */
        begin++;
        if (input_n - begin < 14 + 2) {
            return ZM_PP_NODATA;
        }

        packet.use_crc32 = Q_FALSE;

        /*
         * Dehexify
         */
        memset(hex_buffer, 0, sizeof(hex_buffer));
        if (dehexify_string(&input[begin], 2, hex_buffer,
                sizeof(hex_buffer)) == Q_FALSE) {
            return ZM_PP_INVALID;
        }
        packet.type = hex_buffer[0];

        memset(hex_buffer, 0, sizeof(hex_buffer));
        if (dehexify_string(&input[begin + 2], 8, hex_buffer,
                sizeof(hex_buffer)) == Q_FALSE) {
            return ZM_PP_INVALID;
        }
        packet.argument = ((hex_buffer[0] & 0xFF) << 24) |
                          ((hex_buffer[1] & 0xFF) << 16) |
                          ((hex_buffer[2] & 0xFF) <<  8) |
                           (hex_buffer[3] & 0xFF);

        memset(hex_buffer, 0, sizeof(hex_buffer));
        if (dehexify_string(&input[begin + 10], 4, hex_buffer,
                sizeof(hex_buffer)) == Q_FALSE) {
            return ZM_PP_INVALID;
        }
        packet.crc16 = ((hex_buffer[0] & 0xFF) << 8) | (hex_buffer[1] & 0xFF);

        /*
         * Point to end
         */
        begin += 14;

        /*
         * Copy header to crc_header
         */
        crc_header[0] = packet.type;
        crc_header[1] = (unsigned char) ((packet.argument >> 24) & 0xFF);
        crc_header[2] = (unsigned char) ((packet.argument >> 16) & 0xFF);
        crc_header[3] = (unsigned char) ((packet.argument >>  8) & 0xFF);
        crc_header[4] = (unsigned char) ( packet.argument        & 0xFF);

        /*
         * More special-case junk: sz sends 0d 8a at the end of each hex
         * header.
         */
        begin += 2;

        /*
         * sz also sends XON at the end of each hex header except ZFIN and
         * ZACK.
         */
        switch (packet.type) {
        case P_ZFIN:
        case P_ZACK:
            break;
        default:
            if (input_n - begin < 1) {
                return ZM_PP_NODATA;
            }
            begin++;
            break;
        }

    } else if (input[begin] == 'C') {
        /*
         * CRC-32
         */
        if (input_n - begin < 10) {
            return ZM_PP_NODATA;
        }

        packet.use_crc32 = Q_TRUE;
        packet.argument = 0;
        packet.crc32 = 0;

        /*
         * Loop through the type, argument, and crc values, unescaping
         * control characters along the way
         */
        begin += 1;
        for (i = 0; i < 9; i++, begin++) {
            if (begin >= input_n) {
                return ZM_PP_NODATA;
            }

            if (input[begin] == C_CAN) {
                /*
                 * Escape control char
                 */
                got_can = Q_TRUE;
                i--;
                continue;
            }

            if (got_can == Q_TRUE) {
                got_can = Q_FALSE;
                if (input[begin] == 'l') {
                    /*
                     * Escaped control character: 0x7f
                     */
                    ch = 0x7F;
                } else if (input[begin] == 'm') {
                    /*
                     * Escaped control character: 0xff
                     */
                    ch = 0xFF;
                } else if ((input[begin] & 0x40) != 0) {
                    /*
                     * Escaped control character: CAN m OR 0x40
                     */
                    ch = input[begin] & 0xBF;
                } else {
                    /*
                     * Should never get here
                     */
                    return ZM_PP_NODATA;
                }
            } else {
                ch = input[begin];
            }

            if (i == 0) {
                /*
                 * Type
                 */
                packet.type = ch;
                crc_header[0] = packet.type;
            } else if (i < 5) {
                /*
                 * Argument
                 */
                packet.argument |= (ch << (32 - (8 * i)));
                crc_header[i] = ch;
            } else {
                /*
                 * CRC - in little-endian form
                 */
                packet.crc32 |= (ch << (8 * (i - 5)));
            }

        }

    } else {
        /*
         * Invalid packet type
         */
        *discard = *discard + 1;
        return ZM_PP_INVALID;
    }

    /*
     * Type
     */
    switch (packet.type) {

    case P_ZRQINIT:
        type_string = "ZRQINIT";
        break;
    case P_ZRINIT:
        type_string = "ZRINIT";
        break;
    case P_ZSINIT:
        type_string = "ZSINIT";
        break;
    case P_ZACK:
        type_string = "ZACK";
        break;
    case P_ZFILE:
        type_string = "ZFILE";
        break;
    case P_ZSKIP:
        type_string = "ZSKIP";
        break;
    case P_ZNAK:
        type_string = "ZNAK";
        break;
    case P_ZABORT:
        type_string = "ZABORT";
        break;
    case P_ZFIN:
        type_string = "ZFIN";
        break;
    case P_ZRPOS:
        type_string = "ZRPOS";
        break;
    case P_ZDATA:
        type_string = "ZDATA";
        break;
    case P_ZEOF:
        type_string = "ZEOF";
        break;
    case P_ZFERR:
        type_string = "ZFERR";
        break;
    case P_ZCRC:
        type_string = "ZCRC";
        break;
    case P_ZCHALLENGE:
        type_string = "ZCHALLENGE";
        break;
    case P_ZCOMPL:
        type_string = "ZCOMPL";
        break;
    case P_ZCAN:
        type_string = "ZCAN";
        break;
    case P_ZFREECNT:
        type_string = "ZFREECNT";
        break;
    case P_ZCOMMAND:
        type_string = "ZCOMMAND";
        break;

    default:
        DLOG(("parse_packet(): INVALID PACKET TYPE %d\n", packet.type));
        return ZM_PP_INVALID;
    }

    /*
     * Figure out if the argument is supposed to be flipped
     */
    switch (packet.type) {

    case P_ZRPOS:
    case P_ZEOF:
    case P_ZCRC:
    case P_ZCOMPL:
    case P_ZFREECNT:
        /*
         * Swap the packet argument around
         */
        packet.argument = big_to_little_endian(packet.argument);
        break;

    default:
        break;
    }

    if (packet.use_crc32 == Q_TRUE) {
        DLOG(("parse_packet(): CRC32 type = %s (%d) argument=%08x crc=%08x\n",
                type_string, packet.type, packet.argument, packet.crc32));
    } else {
        DLOG(("parse_packet(): CRC16 type = %s (%d) argument=%08x crc=%04x\n",
                type_string, packet.type, packet.argument, packet.crc16));
    }

    /*
     * Check CRC
     */
    if (packet.use_crc32 == Q_TRUE) {
        crc_32 = compute_crc32(0, NULL, 0);
        crc_32 = compute_crc32(crc_32, crc_header, 5);
        if (crc_32 != packet.crc32) {
            DLOG(("parse_packet(): CRC ERROR (given=%08x computed=%08x)\n",
                    packet.crc32, crc_32));
            stats_increment_errors(_("CRC ERROR"));
            return ZM_PP_CRCERROR;
        }

    } else {
        crc_16 = compute_crc16(0, crc_header, 5);
        if (crc_16 != packet.crc16) {
            DLOG(("parse_packet(): CRC ERROR (given=%04x computed=%04x)\n",
                    packet.crc16, crc_16));
            stats_increment_errors(_("CRC ERROR"));
            return ZM_PP_CRCERROR;
        }

    }

    DLOG(("parse_packet(): CRC OK\n"));

    /*
     * Pull data for certain packet types
     */
    switch (packet.type) {
    case P_ZSINIT:
    case P_ZFILE:
    case P_ZDATA:
    case P_ZCOMMAND:
        /*
         * Packet data will follow
         */
        has_data = Q_TRUE;
        break;
    default:
        break;
    }

    /*
     * Discard what's been processed
     */
    *discard = begin;

    if (has_data == Q_TRUE) {
        status.prior_state = status.state;
        status.state = ZDATA;
        packet.data_n = 0;
        packet.crc16 = 0;
        packet.crc32 = compute_crc32(0, NULL, 0);
    }

    /*
     * All OK
     */
    return ZM_PP_OK;
}

/* ------------------------------------------------------------------------ */
/* Top-level states ------------------------------------------------------- */
/* ------------------------------------------------------------------------ */

/**
 * Receive:  ZCHALLENGE
 *
 * @param output a buffer to write bytes to send to the remote side
 * @param output_n the number of bytes that this function wrote to output
 * @param output_max the maximum size of the output buffer
 * @return true if we are done reading input and are ready to send bytes to
 * the remote side
 */
static Q_BOOL receive_zchallenge(unsigned char * output,
                                 unsigned int * output_n,
                                 const unsigned int output_max) {

    uint32_t options;
    FILE * dev_random;

    DLOG(("receive_zchallenge()\n"));

    /*
     * Build a random value for ZCHALLENGE using /dev/random
     */
    dev_random = fopen("/dev/random", "r");
    if (dev_random == NULL) {
        /*
         * /dev/random isn't here, or isn't readable.  Use random() instead,
         * even though it probably sucks.
         */
#ifdef USE_WIN32API
        zchallenge_value = rand();
#else
        zchallenge_value = random();
#endif
    } else {
        fread(&zchallenge_value, sizeof(uint32_t), 1, dev_random);
        fclose(dev_random);
    }

    DLOG(("receive_zchallenge() VALUE = %08x\n", zchallenge_value));

    options = zchallenge_value;
    build_packet(P_ZCHALLENGE, options, output, output_n, output_max);
    status.state = ZCHALLENGE_WAIT;

    /*
     * Discard input bytes
     */
    packet_buffer_n = 0;

    /*
     * Process through the new state
     */
    return Q_FALSE;
}

/**
 * Receive:  ZCHALLENGE_WAIT
 *
 * @param output a buffer to write bytes to send to the remote side
 * @param output_n the number of bytes that this function wrote to output
 * @param output_max the maximum size of the output buffer
 * @return true if we are done reading input and are ready to send bytes to
 * the remote side
 */
static Q_BOOL receive_zchallenge_wait(unsigned char * output,
                                      unsigned int * output_n,
                                      const unsigned int output_max) {

    ZM_PARSE_PACKET rc_pp;
    uint32_t options = 0;
    int discard;

    DLOG(("receive_zchallenge_wait()\n"));

    if (packet_buffer_n > 0) {
        rc_pp = parse_packet(packet_buffer, packet_buffer_n, &discard);

        /*
         * Take the bytes off the stream
         */
        if (discard > 0) {
            memmove(packet_buffer, packet_buffer + discard,
                    packet_buffer_n - discard);
            packet_buffer_n -= discard;
        }

        if ((rc_pp == ZM_PP_CRCERROR) || (rc_pp == ZM_PP_INVALID)) {
            DLOG(("receive_zchallenge_wait(): ERROR garbled header\n"));
            stats_increment_errors(_("GARBLED HEADER"));
            packet_buffer_n = 0;
            build_packet(P_ZNAK, options, output, output_n, output_max);
            return Q_TRUE;
        }

        if (rc_pp == ZM_PP_NODATA) {
            return Q_TRUE;
        }

        if (rc_pp == ZM_PP_OK) {

            if (packet.type == P_ZACK) {
                /*
                 * Verify the value returned
                 */
                DLOG(("receive_zchallenge_wait() argument = %08x correct = %08x\n",
                        packet.argument, zchallenge_value));

                if (packet.argument == zchallenge_value) {
                    set_transfer_stats_last_message(_("ZCHALLENGE -- OK"));
                    /*
                     * I'd love to wait a second here so the user can see the
                     * successful ZCHALLENGE response on the transfer
                     * screen...
                     */

                    /*
                     * Send the ZRINIT
                     */
                    set_transfer_stats_last_message("ZRINIT");
                    status.state = ZRINIT;
                    packet.crc16 = 0;
                    packet.crc32 = compute_crc32(0, NULL, 0);
                    return Q_FALSE;

                } else {
                    DLOG(("receive_zchallenge_wait(): ERROR zchallenge error\n"));
                    stats_increment_errors(_("ZCHALLENGE -- ERROR"));
                    status.state = ABORT;
                    stop_file_transfer(Q_TRANSFER_STATE_ABORT);
                    return Q_TRUE;
                }

            } else if (packet.type == P_ZNAK) {
                DLOG(("receive_zchallenge_wait(): ERROR ZNAK\n"));
                /*
                 * Re-send
                 */
                stats_increment_errors("ZNAK");
                status.state = ZCHALLENGE;

            } else if (packet.type == P_ZRQINIT) {
                DLOG(("receive_zchallenge_wait(): ERROR ZRQINIT\n"));
                /*
                 * Re-send, but don't count as an error
                 */
                set_transfer_stats_last_message("ZRQINIT");
                status.state = ZCHALLENGE;

            } else {
                /*
                 * Sender isn't Zmodem compliant, abort.
                 */
                status.state = ABORT;
                stop_file_transfer(Q_TRANSFER_STATE_ABORT);
                return Q_TRUE;
            }
        }

        /*
         * Process through the new state
         */
        return Q_FALSE;
    }

    if (check_timeout() == Q_TRUE) {
        status.state = ZCHALLENGE;
        return Q_FALSE;
    }

    /*
     * No data, done
     */
    return Q_TRUE;

}

/**
 * Receive:  ZCRC
 *
 * @param output a buffer to write bytes to send to the remote side
 * @param output_n the number of bytes that this function wrote to output
 * @param output_max the maximum size of the output buffer
 * @return true if we are done reading input and are ready to send bytes to
 * the remote side
 */
static Q_BOOL receive_zcrc(unsigned char * output,
                           unsigned int * output_n,
                           const unsigned int output_max) {

    /*
     * Buffer for reading the file
     */
    unsigned char file_buffer[1];
    size_t file_buffer_n;

    /*
     * Save the original file position
     */
    off_t original_position = status.file_position;
    int total_bytes = 0;

    DLOG(("receive_zcrc() ENTER\n"));

    /*
     * Reset crc32
     */
    status.file_crc32 = compute_crc32(0, NULL, 0);

    /*
     * Seek to beginning of file
     */
    fseek(status.file_stream, 0, SEEK_SET);
    while (!feof(status.file_stream)) {
        file_buffer_n =
            fread(file_buffer, 1, sizeof(file_buffer), status.file_stream);
        total_bytes += file_buffer_n;
        /*
         * I think I have a different CRC function from lrzsz...  I have to
         * negate both here and below to get the same value.
         */
        status.file_crc32 =
            ~compute_crc32(status.file_crc32, file_buffer, file_buffer_n);
    }
    /*
     * Seek back to the original location
     */
    fseek(status.file_stream, original_position, SEEK_SET);

    status.file_crc32 = ~status.file_crc32;

    DLOG(("receive_zcrc() total_bytes = %d on-disk CRC32 = %08lx\n",
            total_bytes, (unsigned long) status.file_crc32));

    build_packet(P_ZCRC, total_bytes, output, output_n, output_max);
    status.state = ZCRC_WAIT;
    packet_buffer_n = 0;
    return Q_FALSE;
}

/**
 * Receive:  ZCRC_WAIT
 *
 * @param output a buffer to write bytes to send to the remote side
 * @param output_n the number of bytes that this function wrote to output
 * @param output_max the maximum size of the output buffer
 * @return true if we are done reading input and are ready to send bytes to
 * the remote side
 */
static Q_BOOL receive_zcrc_wait(unsigned char * output,
                                unsigned int * output_n,
                                const unsigned int output_max) {

    ZM_PARSE_PACKET rc_pp;

    struct stat fstats;
    int i;
    int rc;
    uint32_t options = 0;
    int discard;

    DLOG(("receive_zcrc_wait()\n"));

    if (packet_buffer_n > 0) {
        rc_pp = parse_packet(packet_buffer, packet_buffer_n, &discard);

        /*
         * Take the bytes off the stream
         */
        if (discard > 0) {
            memmove(packet_buffer, packet_buffer + discard,
                    packet_buffer_n - discard);
            packet_buffer_n -= discard;
        }

        if ((rc_pp == ZM_PP_CRCERROR) || (rc_pp == ZM_PP_INVALID)) {
            DLOG(("receive_zcrc_wait(): ERROR garbled header\n"));
            stats_increment_errors(_("GARBLED HEADER"));
            packet_buffer_n = 0;
            build_packet(P_ZNAK, options, output, output_n, output_max);
            return Q_TRUE;
        }

        if (rc_pp == ZM_PP_NODATA) {
            return Q_TRUE;
        }

        if (rc_pp == ZM_PP_OK) {

            if (packet.type == P_ZCRC) {
                /*
                 * Verify the value returned
                 */
                DLOG(("receive_zcrc_wait() argument = %08x correct = %08lx\n",
                        packet.argument, (unsigned long) status.file_crc32));

                if (packet.argument == status.file_crc32) {
                    /*
                     * We're working on the same file, check its length to
                     * see if we should send ZRPOS or ZSKIP.
                     */

                    if (status.file_size == status.file_position) {
                        DLOG(("receive_zcrc_wait(): got file, switch to ZSKIP\n"));
                        status.state = ZSKIP;

                    } else {
                        DLOG(("receive_zcrc_wait(): crash recovery, switch to ZRPOS\n"));
                        status.state = ZRPOS;
                    }

                } else {

                    /*
                     * This is a different file, rename it
                     */
                    for (i = 0; ; i++) {
                        /*
                         * Add a numeric suffix and see if the file doesn't
                         * exist.  If so, we can use that filename.
                         */
                        sprintf(status.file_fullname, "%s/%s.%04d",
                                download_path, status.file_name, i);

                        rc = stat(status.file_fullname, &fstats);
                        if (rc < 0) {
                            if (errno == ENOENT) {
                                /*
                                 * We found a free filename, break out and
                                 * try to create it.
                                 */

                                DLOG(("receive_zcrc_wait(): prevent overwrite, switch to ZRPOS, new filename = %s\n",
                                        status.file_fullname));
                                break;

                            } else {
                                status.state = ABORT;
                                set_transfer_stats_last_message(
                                    _("DISK I/O ERROR"));
                                stop_file_transfer(Q_TRANSFER_STATE_ABORT);
                                return Q_TRUE;
                            }
                        }
                    } /* for (i = 0; ; i++) */

                    status.file_position = 0;
                    status.file_stream = fopen(status.file_fullname, "w+b");
                    if (status.file_stream == NULL) {
                        status.state = ABORT;
                        set_transfer_stats_last_message(
                            _("CANNOT CREATE FILE"));
                        stop_file_transfer(Q_TRANSFER_STATE_ABORT);
                        return Q_TRUE;
                    }

                    /*
                     * Seek to the end
                     */
                    fseek(status.file_stream, 0, SEEK_END);

                    /*
                     * Update progress display
                     */
                    stats_new_file(status.file_fullname, status.file_size);

                    /*
                     * Ready for ZRPOS now
                     */
                    status.state = ZRPOS;
                }

                /*
                 * At this point we have switched to ZRPOS or ZSKIP, either
                 * with the original filename (crash recovery) or a new one
                 * (different file).
                 */

            } else if (packet.type == P_ZNAK) {
                DLOG(("receive_zcrc_wait(): ERROR ZNAK\n"));
                stats_increment_errors("ZNAK");
                status.state = ZCRC;

            } else if (packet.type == P_ZFILE) {
                DLOG(("receive_zcrc_wait(): ZFILE sender does not understand ZCRC, move to crash recovery (even though this may corrupt the file)\n"));
                stats_increment_errors("Sender does not understand ZCRC!");
                status.state = ZRPOS;

            } else {
                /*
                 * Sender isn't Zmodem compliant, abort.
                 */
                status.state = ABORT;
                stop_file_transfer(Q_TRANSFER_STATE_ABORT);
                return Q_TRUE;
            }
        }

        /*
         * Process through the new state
         */
        return Q_FALSE;

    }

    if (check_timeout() == Q_TRUE) {
        status.state = ZCRC;
        return Q_FALSE;
    }

    /*
     * No data, done
     */
    return Q_TRUE;

}

/**
 * Receive:  ZRINIT
 *
 * @param output a buffer to write bytes to send to the remote side
 * @param output_n the number of bytes that this function wrote to output
 * @param output_max the maximum size of the output buffer
 * @return true if we are done reading input and are ready to send bytes to
 * the remote side
 */
static Q_BOOL receive_zrinit(unsigned char * output,
                             unsigned int * output_n,
                             const unsigned int output_max) {

    uint32_t options;

    DLOG(("receive_zrinit()\n"));

    options = TX_CAN_FULL_DUPLEX | TX_CAN_OVERLAP_IO;
    if (status.use_crc32 == Q_TRUE) {
        options |= TX_CAN_CRC32;
    }
    if (zmodem_escape_ctrl == Q_TRUE) {
        options |= TX_ESCAPE_CTRL;
    }
    status.flags = options;
    build_packet(P_ZRINIT, options, output, output_n, output_max);
    status.state = ZRINIT_WAIT;
    packet_buffer_n = 0;
    return Q_FALSE;
}

/**
 * Receive:  ZRINIT_WAIT
 *
 * @param output a buffer to write bytes to send to the remote side
 * @param output_n the number of bytes that this function wrote to output
 * @param output_max the maximum size of the output buffer
 * @return true if we are done reading input and are ready to send bytes to
 * the remote side
 */
static Q_BOOL receive_zrinit_wait(unsigned char * output,
                                  unsigned int * output_n,
                                  const unsigned int output_max) {

    ZM_PARSE_PACKET rc_pp;

    int discard;
    uint32_t options = 0;

    DLOG(("receive_zrinit_wait()\n"));

    if (packet_buffer_n > 0) {
        rc_pp = parse_packet(packet_buffer, packet_buffer_n, &discard);

        /*
         * Take the bytes off the stream
         */
        if (discard > 0) {
            memmove(packet_buffer, packet_buffer + discard,
                    packet_buffer_n - discard);
            packet_buffer_n -= discard;
        }

        if ((rc_pp == ZM_PP_CRCERROR) || (rc_pp == ZM_PP_INVALID)) {
            DLOG(("receive_zrinit_wait(): ERROR garbled header\n"));
            stats_increment_errors(_("GARBLED HEADER"));
            packet_buffer_n = 0;
            build_packet(P_ZNAK, options, output, output_n, output_max);
            return Q_TRUE;
        }

        if (rc_pp == ZM_PP_NODATA) {
            return Q_TRUE;
        }

        if (rc_pp == ZM_PP_OK) {

            if (packet.type == P_ZFIN) {
                /*
                 * Last file has come down, we're done
                 */
                set_transfer_stats_last_message("ZFIN");

                /*
                 * All done.
                 */
                options = 0;
                build_packet(P_ZFIN, options, output, output_n, output_max);

                /*
                 * Waiting for the Over-and-Out
                 */
                status.state = ZFIN_WAIT;

            } else if (packet.type == P_ZRQINIT) {

                /*
                 * Sender has repeated its ZRQINIT, re-send the ZRINIT
                 * response.
                 */
                set_transfer_stats_last_message("ZRINIT");
                status.state = ZRINIT;
                packet.crc16 = 0;
                packet.crc32 = compute_crc32(0, NULL, 0);

            } else if (packet.type == P_ZSINIT) {
                set_transfer_stats_last_message("ZSINIT");

                /*
                 * See what options were specified
                 */
                if (packet.argument & TX_ESCAPE_CTRL) {
                    status.flags |= TX_ESCAPE_CTRL;
                    DLOG(("receive_zrinit_wait() ZSINIT TX_ESCAPE_CTRL\n"));
                }
                if (packet.argument & TX_ESCAPE_8BIT) {
                    status.flags |= TX_ESCAPE_8BIT;
                    DLOG(("receive_zrinit_wait() ZSINIT TX_ESCAPE_8BIT\n"));
                }
                DLOG(("receive_zrinit_wait() ZSINIT state = %d\n",
                        status.state));

                /*
                 * Update the encode map
                 */
                setup_encode_byte_map();

                /*
                 * ZACK the ZSINIT
                 */
                options = 0;
                build_packet(P_ZACK, options, output, output_n, output_max);

            } else if (packet.type == P_ZCOMMAND) {
                /*
                 * Be compliant with the co-called standard, but emit a
                 * warning because NO ONE should ever use ZCOMMAND.
                 */
                set_transfer_stats_last_message(
                    _("ERROR: ZCOMMAND NOT SUPPORTED"));

            } else if (packet.type == P_ZFILE) {
                set_transfer_stats_last_message("ZFILE");

                /*
                 * Record the prior state and switch to data processing
                 */
                status.prior_state = ZRINIT_WAIT;
                status.state = ZDATA;
                packet.data_n = 0;
                packet.crc16 = 0;
                packet.crc32 = compute_crc32(0, NULL, 0);

            } else if (packet.type == P_ZNAK) {
                DLOG(("receive_zrinit_wait(): ERROR ZNAK\n"));
                stats_increment_errors("ZNAK");

                if (status.prior_state == ZSKIP) {
                    /*
                     * Special case: send a ZSKIP back instead of a ZRINIT if
                     * we asked for a ZSKIP the last time.
                     */
                    status.state = ZSKIP;
                } else {
                    status.state = ZRINIT;
                }

            } else {
                /*
                 * Sender isn't Zmodem compliant, abort.
                 */
                status.state = ABORT;
                stop_file_transfer(Q_TRANSFER_STATE_ABORT);
                return Q_TRUE;
            }
        }

        /*
         * Process through the new state
         */
        return Q_FALSE;

    }

    if (check_timeout() == Q_TRUE) {
        if (status.prior_state == ZSKIP) {
            /*
             * Special case: send a ZSKIP back instead of a ZRINIT if we
             * asked for a ZSKIP the last time.
             */
            status.state = ZSKIP;
        } else {
            status.state = ZRINIT;
        }
        return Q_FALSE;
    }

    /*
     * No data, done
     */
    return Q_TRUE;
}


/**
 * Receive:  ZRPOS
 *
 * @param output a buffer to write bytes to send to the remote side
 * @param output_n the number of bytes that this function wrote to output
 * @param output_max the maximum size of the output buffer
 * @return true if we are done reading input and are ready to send bytes to
 * the remote side
 */
static Q_BOOL receive_zrpos(unsigned char * output,
                            unsigned int * output_n,
                            const unsigned int output_max) {

    uint32_t options;

    DLOG(("receive_zrpos()\n"));

    options = status.file_position;
    build_packet(P_ZRPOS, options, output, output_n, output_max);
    status.state = ZRPOS_WAIT;
    DLOG(("receive_zrpos(): ZRPOS file_position = %ld\n",
            status.file_position));

    packet_buffer_n = 0;
    return Q_FALSE;
}

/**
 * Receive:  ZRPOS_WAIT
 *
 * @param output a buffer to write bytes to send to the remote side
 * @param output_n the number of bytes that this function wrote to output
 * @param output_max the maximum size of the output buffer
 * @return true if we are done reading input and are ready to send bytes to
 * the remote side
 */
static Q_BOOL receive_zrpos_wait(unsigned char * output,
                                 unsigned int * output_n,
                                 const unsigned int output_max) {

    ZM_PARSE_PACKET rc_pp;
    int discard;
    uint32_t options = 0;
    struct utimbuf utime_buffer;

    DLOG(("receive_zrpos_wait()\n"));

    if (packet_buffer_n > 0) {
        rc_pp = parse_packet(packet_buffer, packet_buffer_n, &discard);

        /*
         * Take the bytes off the stream
         */
        if (discard > 0) {
            memmove(packet_buffer, packet_buffer + discard,
                    packet_buffer_n - discard);
            packet_buffer_n -= discard;
        }

        if ((rc_pp == ZM_PP_CRCERROR) || (rc_pp == ZM_PP_INVALID)) {

            if (status.prior_state != ZRPOS_WAIT) {
                /*
                 * Only send ZNAK when we aren't in ZDATA mode
                 */
                DLOG(("receive_zrpos_wait(): ERROR garbled header\n"));
                stats_increment_errors(_("GARBLED HEADER"));
                packet_buffer_n = 0;
                build_packet(P_ZNAK, options, output, output_n, output_max);
                return Q_TRUE;
            } else {
                /*
                 * Keep processing the buffer until we get ZM_PP_NODATA
                 */
                return Q_FALSE;
            }

        }

        if (rc_pp == ZM_PP_NODATA) {
            return Q_TRUE;
        }

        if (rc_pp == ZM_PP_OK) {

            if (packet.type == P_ZEOF) {

                DLOG(("receive_zrpos_wait() ZEOF bytes = %ud file size = %ld\n",
                        packet.argument, status.file_position));

                q_transfer_stats.state = Q_TRANSFER_STATE_FILE_DONE;
                set_transfer_stats_last_message("ZEOF");

                /*
                 * Check file length and ack
                 */
                if (status.file_position == packet.argument) {
                    /*
                     * All ok
                     */
                    fclose(status.file_stream);

                    /*
                     * Set access and modification times
                     */
                    utime_buffer.actime = status.file_modtime;
                    utime_buffer.modtime = status.file_modtime;
                    utime(status.file_fullname, &utime_buffer);

                    assert(status.file_name != NULL);

                    Xfree(status.file_name, __FILE__, __LINE__);
                    status.file_name = NULL;
                    status.file_stream = NULL;

                    options = 0;
                    build_packet(P_ZRINIT, options, output, output_n,
                                 output_max);

                    set_transfer_stats_last_message("ZRINIT");

                    /*
                     * ZEOF will be followed by ZFIN or ZFILE, let
                     * receive_zrinit_wait() figure it out.
                     */
                    status.state = ZRINIT_WAIT;

                } else {
                    /*
                     * The sender claims that the file size is different from
                     * the actual size they sent.  We will send an error and
                     * try to recover.
                     */
                    DLOG(("receive_zrpos_wait(): ERROR BAD FILE POSITION FROM SENDER\n"));
                    stats_increment_errors(_("BAD FILE POSITION FROM SENDER"));
                    status.state = ZRPOS;
                }

            } else if (packet.type == P_ZDATA) {

                set_transfer_stats_last_message("ZDATA");

                /*
                 * Record the prior state and switch to data processing
                 */
                status.prior_state = ZRPOS_WAIT;
                status.state = ZDATA;
                packet.data_n = 0;
                packet.crc16 = 0;
                packet.crc32 = compute_crc32(0, NULL, 0);

            } else if (packet.type == P_ZNAK) {
                DLOG(("receive_zrpos_wait(): ERROR ZNAK\n"));
                stats_increment_errors("ZNAK");
                status.state = ZRPOS;

            } else {
                /*
                 * Sender isn't Zmodem compliant, abort.
                 */
                status.state = ABORT;
                stop_file_transfer(Q_TRANSFER_STATE_ABORT);
                return Q_TRUE;
            }

        }

        /*
         * Process through the new state
         */
        return Q_FALSE;

    }

    if (check_timeout() == Q_TRUE) {
        status.state = ZRPOS;
        return Q_FALSE;
    }

    /*
     * No data, done
     */
    return Q_TRUE;
}

/**
 * Receive:  ZFILE
 *
 * @return true if we are done reading input and are ready to send bytes to
 * the remote side
 */
static Q_BOOL receive_zfile() {

    int filesleft;
    long totalbytesleft;
    mode_t permissions;
    struct stat fstats;
    Q_BOOL need_new_file = Q_FALSE;
    int i;
    int rc;
    Q_BOOL file_exists = Q_FALSE;

    DLOG(("receive_zfile()\n"));

    /*
     * Break out ZFILE data
     */

    /*
     * filename
     */
    status.file_name = Xstrdup((char *) packet.data, __FILE__, __LINE__);

    /*
     * size, mtime, umask, files left, total left
     */
    sscanf((char *) packet.data + strlen((char *) packet.data) + 1,
           "%u %lo %o 0 %d %ld", (unsigned int *) &status.file_size,
           &status.file_modtime, (int *) &permissions, &filesleft,
           &totalbytesleft);

    /*
     * It so happens we can't use the permissions mask.  Forsberg didn't
     * encode it in a standard way.
     */

    DLOG(("receive_zfile(): ZFILE name: %s\n", status.file_name));
    DLOG(("receive_zfile(): ZFILE size: %u mtime: %s umask: %04o\n",
            status.file_size, ctime(&status.file_modtime), permissions));
    DLOG(("receive_zfile(): ZFILE filesleft: %d totalbytesleft: %ld\n",
            filesleft, totalbytesleft));

    /*
     * Open the file
     */
    sprintf(status.file_fullname, "%s/%s", download_path, status.file_name);
    rc = stat(status.file_fullname, &fstats);
    if (rc < 0) {
        if (errno == ENOENT) {
            /*
             * Creating the file, so go straight to ZRPOS
             */
            status.file_position = 0;
            DLOG(("receive_zfile(): new file, switch to ZRPOS\n"));
            set_transfer_stats_last_message("ZRPOS");
            status.state = ZRPOS;
        } else {
            status.state = ABORT;
            set_transfer_stats_last_message(_("DISK I/O ERROR"));
            stop_file_transfer(Q_TRANSFER_STATE_ABORT);
            return Q_TRUE;
        }
    } else {
        file_exists = Q_TRUE;
        status.file_position = fstats.st_size;

        /*
         * Check if we need to ZSKIP or ZCRC this file
         */
        if (status.file_size < status.file_position) {
            /*
             * Uh-oh, this is obviously a new file because it is smaller than
             * the file on disk.
             */
            need_new_file = Q_TRUE;

        } else if (status.file_size == status.file_position) {
            /*
             * Hmm, we have a file on disk already.  We'll open the file, but
             * switch to ZCRC and see if we should ZSKIP the file based on
             * its CRC value.
             */
            DLOG(("receive_zfile(): existing file, switch to ZCRC\n"));
            set_transfer_stats_last_message("ZCRC");
            status.state = ZCRC;

        } else if (status.file_size > 0) {
            /*
             * Looks like a crash recovery case
             */
            DLOG(("receive_zfile(): filename exists, might need crash recovery, switch to ZCRC\n"));
            set_transfer_stats_last_message("ZCRC");
            status.state = ZCRC;
        } else {
            /*
             * 0-length file, so we can switch directly to ZRPOS.
             */
            DLOG(("receive_zfile(): 0-length file, switch to ZRPOS\n"));
            set_transfer_stats_last_message("ZRPOS");
            status.state = ZRPOS;

        }

    }

    if (need_new_file == Q_TRUE) {
        /*
         * Guarantee we get a new file
         */
        file_exists = Q_FALSE;

        for (i = 0; ; i++) {
            /*
             * Change the filename
             */
            sprintf(status.file_fullname, "%s/%s.%04d", download_path,
                    status.file_name, i);

            rc = stat(status.file_fullname, &fstats);
            if (rc < 0) {
                if (errno == ENOENT) {
                    /*
                     * Creating the file, so go straight to ZRPOS
                     */
                    status.file_position = 0;

                    DLOG(("receive_zfile(): prevent overwrite, switch to ZRPOS, new filename = %s\n",
                            status.file_fullname));
                    status.state = ZRPOS;
                    break;

                } else {

                    status.state = ABORT;
                    set_transfer_stats_last_message(_("DISK I/O ERROR"));
                    stop_file_transfer(Q_TRANSFER_STATE_ABORT);
                    return Q_TRUE;
                }
            }

        } /* for (i = 0 ; ; i++) */

    } /* if (need_new_file == Q_TRUE) */

    if (file_exists == Q_TRUE) {
        status.file_stream = fopen(status.file_fullname, "r+b");
    } else {
        status.file_stream = fopen(status.file_fullname, "w+b");
    }

    if (status.file_stream == NULL) {
        status.state = ABORT;
        set_transfer_stats_last_message(_("CANNOT CREATE FILE"));
        stop_file_transfer(Q_TRANSFER_STATE_ABORT);
        return Q_TRUE;
    }

    /*
     * Seek to the end
     */
    fseek(status.file_stream, 0, SEEK_END);

    /*
     * Update progress display
     */
    stats_new_file(status.file_fullname, status.file_size);
    q_transfer_stats.bytes_transfer = status.file_position;

    /*
     * Process through the new state
     */
    return Q_FALSE;
}

/**
 * Receive:  ZDATA
 *
 * @param output a buffer to write bytes to send to the remote side
 * @param output_n the number of bytes that this function wrote to output
 * @param output_max the maximum size of the output buffer
 * @return true if we are done reading input and are ready to send bytes to
 * the remote side
 */
static Q_BOOL receive_zdata(unsigned char * output,
                            unsigned int * output_n,
                            const unsigned int output_max) {

    Q_BOOL end_of_packet;
    Q_BOOL acknowledge;
    Q_BOOL crc_ok = Q_FALSE;
    uint32_t options;
    int crc16;
    uint32_t crc32;
    unsigned int i;

    DLOG(("receive_zdata(): DATA state=%d prior_state=%d packet.data_n=%d\n",
            status.state, status.prior_state, packet.data_n));

    /*
     * First, decode the bytes for escaped control characters
     */
    if (decode_zdata_bytes
        (packet_buffer, &packet_buffer_n, packet.data + packet.data_n,
         &packet.data_n, sizeof(packet.data) - packet.data_n,
         packet.crc_buffer) == Q_FALSE) {

        /*
         * Not enough data available, wait for more
         */

        /*
         * Trash the partial data in packet.data.
         */
        if (packet_buffer_n > 0) {
            packet.data_n = 0;
        }
        return Q_TRUE;
    }

    /*
     * See what kind of CRC escape was requested
     */
    if (packet.crc_buffer[0] == ZCRCG) {
        /*
         * CRC escape:  not end of packet, no acknowledgement required
         */
        end_of_packet = Q_FALSE;
        acknowledge = Q_FALSE;

    } else if (packet.crc_buffer[0] == ZCRCE) {
        /*
         * CRC escape:  end of packet, no acknowledgement required
         */
        end_of_packet = Q_TRUE;
        acknowledge = Q_FALSE;

    } else if (packet.crc_buffer[0] == ZCRCW) {
        /*
         * CRC escape:  end of packet, acknowledgement required
         */
        end_of_packet = Q_TRUE;
        acknowledge = Q_TRUE;

    } else if (packet.crc_buffer[0] == ZCRCQ) {
        /*
         * CRC escape:  not end of packet, acknowledgement required
         */
        end_of_packet = Q_FALSE;
        acknowledge = Q_TRUE;
    } else {
        /*
         * Sender isn't Zmodem compliant, abort.
         */
        status.state = ABORT;
        stop_file_transfer(Q_TRANSFER_STATE_ABORT);
        return Q_TRUE;
    }

    /*
     * Check the crc
     */

    /*
     * Copy the CRC escape byte onto the end of packet.data
     */
    packet.data[packet.data_n] = packet.crc_buffer[0];
    if (packet.use_crc32 == Q_TRUE) {

        /*
         * 32-bit CRC
         */
        packet.crc32 = compute_crc32(0, NULL, 0);
        packet.crc32 =
            compute_crc32(packet.crc32, packet.data, packet.data_n + 1);

        /*
         * Little-endian
         */
        crc32 = ((packet.crc_buffer[4] & 0xFF) << 24) |
                ((packet.crc_buffer[3] & 0xFF) << 16) |
                ((packet.crc_buffer[2] & 0xFF) << 8) |
                 (packet.crc_buffer[1] & 0xFF);

        DLOG(("receive_zdata(): DATA CRC32: given    %08x\n", crc32));
        DLOG(("receive_zdata(): DATA CRC32: computed %08x\n", packet.crc32));

        if (crc32 == packet.crc32) {
            /*
             * CRC OK
             */
            crc_ok = Q_TRUE;
        }

    } else {
        /*
         * 16-bit CRC
         */
        packet.crc16 =
            compute_crc16(packet.crc16, packet.data, packet.data_n + 1);
        crc16 = ((packet.crc_buffer[1] & 0xFF) << 8) |
                 (packet.crc_buffer[2] & 0xFF);

        DLOG(("receive_zdata(): DATA CRC16: given    %04x\n", crc16));
        DLOG(("receive_zdata(): DATA CRC16: computed %04x\n", packet.crc16));

        if (crc16 == packet.crc16) {
            /*
             * CRC OK
             */
            crc_ok = Q_TRUE;
        }

    }

    if (crc_ok == Q_TRUE) {

        DLOG(("receive_zdata(): DATA CRC: OK\n"));

        if (status.prior_state == ZRPOS_WAIT) {

            DLOG(("receive_zdata(): WRITE TO FILE %d BYTES\n", packet.data_n));

            /*
             * Write the packet to file
             */
            fwrite(packet.data, 1, packet.data_n, status.file_stream);
            fflush(status.file_stream);

            /*
             * Increment count
             */
            status.file_position += packet.data_n;
            status.block_size = packet.data_n;

            q_transfer_stats.bytes_transfer += packet.data_n;
            stats_increment_blocks();

            packet.data_n = 0;
            packet.crc16 = 0;
            packet.crc32 = compute_crc32(0, NULL, 0);

            if (acknowledge == Q_TRUE) {
                DLOG(("receive_zdata(): ZACK required\n"));
                options = big_to_little_endian(status.file_position);
                build_packet(P_ZACK, options, output, output_n, output_max);
            }

            if (end_of_packet == Q_TRUE) {
                DLOG(("receive_zdata(): PACKET EOF\n"));
                status.state = ZRPOS_WAIT;
                return Q_FALSE;
            }
        }

    } else {
        /*
         * CRC error
         */
        DLOG(("receive_zdata(): CRC ERROR\n"));

        if (status.prior_state == ZRPOS_WAIT) {

            /*
             * CRC error in the packet
             */
            stats_increment_errors(_("CRC ERROR"));

            /*
             * Send ZRPOS
             */
            packet_buffer_n = 0;
            options = status.file_position;
            build_packet(P_ZRPOS, options, output, output_n, output_max);
            status.state = ZRPOS_WAIT;

            DLOG(("receive_zdata(): send ZRPOS file_position = %ld\n",
                    status.file_position));
            return Q_TRUE;

        } else if (status.prior_state == ZRINIT_WAIT) {

            /*
             * CRC error in the packet
             */
            stats_increment_errors(_("CRC ERROR"));

            /*
             * Send ZNAK
             */
            packet_buffer_n = 0;
            options = 0;
            build_packet(P_ZNAK, options, output, output_n, output_max);
            status.state = ZRINIT_WAIT;
            DLOG(("receive_zdata(): send ZNAK, back to zrinit to get filename\n"));
            return Q_TRUE;
        } else {
            /*
             * Some other state.  This is a coding error.  Save a log message,
             */
            DLOG(("receive_zdata(): CRC error, but in incorrect protocol state!\n"));
        }
    }

    DLOG(("receive_zdata(): DATA (post-processed): "));
    for (i = 0; i < packet.data_n; i++) {
        DLOG2(("%02x ", (packet.data[i] & 0xFF)));
    }
    DLOG2(("\n"));

    /*
     * Figure out the next state to transition to
     */
    if (status.prior_state == ZRINIT_WAIT) {
        switch (packet.type) {
        case P_ZFILE:
            status.state = ZFILE;
            break;

        case P_ZSINIT:
            status.state = ZRINIT_WAIT;

            /*
             * Send ZACK
             */
            options = 0;
            build_packet(P_ZACK, options, output, output_n, output_max);
            return Q_TRUE;

        case P_ZCOMMAND:
            status.state = ZRINIT_WAIT;

            /*
             * Send ZCOMPL, assume it failed
             */
            options = 1;
            build_packet(P_ZCOMPL, options, output, output_n, output_max);
            return Q_TRUE;

        default:
            status.state = ZDATA;
            break;
        }

    } else {
        /*
         * We came here from ZRPOS_WAIT
         */
        status.state = ZDATA;
    }

    /*
     * Process through the new state
     */
    return Q_FALSE;
}

/**
 * Receive:  ZSKIP
 *
 * @param output a buffer to write bytes to send to the remote side
 * @param output_n the number of bytes that this function wrote to output
 * @param output_max the maximum size of the output buffer
 * @return true if we are done reading input and are ready to send bytes to
 * the remote side
 */
static Q_BOOL receive_zskip(unsigned char * output,
                            unsigned int * output_n,
                            const unsigned int output_max) {

    uint32_t options = 0;
    struct utimbuf utime_buffer;

    DLOG(("receive_zskip()\n"));

    /*
     * Close existing file handle, reset file fields...
     */
    fclose(status.file_stream);

    /*
     * Set access and modification times
     */
    utime_buffer.actime = status.file_modtime;
    utime_buffer.modtime = status.file_modtime;
    utime(status.file_fullname, &utime_buffer);

    assert(status.file_name != NULL);
    Xfree(status.file_name, __FILE__, __LINE__);
    status.file_name = NULL;
    status.file_stream = NULL;

    /*
     * Send out ZSKIP packet
     */
    build_packet(P_ZSKIP, options, output, output_n, output_max);

    /*
     * Update progress display
     */
    q_transfer_stats.state = Q_TRANSFER_STATE_FILE_DONE;
    set_transfer_stats_last_message("ZSKIP");

    /*
     * ZSKIP will be followed immediately by another ZFILE, which is handled
     * in receive_zrinit_wait().
     */
    status.prior_state = ZSKIP;
    status.state = ZRINIT_WAIT;
    packet_buffer_n = 0;
    return Q_FALSE;
}

/**
 * Receive a file via the Zmodem protocol.
 *
 * @param input the bytes from the remote side
 * @param input_n the number of bytes in input_n
 * @param output a buffer to contain the bytes to send to the remote side
 * @param output_n the number of bytes that this function wrote to output
 * @param output_max the maximum number of bytes this function may write to
 * output
 */
static void zmodem_receive(unsigned char * input, unsigned int input_n,
                           unsigned char * output, unsigned int * output_n,
                           const unsigned int output_max) {

    Q_BOOL done;

    /*
     * DLOG(("zmodem_receive() NOISE ON\n"));
     * static int noise = 0;
     * noise++;
     * if (noise > 30) {
     *     noise = 0;
     *     input[0] = 0xaa;
     * }
     */

    unsigned int i;
    DLOG(("zmodem_receive() START packet_buffer_n = %d packet_buffer = ",
            packet_buffer_n));
    for (i = 0; i < packet_buffer_n; i++) {
        DLOG2(("%02x ", (packet_buffer[i] & 0xFF)));
    }
    DLOG2(("\n"));

    done = Q_FALSE;
    while (done == Q_FALSE) {

        /*
         * Add input_n to packet_buffer
         */
        if (input_n > sizeof(packet_buffer) - packet_buffer_n) {
            memcpy(packet_buffer + packet_buffer_n, input,
                   sizeof(packet_buffer) - packet_buffer_n);
            memmove(input, input + sizeof(packet_buffer) - packet_buffer_n,
                    input_n - (sizeof(packet_buffer) - packet_buffer_n));
            input_n -= (sizeof(packet_buffer) - packet_buffer_n);
            packet_buffer_n = sizeof(packet_buffer);
        } else {
            memcpy(packet_buffer + packet_buffer_n, input, input_n);
            packet_buffer_n += input_n;
            input_n = 0;
        }

        switch (status.state) {

        case INIT:
            /*
             * This state is where everyone begins.  Start with ZCHALLENGE or
             * ZRINIT.
             */
            if (zmodem_zchallenge == Q_TRUE) {
                set_transfer_stats_last_message("ZCHALLENGE");
                status.state = ZCHALLENGE;
            } else {
                set_transfer_stats_last_message("ZRINIT");
                status.state = ZRINIT;
                packet.crc16 = 0;
                packet.crc32 = compute_crc32(0, NULL, 0);
            }
            break;

        case ZCHALLENGE:
            done = receive_zchallenge(output, output_n, output_max);
            break;

        case ZCHALLENGE_WAIT:
            done = receive_zchallenge_wait(output, output_n, output_max);
            break;

        case ZCRC:
            done = receive_zcrc(output, output_n, output_max);
            break;

        case ZCRC_WAIT:
            done = receive_zcrc_wait(output, output_n, output_max);
            break;

        case ZRINIT:
            done = receive_zrinit(output, output_n, output_max);
            break;

        case ZRINIT_WAIT:
            done = receive_zrinit_wait(output, output_n, output_max);
            break;

        case ZRPOS:
            done = receive_zrpos(output, output_n, output_max);
            break;

        case ZRPOS_WAIT:
            done = receive_zrpos_wait(output, output_n, output_max);
            break;

        case ZFILE:
            done = receive_zfile();
            break;

        case ZSKIP:
            done = receive_zskip(output, output_n, output_max);
            break;

        case ZDATA:
            done = receive_zdata(output, output_n, output_max);
            break;

        case ZFIN_WAIT:

            DLOG(("zmodem_receive(): ZFIN\n"));

            /*
             * NOP
             */
            done = Q_TRUE;

            status.state = COMPLETE;

            /*
             * Update transfer stats
             */
            set_transfer_stats_last_message(_("SUCCESS"));
            stop_file_transfer(Q_TRANSFER_STATE_END);
            time(&q_transfer_stats.end_time);

            break;

        case ABORT:
        case COMPLETE:
            /*
             * NOP
             */
            done = Q_TRUE;
            break;

        case ZFILE_WAIT:
        case ZSINIT:
        case ZSINIT_WAIT:
        case ZRQINIT:
        case ZRQINIT_WAIT:
        case ZFIN:
        case ZEOF:
        case ZEOF_WAIT:
            /*
             * Receive should NEVER see these states
             */
            abort();
            break;

        } /* switch (status.state) */

        DLOG(("zmodem_receive(): done = %s\n",
                (done == Q_TRUE ? "true" : "false")));

    } /* while (done == Q_FALSE) */

    DLOG(("zmodem_receive() END packet_buffer_n = %d packet_buffer = ",
            packet_buffer_n));
    for (i = 0; i < packet_buffer_n; i++) {
        DLOG2(("%02x ", (packet_buffer[i] & 0xFF)));
    }
    DLOG2(("\n"));

}

/**
 * Send:  ZRQINIT
 *
 * @param output a buffer to contain the bytes to send to the remote side
 * @param output_n the number of bytes that this function wrote to output
 * @param output_max the maximum number of bytes this function may write to
 * output
 */
static Q_BOOL send_zrqinit(unsigned char * output,
                           unsigned int * output_n,
                           const unsigned int output_max) {

    uint32_t options;

    DLOG(("send_zrqinit()\n"));

    options = 0;
    build_packet(P_ZRQINIT, options, output, output_n, output_max);
    status.state = ZRQINIT_WAIT;
    packet_buffer_n = 0;
    return Q_FALSE;
}

/**
 * Send:  ZRQINIT_WAIT
 *
 * @param output a buffer to contain the bytes to send to the remote side
 * @param output_n the number of bytes that this function wrote to output
 * @param output_max the maximum number of bytes this function may write to
 * output
 */
static Q_BOOL send_zrqinit_wait(unsigned char * output,
                                unsigned int * output_n,
                                const unsigned int output_max) {

    ZM_PARSE_PACKET rc_pp;

    int discard;
    uint32_t options = 0;

    DLOG(("send_zrqinit_wait()\n"));

    if (packet_buffer_n > 0) {
        rc_pp = parse_packet(packet_buffer, packet_buffer_n, &discard);

        /*
         * Take the bytes off the stream
         */
        if (discard > 0) {
            DLOG(("send_zrqinit_wait(): packet_buffer_n = %d discard = %d\n",
                    packet_buffer_n, discard));
            memmove(packet_buffer, packet_buffer + discard,
                    packet_buffer_n - discard);
            packet_buffer_n -= discard;
        }

        if ((rc_pp == ZM_PP_CRCERROR) || (rc_pp == ZM_PP_INVALID)) {
            DLOG(("send_zrqinit_wait(): ERROR garbled header\n"));
            stats_increment_errors(_("GARBLED HEADER"));
            packet_buffer_n = 0;
            build_packet(P_ZNAK, options, output, output_n, output_max);
            return Q_TRUE;
        }

        if (rc_pp == ZM_PP_NODATA) {
            return Q_TRUE;
        }

        if (rc_pp == ZM_PP_OK) {

            if (packet.type == P_ZRINIT) {
                set_transfer_stats_last_message("ZRINIT");

                /*
                 * See what options were specified
                 */
                if (packet.argument & TX_ESCAPE_CTRL) {
                    status.flags |= TX_ESCAPE_CTRL;
                    DLOG(("send_zrqinit_wait() ZRINIT TX_ESCAPE_CTRL\n"));
                }
                if (packet.argument & TX_ESCAPE_8BIT) {
                    status.flags |= TX_ESCAPE_8BIT;
                    DLOG(("send_zrqinit_wait() ZRINIT TX_ESCAPE_8BIT\n"));
                }
                if (packet.argument & TX_CAN_FULL_DUPLEX) {
                    status.flags |= TX_CAN_FULL_DUPLEX;
                    DLOG(("send_zrqinit_wait() ZRINIT TX_CAN_FULL_DUPLEX\n"));
                }
                if (packet.argument & TX_CAN_OVERLAP_IO) {
                    status.flags |= TX_CAN_OVERLAP_IO;
                    DLOG(("send_zrqinit_wait() ZRINIT TX_CAN_OVERLAP_IO\n"));
                }
                if (packet.argument & TX_CAN_BREAK) {
                    status.flags |= TX_CAN_BREAK;
                    DLOG(("send_zrqinit_wait() ZRINIT TX_CAN_BREAK\n"));
                }
                if (packet.argument & TX_CAN_DECRYPT) {
                    status.flags |= TX_CAN_DECRYPT;
                    DLOG(("send_zrqinit_wait() ZRINIT TX_CAN_DECRYPT\n"));
                }
                if (packet.argument & TX_CAN_LZW) {
                    status.flags |= TX_CAN_LZW;
                    DLOG(("send_zrqinit_wait() ZRINIT TX_CAN_LZW\n"));
                }
                /*
                 * zmodem_start(..., Z_CRC16) fails to send a file by this.
                 * If this is not commented out, use Z_CRC32 all the time.
                 */
#if 0
                if (packet.argument & TX_CAN_CRC32) {
                    status.flags |= TX_CAN_CRC32;
                    DLOG(("send_zrqinit_wait() ZRINIT TX_CAN_CRC32\n"));
                    status.use_crc32 = Q_TRUE;
                }
#endif

                /*
                 * Update the encode map
                 */
                setup_encode_byte_map();

                /*
                 * Now switch to ZSINIT
                 */
                status.state = ZSINIT;

            } else if (packet.type == P_ZCHALLENGE) {
                /*
                 * Respond to ZCHALLENGE, remain in ZRINIT_WAIT
                 */
                options = packet.argument;
                build_packet(P_ZACK, options, output, output_n, output_max);

            } else if (packet.type == P_ZNAK) {
                DLOG(("send_zrqinit_wait(): ERROR ZNAK\n"));
                stats_increment_errors("ZNAK");
                status.state = ZRQINIT;

            } else {
                /*
                 * Sender isn't Zmodem compliant, abort.
                 */
                status.state = ABORT;
                stop_file_transfer(Q_TRANSFER_STATE_ABORT);
                return Q_TRUE;
            }
        }

        /*
         * Process through the new state
         */
        return Q_FALSE;

    }

    if (check_timeout() == Q_TRUE) {
        status.state = ZRQINIT;
        return Q_FALSE;
    }

    /*
     * No data, done
     */
    return Q_TRUE;

}

/**
 * Send:  ZSINIT
 *
 * @param output a buffer to contain the bytes to send to the remote side
 * @param output_n the number of bytes that this function wrote to output
 * @param output_max the maximum number of bytes this function may write to
 * output
 */
static Q_BOOL send_zsinit(unsigned char * output,
                          unsigned int * output_n,
                          const unsigned int output_max) {

    uint32_t options;

    DLOG(("send_zsinit()\n"));

    /*
     * Escape ctrl characters by default, but not 8bit characters
     */
    if (((status.flags & TX_ESCAPE_CTRL) == 0) &&
        (zmodem_escape_ctrl == Q_TRUE)) {

        options = TX_ESCAPE_CTRL;
        build_packet(P_ZSINIT, options, output, output_n, output_max);
        status.state = ZSINIT_WAIT;
        set_transfer_stats_last_message("ZSINIT");
        /*
         * This is where I could put an attention string
         */
        /*
         * snprintf(packet.data, sizeof(packet.data) - 1, "SOMETHING HERE");
         */
        packet.data[0] = 0x0;
        packet.data_n = strlen((char *) packet.data) + 1;

        /*
         * Make sure we continue to use the right CRC
         */
        packet.use_crc32 = Q_FALSE;

        /*
         * Now encode
         */
        encode_zdata_bytes(output, output_n, output_max, ZCRCW);

    } else {
        /*
         * Head straight into file upload
         */
        set_transfer_stats_last_message("ZFILE");
        status.state = ZFILE;
    }

    packet_buffer_n = 0;
    return Q_FALSE;
}

/**
 * Send:  ZSINIT_WAIT
 *
 * @param output a buffer to contain the bytes to send to the remote side
 * @param output_n the number of bytes that this function wrote to output
 * @param output_max the maximum number of bytes this function may write to
 * output
 */
static Q_BOOL send_zsinit_wait(unsigned char * output,
                               unsigned int * output_n,
                               const unsigned int output_max) {

    ZM_PARSE_PACKET rc_pp;

    int discard;
    uint32_t options = 0;

    DLOG(("send_zsinit_wait()\n"));

    if (packet_buffer_n > 0) {
        rc_pp = parse_packet(packet_buffer, packet_buffer_n, &discard);

        /*
         * Take the bytes off the stream
         */
        if (discard > 0) {
            memmove(packet_buffer, packet_buffer + discard,
                    packet_buffer_n - discard);
            packet_buffer_n -= discard;
        }

        if ((rc_pp == ZM_PP_CRCERROR) || (rc_pp == ZM_PP_INVALID)) {
            DLOG(("send_zsinit_wait(): ERROR garbled header\n"));
            stats_increment_errors(_("GARBLED HEADER"));
            packet_buffer_n = 0;
            build_packet(P_ZNAK, options, output, output_n, output_max);
            return Q_TRUE;
        }

        if (rc_pp == ZM_PP_NODATA) {
            return Q_TRUE;
        }

        if (rc_pp == ZM_PP_OK) {

            if (packet.type == P_ZACK) {
                set_transfer_stats_last_message("ZACK");

                /*
                 * I'd love to wait a full second here...
                 */

                /*
                 * Switch to ZFILE
                 */
                set_transfer_stats_last_message("ZFILE");
                status.state = ZFILE;

            } else if (packet.type == P_ZNAK) {
                DLOG(("send_zsinit_wait(): ERROR ZNAK\n"));
                stats_increment_errors("ZNAK");
                status.state = ZSINIT;

            } else {
                /*
                 * Sender isn't Zmodem compliant, abort.
                 */
                status.state = ABORT;
                stop_file_transfer(Q_TRANSFER_STATE_ABORT);
                return Q_TRUE;
            }
        }

        /*
         * Process through the new state
         */
        return Q_FALSE;

    }

    if (check_timeout() == Q_TRUE) {
        status.state = ZSINIT;
        return Q_FALSE;
    }

    /*
     * No data, done
     */
    return Q_TRUE;

}

/**
 * Send:  ZFILE
 *
 * @param output a buffer to contain the bytes to send to the remote side
 * @param output_n the number of bytes that this function wrote to output
 * @param output_max the maximum number of bytes this function may write to
 * output
 */
static Q_BOOL send_zfile(unsigned char * output,
                         unsigned int * output_n,
                         const unsigned int output_max) {

    uint32_t options;

    DLOG(("send_zfile()\n"));

    /*
     * Send header for the next file
     */
    options = 0;
    build_packet(P_ZFILE, options, output, output_n, output_max);
    status.state = ZFILE_WAIT;

    /*
     * Put together the filename info
     */
    snprintf((char *) packet.data, sizeof(packet.data) - 1,
             "%s %d %lo 0 0 1 %d", status.file_name, status.file_size,
             status.file_modtime, status.file_size);

    /*
     * Include the NUL terminator
     */
    packet.data_n = strlen((char *) packet.data) + 1;
    packet.data[strlen(status.file_name)] = 0x00;

    /*
     * Make sure we continue to use the right CRC
     */
    packet.use_crc32 = status.use_crc32;

    /*
     * Now encode the filename
     */
    encode_zdata_bytes(output, output_n, output_max, ZCRCW);

    packet_buffer_n = 0;
    return Q_FALSE;
}

/**
 * Send:  ZFILE_WAIT
 *
 * @param output a buffer to contain the bytes to send to the remote side
 * @param output_n the number of bytes that this function wrote to output
 * @param output_max the maximum number of bytes this function may write to
 * output
 */
static Q_BOOL send_zfile_wait(unsigned char * output,
                              unsigned int * output_n,
                              const unsigned int output_max) {

    ZM_PARSE_PACKET rc_pp;

    int discard;
    uint32_t options = 0;

    DLOG(("send_zfile_wait()\n"));

    if (packet_buffer_n > 0) {
        rc_pp = parse_packet(packet_buffer, packet_buffer_n, &discard);

        /*
         * Take the bytes off the stream
         */
        if (discard > 0) {
            memmove(packet_buffer, packet_buffer + discard,
                    packet_buffer_n - discard);
            packet_buffer_n -= discard;
        }

        if ((rc_pp == ZM_PP_CRCERROR) || (rc_pp == ZM_PP_INVALID)) {
            DLOG(("send_zfile_wait(): ERROR garbled header\n"));
            stats_increment_errors(_("GARBLED HEADER"));
            packet_buffer_n = 0;
            build_packet(P_ZNAK, options, output, output_n, output_max);
            return Q_TRUE;
        }

        if (rc_pp == ZM_PP_NODATA) {
            return Q_TRUE;
        }

        if (rc_pp == ZM_PP_OK) {

            if (packet.type == P_ZRPOS) {
                set_transfer_stats_last_message("ZRPOS");
                DLOG(("send_zfile_wait() ZRPOS packet.argument = %u status.file_size = %u\n",
                        (uint32_t) packet.argument,
                        (uint32_t) status.file_size));

                /*
                 * Sanity check the file size
                 */
                if (packet.argument > status.file_size) {
                    /*
                     * The receiver lied to me, so screw them.
                     */
                    status.state = ABORT;
                    stop_file_transfer(Q_TRANSFER_STATE_ABORT);
                    return Q_TRUE;
                }

                /*
                 * Seek to the desired location
                 */
                status.file_position = packet.argument;
                fseek(status.file_stream, status.file_position, SEEK_SET);

                /*
                 * Send the ZDATA start
                 */
                options = big_to_little_endian(status.file_position);
                build_packet(P_ZDATA, options, output, output_n, output_max);
                status.prior_state = ZFILE_WAIT;
                status.state = ZDATA;
                status.ack_required = Q_FALSE;

            } else if (packet.type == P_ZNAK) {
                DLOG(("send_zfile_wait(): ERROR ZNAK\n"));
                stats_increment_errors("ZNAK");
                status.state = ZFILE;

            } else if (packet.type == P_ZCRC) {
                int total_bytes = 0;
                /*
                 * Buffer for reading the file
                 */
                unsigned char file_buffer[1];
                size_t file_buffer_n;

                /*
                 * Save the original file position
                 */
                off_t original_position = status.file_position;

                /*
                 * Receiver wants the file CRC between 0 and packet.argument
                 */
                set_transfer_stats_last_message("ZCRC");

                /*
                 * Reset crc32
                 */
                status.file_crc32 = compute_crc32(0, NULL, 0);

                /*
                 * Seek to beginning of file
                 */
                fseek(status.file_stream, 0, SEEK_SET);
                while ((!feof(status.file_stream)) &&
                    (total_bytes < packet.argument)
                ) {
                    file_buffer_n = fread(file_buffer, 1, sizeof(file_buffer),
                              status.file_stream);
                    total_bytes += file_buffer_n;
                    /*
                     * I think I have a different CRC function from lrzsz...
                     * I have to negate both here and below to get the same
                     * value.
                     */
                    status.file_crc32 =
                        ~compute_crc32(status.file_crc32, file_buffer,
                                       file_buffer_n);
                }
                status.file_crc32 = ~status.file_crc32;

                /*
                 * Seek back to the original location
                 */
                fseek(status.file_stream, original_position, SEEK_SET);

                DLOG(("send_zfile_wait() respond to ZCRC total_bytes = %d on-disk CRC32 = %08lx\n",
                        total_bytes, (unsigned long) status.file_crc32));

                /*
                 * Send it as a ZCRC
                 */
                options = status.file_crc32;
                build_packet(P_ZCRC, options, output, output_n, output_max);

            } else if (packet.type == P_ZSKIP) {
                /*
                 * Skip this file
                 */
                set_transfer_stats_last_message("ZSKIP");

                /*
                 * Increase the total batch transfer
                 */
                q_transfer_stats.batch_bytes_transfer += status.file_size;

                q_transfer_stats.state = Q_TRANSFER_STATE_FILE_DONE;
                set_transfer_stats_last_message("ZRINIT");

                fclose(status.file_stream);

                assert(status.file_name != NULL);
                Xfree(status.file_name, __FILE__, __LINE__);
                status.file_name = NULL;
                status.file_stream = NULL;

                /*
                 * Setup for the next file.
                 */
                upload_file_list_i++;
                setup_for_next_file();

            } else {
                /*
                 * Sender isn't Zmodem compliant, abort.
                 */
                status.state = ABORT;
                stop_file_transfer(Q_TRANSFER_STATE_ABORT);
                return Q_TRUE;
            }
        }

        /*
         * Process through the new state
         */
        return Q_FALSE;

    }

    if (check_timeout() == Q_TRUE) {
        status.state = ZFILE;
        return Q_FALSE;
    }

    /*
     * No data, done
     */
    return Q_TRUE;

}

/**
 * Send:  ZDATA
 *
 * @param output a buffer to contain the bytes to send to the remote side
 * @param output_n the number of bytes that this function wrote to output
 * @param output_max the maximum number of bytes this function may write to
 * output
 */
static Q_BOOL send_zdata(unsigned char * output,
                         unsigned int * output_n,
                         const unsigned int output_max) {

    ZM_PARSE_PACKET rc_pp;
    uint32_t options = 0;
    int discard;
    int rc;
    Q_BOOL last_block = Q_FALSE;
    Q_BOOL use_spare_packet = Q_FALSE;
    Q_BOOL got_error = Q_FALSE;

    DLOG(("send_zdata(): DATA state=%d prior_state=%d packet.data_n=%d\n",
            status.state, status.prior_state, packet.data_n));
    DLOG(("send_zdata(): waiting_for_ack = %s ack_required = %s reliable_link = %s confirmed_bytes = %u last_confirmed_bytes = %u block_size = %d blocks_ack_count = %d\n",
            (status.waiting_for_ack == Q_TRUE ? "true" : "false"),
            (status.ack_required == Q_TRUE ? "true" : "false"),
            (status.reliable_link == Q_TRUE ? "true" : "false"),
            status.confirmed_bytes, status.last_confirmed_bytes,
            status.block_size, status.blocks_ack_count));

    /*
     * Check the input buffer first
     */
    if (packet_buffer_n > 0) {
        rc_pp = parse_packet(packet_buffer, packet_buffer_n, &discard);

        /*
         * Take the bytes off the stream
         */
        if (discard > 0) {
            memmove(packet_buffer, packet_buffer + discard,
                    packet_buffer_n - discard);
            packet_buffer_n -= discard;
        }

        if ((rc_pp == ZM_PP_CRCERROR) || (rc_pp == ZM_PP_INVALID)) {
            DLOG(("send_zdata(): ERROR garbled header\n"));
            stats_increment_errors(_("GARBLED HEADER"));
            packet_buffer_n = 0;
            build_packet(P_ZNAK, options, output, output_n, output_max);
            return Q_TRUE;
        }

        if (rc_pp == ZM_PP_NODATA) {
            return Q_TRUE;
        }

        if (rc_pp == ZM_PP_OK) {

            if (packet.type == P_ZSKIP) {

                DLOG(("send_zdata() ZSKIP\n"));

                /*
                 * This is the proper way to skip a file - head to ZEOF
                 */

                /*
                 * Send an empty ZCRCW on recovery
                 */
                outbound_packet_n = 0;
                *output_n = 0;

                /*
                 * Make sure we continue to use the right CRC
                 */
                packet.use_crc32 = status.use_crc32;

                /*
                 * Encode directly to output
                 */
                encode_zdata_bytes(output, output_n, output_max, ZCRCW);

                /*
                 * Send ZEOF
                 */
                set_transfer_stats_last_message("ZEOF");
                status.state = ZEOF;

                /*
                 * Process through the new state
                 */
                return Q_FALSE;
            }

            if (packet.type == P_ZRPOS) {

                DLOG(("send_zdata() ZRPOS\n"));

                if (status.ack_required == Q_FALSE) {
                    /*
                     * This is the first ZRPOS that indicates an error.
                     */
                    DLOG(("send_zdata(): ERROR ZRPOS\n"));
                    stats_increment_errors(_("CRC ERROR"));

                    /*
                     * Send a ZCRCW.  That part is handled below.
                     */
                    status.ack_required = Q_TRUE;
                    status.waiting_for_ack = Q_FALSE;

                    /*
                     * Throw away everything that is still in the buffer so
                     * we can start with the empty ZCRCW packet.
                     */
                    *output_n = 0;
                    outbound_packet_n = 0;
                    status.streaming_zdata = Q_FALSE;

                    packet_buffer_n = 0;

                    got_error = Q_TRUE;

                } else {

                    /*
                     * lrz will send a second ZRPOS, but Hyperterm does not
                     * when the user hits 'Skip file'.  I'm not sure which is
                     * really correct, so handle both cases gracefully.
                     */
                    status.ack_required = Q_FALSE;
                    status.waiting_for_ack = Q_FALSE;

                    DLOG(("send_zdata(): 2nd ZRPOS in reponse to ZCRCW\n"));
                }

                DLOG(("send_zdata(): ZRPOS reposition to %u, file size is %u\n",
                        (uint32_t) packet.argument,
                        (uint32_t) status.file_size));

                if (packet.argument <= status.file_size) {
                    /*
                     * Record the confirmed bytes and use them to change
                     * block size as needed.
                     */
                    status.confirmed_bytes = packet.argument;
                    if (got_error == Q_TRUE) {
                        block_size_down();
                        if (status.state == ABORT) {
                            DLOG(("Transfer was cancelled, bye!\n"));
                            return Q_TRUE;
                        }
                    }

                    /*
                     * Seek to the desired location
                     */
                    status.file_position = packet.argument;
                    fseek(status.file_stream, status.file_position, SEEK_SET);

                    DLOG(("send_zdata() ZRPOS new file position: %lu\n",
                            status.file_position));

                    /*
                     * Update the progress display
                     */
                    q_transfer_stats.bytes_transfer = status.file_position;

                    /*
                     * Send the ZDATA start.
                     */
                    options = big_to_little_endian(status.file_position);
                    build_packet(P_ZDATA, options, output, output_n,
                                 output_max);
                } else if (packet.argument > status.file_size) {
                    /*
                     * The receiver lied to me, so screw them.
                     */
                    status.state = ABORT;
                    stop_file_transfer(Q_TRANSFER_STATE_ABORT);
                    return Q_TRUE;
                }

            } else if (packet.type == P_ZACK) {
                DLOG(("send_zdata() ZACK\n"));

                /*
                 * See how much they acked
                 */
                status.ack_required = Q_FALSE;
                status.waiting_for_ack = Q_FALSE;

                /*
                 * Seek to the desired location
                 */
                DLOG(("send_zdata() ZACK original file position: %lu\n",
                        status.file_position));

                /*
                 * Hyperterm lies to me when the user clicks 'Skip file'
                 */
                if (big_to_little_endian(packet.argument) > status.file_size) {
                    DLOG(("send_zdata() ZACK RECEIVER LIED ABOUT FILE POSITION\n"));
                    /*
                     * Treat this as ZEOF
                     */
                    set_transfer_stats_last_message("ZEOF");
                    status.state = ZEOF;
                    return Q_FALSE;
                }

                status.file_position = big_to_little_endian(packet.argument);

                /*
                 * Normal case: file position is somewhere within the file
                 */
                fseek(status.file_stream, status.file_position, SEEK_SET);

                DLOG(("send_zdata() ZACK new file position: %lu\n",
                        status.file_position));

                /*
                 * Record the confirmed bytes and use them to change block
                 * size as needed.
                 */
                status.confirmed_bytes = status.file_position;
                block_size_up();

                if (status.file_position == status.file_size) {
                    /*
                     * Yippee, done
                     */

                    /*
                     * Send ZEOF
                     */
                    set_transfer_stats_last_message("ZEOF");
                    status.state = ZEOF;
                    return Q_FALSE;

                } else {
                    /*
                     * Update the progress display
                     */
                    q_transfer_stats.bytes_transfer = status.file_position;

                    /*
                     * Check to see if we need to begin a new frame or just
                     * keep running with this one.
                     */
                    if (status.streaming_zdata == Q_FALSE) {
                        DLOG(("send_zdata() Send new ZDATA start at file position %lu\n",
                                status.file_position));

                        /*
                         * Send the ZDATA start
                         */
                        options = big_to_little_endian(status.file_position);
                        build_packet(P_ZDATA, options, output, output_n,
                                     output_max);
                        status.streaming_zdata = Q_TRUE;
                    }
                }

            } else if (packet.type == P_ZNAK) {
                DLOG(("send_zdata(): ERROR ZNAK\n"));
                stats_increment_errors("ZNAK");

                /*
                 * Technically ZNAK on ZDATA is a protocol error.  I am
                 * choosing to re-send starting from the last ack'd position
                 * hoping for a recovery, but it would be just as valid to
                 * cancel the transfer here.
                 */
                status.state = ZRPOS;

            } else {
                /*
                 * Sender isn't Zmodem compliant, abort.
                 */
                status.state = ABORT;
                stop_file_transfer(Q_TRANSFER_STATE_ABORT);
                return Q_TRUE;
            }
        }
    } else {
        /*
         * No input data, see if we are waiting on the other side
         */
        if (status.waiting_for_ack == Q_TRUE) {
            /*
             * We are waiting for a new ZRPOS, check timeout
             */
            if (check_timeout() == Q_TRUE) {
                /*
                 * Resend the ZCRCW for recovery
                 */
                status.ack_required = Q_TRUE;
                status.waiting_for_ack = Q_FALSE;
            } else {
                /*
                 * No timeout, exit out
                 */
                return Q_TRUE;
            }
        }

    } /* if (packet_buffer_n > 0) */

    if ((status.waiting_for_ack == Q_FALSE) &&
        (status.ack_required == Q_FALSE)) {

        /*
         * Send more data if it's available (or we are right at the end) AND
         * there is room in the output buffer.
         */
        if (((!feof(status.file_stream)) ||
                (ftell(status.file_stream) == status.file_size)) &&
            (outbound_packet_n == 0)
        ) {

            if (output_max - *output_n < (2 * status.block_size)) {
                /*
                 * There isn't enough space in output, instead put the data
                 * in outbound_packet where it will be queued for later.
                 */
                DLOG(("send_zdata(): switch to outbound_packet\n"));
                use_spare_packet = Q_TRUE;
                assert(outbound_packet_n == 0);
            }

            set_transfer_stats_last_message("ZDATA");

            DLOG(("send_zdata(): read %d bytes from file\n",
                    status.block_size));

            rc = fread(packet.data, 1, status.block_size, status.file_stream);
            if (rc < 0) {
                status.state = ABORT;
                set_transfer_stats_last_message(_("DISK I/O ERROR"));
                stop_file_transfer(Q_TRANSFER_STATE_ABORT);
                return Q_TRUE;
            } else if ((rc < status.block_size) || (rc == 0)) {
                /*
                 * Last packet, woo!
                 */
                last_block = Q_TRUE;
                status.file_position = status.file_size;
                q_transfer_stats.bytes_transfer = status.file_size;
            } else {
                status.file_position += status.block_size;
                q_transfer_stats.bytes_transfer += status.block_size;
            }
            packet.data_n = rc;

            /*
             * Increment count
             */
            stats_increment_blocks();

            if (use_spare_packet == Q_TRUE) {
                assert(outbound_packet_n == 0);

                if (last_block == Q_TRUE) {
                    /*
                     * ZCRCW on last block
                     */

                    /*
                     * Make sure we continue to use the right CRC
                     */
                    packet.use_crc32 = status.use_crc32;

                    encode_zdata_bytes(outbound_packet, &outbound_packet_n,
                        sizeof(outbound_packet), ZCRCW);

                    status.waiting_for_ack = Q_TRUE;
                } else {
                    /*
                     * Check window size
                     */
                    status.blocks_ack_count--;
                    if (status.blocks_ack_count == 0) {
                        DLOG(("send_zdata(): Require a ZACK via ZCRCQ \n"));

                        /*
                         * Require a ZACK via ZCRCQ
                         */
                        if (status.reliable_link == Q_TRUE) {
                            status.blocks_ack_count = WINDOW_SIZE_RELIABLE;
                        } else {
                            status.blocks_ack_count = WINDOW_SIZE_UNRELIABLE;
                        }
                        status.waiting_for_ack = Q_TRUE;
                        status.streaming_zdata = Q_TRUE;

                        /*
                         * Make sure we continue to use the right CRC
                         */
                        packet.use_crc32 = status.use_crc32;
                        encode_zdata_bytes(outbound_packet, &outbound_packet_n,
                            sizeof(outbound_packet), ZCRCQ);
                    } else {

                        DLOG(("send_zdata(): Keep streaming with ZCRCG \n"));

                        /*
                         * ZCRCG otherwise
                         */

                        /*
                         * Make sure we continue to use the right CRC
                         */
                        packet.use_crc32 = status.use_crc32;

                        encode_zdata_bytes(outbound_packet, &outbound_packet_n,
                            sizeof(outbound_packet), ZCRCG);
                    }
                }

            } else {

                if (last_block == Q_TRUE) {
                    /*
                     * ZCRCW on last block
                     */

                    /*
                     * Make sure we continue to use the right CRC
                     */
                    packet.use_crc32 = status.use_crc32;

                    encode_zdata_bytes(output, output_n, output_max, ZCRCW);

                    status.waiting_for_ack = Q_TRUE;
                } else {
                    /*
                     * Check window size
                     */
                    status.blocks_ack_count--;
                    if (status.blocks_ack_count == 0) {
                        DLOG(("send_zdata(): Require a ZACK via ZCRCQ \n"));

                        /*
                         * Require a ZACK via ZCRCQ
                         */
                        if (status.reliable_link == Q_TRUE) {
                            status.blocks_ack_count = WINDOW_SIZE_RELIABLE;
                        } else {
                            status.blocks_ack_count = WINDOW_SIZE_UNRELIABLE;
                        }
                        status.waiting_for_ack = Q_TRUE;
                        status.streaming_zdata = Q_TRUE;

                        /*
                         * Make sure we continue to use the right CRC
                         */
                        packet.use_crc32 = status.use_crc32;

                        encode_zdata_bytes(output, output_n, output_max, ZCRCQ);

                    } else {
                        DLOG(("send_zdata(): Keep streaming with ZCRCG \n"));
                        /*
                         * ZCRCG otherwise
                         */

                        /*
                         * Make sure we continue to use the right CRC
                         */
                        packet.use_crc32 = status.use_crc32;

                        encode_zdata_bytes(output, output_n, output_max, ZCRCG);
                    }
                }
            }

        } /* if ((!feof(status.file_stream)) && (outbound_packet_n == 0)) */

    } else if ((status.ack_required == Q_TRUE) &&
        (status.waiting_for_ack == Q_FALSE)
    ) {

        DLOG(("send_zdata(): Send empty ZCRCW on recovery \n"));

        /*
         * Send empty ZCRCW on recovery
         */
        packet.data_n = 0;
        if ((outbound_packet_n > 0) &&
            (sizeof(outbound_packet) - outbound_packet_n > 32)
        ) {
            /*
             * Encode to the other buffer
             */

            /*
             * Make sure we continue to use the right CRC
             */
            packet.use_crc32 = status.use_crc32;

            encode_zdata_bytes(outbound_packet, &outbound_packet_n,
                sizeof(outbound_packet), ZCRCW);

            status.waiting_for_ack = Q_TRUE;

        } else if (output_max - *output_n > 32) {

            /*
             * Encode directly to output
             */

            /*
             * Make sure we continue to use the right CRC
             */
            packet.use_crc32 = status.use_crc32;

            encode_zdata_bytes(output, output_n, output_max, ZCRCW);

            status.waiting_for_ack = Q_TRUE;
        }
    }

    /*
     * Force the queue to fill up on this call.
     */
    if (use_spare_packet == Q_TRUE) {
        return Q_FALSE;
    }

    /*
     * I either sent some data out, or I can't do anything else
     */
    return Q_TRUE;
}

/**
 * Send:  ZEOF
 *
 * @param output a buffer to contain the bytes to send to the remote side
 * @param output_n the number of bytes that this function wrote to output
 * @param output_max the maximum number of bytes this function may write to
 * output
 */
static Q_BOOL send_zeof(unsigned char * output,
                        unsigned int * output_n,
                        const unsigned int output_max) {
    uint32_t options;

    DLOG(("send_zeof()\n"));

    options = status.file_size;
    build_packet(P_ZEOF, options, output, output_n, output_max);
    status.state = ZEOF_WAIT;
    packet_buffer_n = 0;
    return Q_FALSE;
}

/**
 * Send:  ZEOF_WAIT
 *
 * @param output a buffer to contain the bytes to send to the remote side
 * @param output_n the number of bytes that this function wrote to output
 * @param output_max the maximum number of bytes this function may write to
 * output
 */
static Q_BOOL send_zeof_wait(unsigned char * output,
                             unsigned int * output_n,
                             const unsigned int output_max) {

    ZM_PARSE_PACKET rc_pp;

    uint32_t options = 0;
    int discard;

    DLOG(("send_zeof_wait()\n"));

    /*
     * Check the input buffer first
     */
    if (packet_buffer_n > 0) {
        rc_pp = parse_packet(packet_buffer, packet_buffer_n, &discard);

        /*
         * Take the bytes off the stream
         */
        if (discard > 0) {
            memmove(packet_buffer, packet_buffer + discard,
                    packet_buffer_n - discard);
            packet_buffer_n -= discard;
        }

        if ((rc_pp == ZM_PP_CRCERROR) || (rc_pp == ZM_PP_INVALID)) {
            DLOG(("send_zeof_wait(): ERROR garbled header\n"));
            stats_increment_errors(_("GARBLED HEADER"));
            packet_buffer_n = 0;
            build_packet(P_ZNAK, options, output, output_n, output_max);
            return Q_TRUE;
        }

        if (rc_pp == ZM_PP_NODATA) {
            return Q_TRUE;
        }

        if (rc_pp == ZM_PP_OK) {

            if (packet.type == P_ZRINIT) {
                /*
                 * Yippee, done
                 */

                DLOG(("send_zeof_wait() ZRINIT\n"));

                /*
                 * Increase the total batch transfer
                 */
                q_transfer_stats.batch_bytes_transfer += status.file_size;

                q_transfer_stats.state = Q_TRANSFER_STATE_FILE_DONE;
                set_transfer_stats_last_message("ZRINIT");

                fclose(status.file_stream);

                assert(status.file_name != NULL);
                Xfree(status.file_name, __FILE__, __LINE__);
                status.file_name = NULL;
                status.file_stream = NULL;

                /*
                 * Setup for the next file.
                 */
                upload_file_list_i++;
                setup_for_next_file();

            } else if (packet.type == P_ZNAK) {
                DLOG(("send_zeof_wait(): ERROR ZNAK\n"));
                stats_increment_errors("ZNAK");
                status.state = ZEOF;

            } else {
                /*
                 * Sender isn't Zmodem compliant, abort.
                 */
                status.state = ABORT;
                stop_file_transfer(Q_TRANSFER_STATE_ABORT);
                return Q_TRUE;
            }
        }

        /*
         * Process through the new state
         */
        return Q_FALSE;

    }

    if (check_timeout() == Q_TRUE) {
        status.state = ZEOF;
        return Q_FALSE;
    }

    /*
     * No data, done
     */
    return Q_TRUE;

}

/**
 * Send:  ZFIN
 *
 * @param output a buffer to contain the bytes to send to the remote side
 * @param output_n the number of bytes that this function wrote to output
 * @param output_max the maximum number of bytes this function may write to
 * output
 */
static Q_BOOL send_zfin(unsigned char * output,
                        unsigned int * output_n,
                        const unsigned int output_max) {

    uint32_t options;

    DLOG(("send_zfin()\n"));

    options = 0;
    build_packet(P_ZFIN, options, output, output_n, output_max);
    status.state = ZFIN_WAIT;
    packet_buffer_n = 0;
    return Q_FALSE;
}

/**
 * Send:  ZFIN_WAIT
 *
 * @param output a buffer to contain the bytes to send to the remote side
 * @param output_n the number of bytes that this function wrote to output
 * @param output_max the maximum number of bytes this function may write to
 * output
 */
static Q_BOOL send_zfin_wait(unsigned char * output,
                             unsigned int * output_n,
                             const unsigned int output_max) {

    ZM_PARSE_PACKET rc_pp;

    int discard;
    uint32_t options = 0;

    DLOG(("send_zfin_wait()\n"));

    if (packet_buffer_n > 0) {
        rc_pp = parse_packet(packet_buffer, packet_buffer_n, &discard);

        /*
         * Take the bytes off the stream
         */
        if (discard > 0) {
            memmove(packet_buffer, packet_buffer + discard,
                    packet_buffer_n - discard);
            packet_buffer_n -= discard;
        }

        if ((rc_pp == ZM_PP_CRCERROR) || (rc_pp == ZM_PP_INVALID)) {
            DLOG(("send_zfin_wait(): ERROR garbled header\n"));
            stats_increment_errors(_("GARBLED HEADER"));
            packet_buffer_n = 0;
            build_packet(P_ZNAK, options, output, output_n, output_max);
            return Q_TRUE;
        }

        if (rc_pp == ZM_PP_NODATA) {
            return Q_TRUE;
        }

        if (rc_pp == ZM_PP_OK) {

            if (packet.type == P_ZFIN) {
                DLOG(("send_zfin_wait(): ZFIN, sending Over-and-Out\n"));
                /*
                 * Send Over-and-Out
                 */
                output[0] = 'O';
                output[1] = 'O';
                *output_n = 2;

                /*
                 * Now switch to COMPLETE
                 */
                status.state = COMPLETE;
                set_transfer_stats_last_message(_("SUCCESS"));
                stop_file_transfer(Q_TRANSFER_STATE_END);
                time(&q_transfer_stats.end_time);
            } else if (packet.type == P_ZNAK) {
                DLOG(("send_zfin_wait(): ERROR ZNAK\n"));
                stats_increment_errors("ZNAK");
                status.state = ZFIN;

            } else if (packet.type == P_ZRINIT) {
                DLOG(("send_zfin_wait(): ERROR ZRINIT\n"));
                stats_increment_errors("ZRINIT");
                status.state = ZFIN;

            } else {
                /*
                 * Sender isn't Zmodem compliant, abort.
                 */
                status.state = ABORT;
                stop_file_transfer(Q_TRANSFER_STATE_ABORT);
                return Q_TRUE;
            }
        }

        /*
         * Process through the new state
         */
        return Q_FALSE;

    }

    if (check_timeout() == Q_TRUE) {
        status.state = ZFIN;
        return Q_FALSE;
    }

    /*
     * No data, done
     */
    return Q_TRUE;

}

/**
 * Send a file via the Zmodem protocol.
 *
 * @param input the bytes from the remote side
 * @param input_n the number of bytes in input_n
 * @param output a buffer to contain the bytes to send to the remote side
 * @param output_n the number of bytes that this function wrote to output
 * @param output_max the maximum number of bytes this function may write to
 * output
 */
static void zmodem_send(unsigned char * input, unsigned int input_n,
                        unsigned char * output, unsigned int * output_n,
                        const unsigned int output_max) {
    unsigned int i;
    Q_BOOL done;
    static int can_count = 0;

    done = Q_FALSE;
    while (done == Q_FALSE) {

        DLOG(("zmodem_send() START input_n = %d output_n = %d\n", input_n,
                *output_n));
        DLOG(("zmodem_send() START packet_buffer_n = %d packet_buffer = ",
                packet_buffer_n));
        for (i = 0; i < packet_buffer_n; i++) {
            DLOG2(("%02x ", (packet_buffer[i] & 0xFF)));
        }
        DLOG2(("\n"));

        /*
         * Add input_n to packet_buffer
         */
        if (input_n > sizeof(packet_buffer) - packet_buffer_n) {
            memcpy(packet_buffer + packet_buffer_n, input,
                   sizeof(packet_buffer) - packet_buffer_n);
            memmove(input, input + sizeof(packet_buffer) - packet_buffer_n,
                    input_n - (sizeof(packet_buffer) - packet_buffer_n));
            input_n -= (sizeof(packet_buffer) - packet_buffer_n);
            packet_buffer_n = sizeof(packet_buffer);
        } else {
            memcpy(packet_buffer + packet_buffer_n, input, input_n);
            packet_buffer_n += input_n;
            input_n = 0;
        }

        /*
         * Scan for 4 consecutive C_CANs
         */
        for (i = 0; i < packet_buffer_n; i++) {
            if (packet_buffer[i] != C_CAN) {
                can_count = 0;
            } else {
                can_count++;
            }
            if (can_count >= 4) {
                /*
                 * Receiver has killed the transfer
                 */
                status.state = ABORT;
                set_transfer_stats_last_message(
                    _("TRANSFER CANCELLED BY RECEIVER"));
                stop_file_transfer(Q_TRANSFER_STATE_ABORT);
            }
        }

        if (outbound_packet_n > 0) {
            /*
             * Dispatch whatever is in outbound_packet
             */
            int n = output_max - *output_n;
            if (n > outbound_packet_n) {
                n = outbound_packet_n;
            }
            DLOG(("zmodem_send() dispatch only %d bytes outbound_packet\n", n));
            DLOG(("zmodem_send() output_max %d output_n %d n %d\n",
                    output_max, *output_n, n));
            DLOG(("zmodem_send() outbound_packet: %d bytes: ",
                    outbound_packet_n));
            for (i = 0; i < outbound_packet_n; i++) {
                DLOG2(("%02x ", (outbound_packet[i] & 0xFF)));
            }
            DLOG2(("\n"));

            if (n > 0) {
                memcpy(output + *output_n, outbound_packet, n);
                memmove(outbound_packet, outbound_packet + n,
                        outbound_packet_n - n);
                outbound_packet_n -= n;
                *output_n += n;
            }

            /*
             * Do nothing else
             */
            done = Q_TRUE;

        } else {

            switch (status.state) {

            case INIT:
                /*
                 * This state is where everyone begins.  Start with ZRQINIT
                 */
                status.state = ZRQINIT;
                set_transfer_stats_last_message("ZRQINIT");
                break;

            case ZSINIT:
                done = send_zsinit(output, output_n, output_max);
                break;

            case ZSINIT_WAIT:
                done = send_zsinit_wait(output, output_n, output_max);
                break;

            case ZRQINIT:
                done = send_zrqinit(output, output_n, output_max);
                break;

            case ZRQINIT_WAIT:
                done = send_zrqinit_wait(output, output_n, output_max);
                break;

            case ZFILE:
                done = send_zfile(output, output_n, output_max);
                break;

            case ZFILE_WAIT:
                done = send_zfile_wait(output, output_n, output_max);
                break;

            case ZDATA:
                done = send_zdata(output, output_n, output_max);
                break;

            case ZEOF:
                done = send_zeof(output, output_n, output_max);
                break;

            case ZEOF_WAIT:
                done = send_zeof_wait(output, output_n, output_max);
                break;

            case ZFIN:
                done = send_zfin(output, output_n, output_max);
                break;

            case ZFIN_WAIT:
                done = send_zfin_wait(output, output_n, output_max);
                break;

            case ABORT:
            case COMPLETE:
                /*
                 * NOP
                 */
                done = Q_TRUE;
                break;

            case ZCRC:
            case ZCRC_WAIT:
            case ZRINIT:
            case ZRINIT_WAIT:
            case ZRPOS:
            case ZRPOS_WAIT:
            case ZCHALLENGE:
            case ZCHALLENGE_WAIT:
            case ZSKIP:
                /*
                 * Send should NEVER see these states
                 */
                abort();
                break;
            } /* switch (status.state) */

        }

        DLOG(("zmodem_send(): done = %s\n",
                (done == Q_TRUE ? "true" : "false")));

    } /* while (done == Q_FALSE) */


    /*
     * DLOG(("zmodem_send() NOISE\n"));
     * static int noise = 0;
     * noise++;
     * if (noise > 30) {
     *     noise = 0;
     *     output[0] = 0xaa;
     * }
     */

    DLOG(("zmodem_send() END packet_buffer_n = %d packet_buffer = ",
            packet_buffer_n));
    for (i = 0; i < packet_buffer_n; i++) {
        DLOG2(("%02x ", (packet_buffer[i] & 0xFF)));
    }
    DLOG2(("\n"));

}

/**
 * Process raw bytes from the remote side through the transfer protocol.  See
 * also protocol_process_data().
 *
 * @param input the bytes from the remote side
 * @param input_n the number of bytes in input_n
 * @param output a buffer to contain the bytes to send to the remote side
 * @param output_n the number of bytes that this function wrote to output
 * @param output_max the maximum number of bytes this function may write to
 * output
 */
void zmodem(unsigned char * input, const unsigned int input_n,
            unsigned char * output, unsigned int * output_n,
            const unsigned int output_max) {

    unsigned int i;

    /*
     * Check my input arguments
     */
    assert(input_n >= 0);
    assert(input != NULL);
    assert(output != NULL);
    assert(*output_n >= 0);
    assert(output_max > ZMODEM_MAX_BLOCK_SIZE);

    if ((status.state == ABORT) || (status.state == COMPLETE)) {
        return;
    }

    DLOG(("ZMODEM: state = %d input_n = %d output_n = %d\n", status.state,
            input_n, *output_n));
    DLOG(("ZMODEM: %d input bytes:  ", input_n));
    for (i = 0; i < input_n; i++) {
        DLOG2(("%02x ", (input[i] & 0xFF)));
    }
    DLOG2(("\n"));

    if (input_n > 0) {
        /*
         * Something was sent to me, so reset timeout
         */
        reset_timer();
    }

    if (status.sending == Q_FALSE) {
        zmodem_receive(input, input_n, output, output_n, output_max);
    } else {
        zmodem_send(input, input_n, output, output_n, output_max);
    }

    DLOG(("ZMODEM: %d output bytes: ", *output_n));
    for (i = 0; i < *output_n; i++) {
        DLOG2(("%02x ", (output[i] & 0xFF)));
    }
    DLOG2(("\n"));

    /*
     * Reset the timer if we sent something
     */
    if (*output_n > 0) {
        reset_timer();
    }
}

/**
 * Setup for a new file transfer session.
 *
 * @param file_list list of files to upload, or NULL if this will be a
 * download.
 * @param pathname the path to save downloaded files to
 * @param send if true, this is an upload: file_list must be valid and
 * pathname is ignored.  If false, this is a download: file_list must be NULL
 * and pathname will be used.
 * @param in_flavor the type of Zmodem transfer to perform
 * @return true if successful
 */
Q_BOOL zmodem_start(struct file_info * file_list, const char * pathname,
                    const Q_BOOL send, const ZMODEM_FLAVOR in_flavor, int progress_len) {

    int i;

    /*
     * If I got here, then I know that all the files in file_list exist.
     * forms.c ensures the files are all readable by me.
     */

    /*
     * Verify that file_list is set when send is true
     */
    if (send == Q_TRUE) {
        assert(file_list != NULL);
    } else {
        assert(file_list == NULL);
    }

    /*
     * Assume we don't start up successfully
     */
    status.state = ABORT;

    upload_file_list = file_list;
    upload_file_list_i = 0;

    DLOG(("ZMODEM: START flavor = %d pathname = \'%s\'\n", in_flavor,
            pathname));

    if (upload_file_list != NULL) {
        for (i = 0; upload_file_list[i].name != NULL; i++) {
            DLOG(("upload_file_list[%d] = '%s'\n", i,
                    upload_file_list[i].name));
        }
    }

    status.sending = send;

    if (send == Q_TRUE) {
        /*
         * Set up for first file
         */
        if (setup_for_next_file() == Q_FALSE) {
            return Q_FALSE;
        }
    } else {
        /*
         * Save download path
         */
        download_path = Xstrdup(pathname, __FILE__, __LINE__);
        set_transfer_stats_filename("");
        set_transfer_stats_pathname(pathname);
    }

    if (in_flavor == Z_CRC32) {
        makecrc();
        if (send != Q_TRUE) {
            /*
             * We aren't allowed to send in CRC32 unless the receiver asks
             * for it.
             */
            status.use_crc32 = Q_TRUE;
        }
    } else {
        status.use_crc32 = Q_FALSE;
    }

    status.state = INIT;

    /*
     * Set block size
     */
    q_transfer_stats.block_size = ZMODEM_BLOCK_SIZE;
    status.confirmed_bytes = 0;
    status.last_confirmed_bytes = 0;
    status.consecutive_errors = 0;

    /*
     * Set the window size
     */
    status.reliable_link = Q_TRUE;
    status.blocks_ack_count = WINDOW_SIZE_RELIABLE;
    status.streaming_zdata = Q_FALSE;

    /*
     * Clear the last message
     */
    set_transfer_stats_last_message("");

    /*
     * Clear the packet buffer
     */
    packet_buffer_n = 0;
    outbound_packet_n = 0;

    /*
     * Setup timer
     */
    reset_timer();
    status.timeout_count = 0;

    /*
     * Initialize the encode map
     */
    setup_encode_byte_map();

    progress_length = progress_len;

    DLOG(("ZMODEM: START OK\n"));
    return Q_TRUE;
}

/**
 * Stop the file transfer.  Note that this function is only called in
 * stop_file_transfer() and save_partial is always true.  However it is left
 * in for API completeness.
 *
 * @param save_partial if true, save any partially-downloaded files.
 */
void zmodem_stop(const Q_BOOL save_partial) {
    char notify_message[DIALOG_MESSAGE_SIZE];

    DLOG(("ZMODEM: STOP\n"));

    if ((save_partial == Q_TRUE) || (status.sending == Q_TRUE)) {
        if (status.file_stream != NULL) {
            fflush(status.file_stream);
            fclose(status.file_stream);
        }
    } else {
        if (status.file_stream != NULL) {
            fclose(status.file_stream);
            if (unlink(status.file_name) < 0) {
                snprintf(notify_message, sizeof(notify_message),
                         _("Error deleting file \"%s\": %s\n"), status.file_name,
                         strerror(errno));
#if 0
                notify_form(notify_message, 0);
#endif
            }
        }
    }
    status.file_stream = NULL;
    if (status.file_name != NULL) {
        Xfree(status.file_name, __FILE__, __LINE__);
    }
    status.file_name = NULL;
    if (download_path != NULL) {
        Xfree(download_path, __FILE__, __LINE__);
    }
    download_path = NULL;

}

Q_BOOL zmodem_is_processing(int *progress_cur, int *progress_len) {
    *progress_len = progress_length;

    if (status.state == COMPLETE) {
        *progress_cur = progress_length;

        return Q_FALSE;
    }

    if (status.file_size > 0 && status.file_position < status.file_size) {
        *progress_cur = status.file_position * progress_length / status.file_size;
    } else {
        *progress_cur = 0;
    }

    if (status.state == ABORT) {
        return Q_FALSE;
    } else {
        return Q_TRUE;
    }
}

#if 0
void main(void) {
    unsigned char input[4096];
    unsigned char output[ZMODEM_MAX_BLOCK_SIZE + 1];
    unsigned int len;
    zmodem_start(NULL, "download", Q_FALSE, Z_CRC16);

    input[0] = '\0';
    zmodem(input, 0, output, &len, sizeof(output));
    printf("%d\n", len);

    unsigned int i;
    for (i = 0; i < len; i++) {
        printf("%c", output[i]);
    }
    printf("\n");
}
#endif

#if 0
void main(void) {
    unsigned char input[4096];
    unsigned char output[ZMODEM_MAX_BLOCK_SIZE + 1];
    unsigned int len;
    static struct file_info info[2];
    info[0].name = "/home/ken/.signature";
    stat("/home/ken/.signature", &info[0].fstats);
    info[1].name = NULL;

    zmodem_start(info, "download", Q_TRUE, Z_CRC16);

    input[0] = '\0';
    zmodem(input, 0, output, &len, sizeof(output));
    printf("%d\n", len);

    unsigned int i;
    for (i = 0; i < len; i++) {
        printf("%c", output[i]);
    }
    printf("\n");
}
#endif

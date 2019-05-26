/*
 * zmodem.h
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

#ifndef __ZMODEM_H__
#define __ZMODEM_H__

/* Includes --------------------------------------------------------------- */

#include <sys/stat.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Defines ---------------------------------------------------------------- */

/**
 * view_directory() and batch_entry_window() need to return both the file
 * name and size, so we have a struct to combine these.
 */
struct file_info {
    char * name;
    struct stat fstats;
};

/**
 * The flavors of Zmodem that are supported.
 */
typedef enum {
    Z_CRC16,                    /* Zmodem 16-bit CRC */
    Z_CRC32                     /* Zmodem 32-bit CRC */
} ZMODEM_FLAVOR;

#define Q_BOOL int
#define Q_FALSE 0
#define Q_TRUE 1

/*
 * Technically, Zmodem maxes at 1024 bytes, but each byte might be
 * CRC-escaped to twice its size. Then we've got the CRC escape itself to
 * include.
 */
#define ZMODEM_BLOCK_SIZE       1024
#define ZMODEM_MAX_BLOCK_SIZE   (2 * (ZMODEM_BLOCK_SIZE + 4 + 1))

/* Globals ---------------------------------------------------------------- */

/* Functions -------------------------------------------------------------- */

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
extern void zmodem(unsigned char * input, const unsigned int input_n,
                   unsigned char * output, unsigned int * output_n,
                   const unsigned int output_max);

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
extern Q_BOOL zmodem_start(struct file_info * file_list, const char * pathname,
                           const Q_BOOL send, const ZMODEM_FLAVOR in_flavor);

/**
 * Stop the file transfer.  Note that this function is only called in
 * stop_file_transfer() and save_partial is always true.  However it is left
 * in for API completeness.
 *
 * @param save_partial if true, save any partially-downloaded files.
 */
extern void zmodem_stop(const Q_BOOL save_partial);

extern Q_BOOL zmodem_is_processing(int *progress);

#ifdef __cplusplus
}
#endif

#endif /* __ZMODEM_H__ */

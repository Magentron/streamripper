/*
 * https.h - Header file for HTTPS support in Streamripper.
 *
 * This file contains the function prototypes and data structures needed
 * to establish and manage a secure HTTPS connection using OpenSSL.
 *
 * NOTE: This is a foundational implementation. It requires a proper
 * build environment with OpenSSL libraries linked and would need
 * to be integrated into the main application's logic (e.g., in connect.c)
 * to be fully functional.
 */

#ifndef __HTTPS_H
#define __HTTPS_H

#include "http.h"

/**
 * Initialize OpenSSL library.
 */
void
https_init();

/*
 * This is the main entry function to establish an HTTPS connection.
 * It performs a secure socket connection, SSL handshake, and sends
 * the initial HTTP request.
 *
 * @param host        The host to connect to.
 * @param port        The port to connect to (typically 443).
 * @param request_url The full URL path and query string for the request.
 * @param headers     A list of key/value pairs for custom headers.
 *
 * @return A pointer to a newly allocated https_sc_data structure on success,
 * or NULL on failure. The caller is responsible for freeing this
 * memory and closing the connection using a corresponding cleanup function.
 */
error_code https_sc_connect(
    RIP_MANAGER_INFO *rmi,
    HTTPS_DATA **data,
	const char *url,
    const char *proxyurl,
    SR_HTTP_HEADER *info,
	const char *useragent,
	const char *if_name);

/*
 * NOTE: https_sc_send_request() was removed because it referenced an undefined
 * 'headers' variable and is not needed. Request sending is now handled directly
 * in https_sc_connect() via SSL_write after http_construct_sc_request().
 */

/*
 * Reads HTTP response headers over SSL.
 *
 * @param rmi  The rip manager info structure.
 * @param data A pointer to the HTTPS_DATA structure.
 * @param url  The URL of the stream.
 * @param info The structure to store parsed header info.
 *
 * @return SR_SUCCESS on success, error code on failure.
 */
error_code https_get_sc_header(
    RIP_MANAGER_INFO *rmi,
    HTTPS_DATA *data,
    const char *url,
    SR_HTTP_HEADER *info);

/*
 * Reads a single line from the secure connection.
 *
 * @param data  A pointer to the HTTPS_DATA structure.
 * @param buffer The buffer to store the read line.
 * @param size  The size of the buffer.
 *
 * @return The number of bytes read, or -1 on failure.
 */
int https_sc_read_line(HTTPS_DATA *data, char *buffer, size_t size);

/*
 * Reads a specified number of bytes from the secure connection.
 *
 * @param data A pointer to the HTTPS_DATA structure.
 * @param buffer The buffer to store the read data.
 * @param size The number of bytes to read.
 *
 * @return The number of bytes read, or -1 on failure.
 */
int https_sc_read_data(HTTPS_DATA *data, char *buffer, size_t size);

/*
 * Closes the secure connection and frees allocated resources.
 *
 * @param data A pointer to the HTTPS_DATA structure.
 */
void https_sc_close(HTTPS_DATA *data);

#endif // __HTTPS_H


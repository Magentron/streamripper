/*
 * https.c - Source file for HTTPS support in Streamripper.
 *
 * This file implements the functions defined in https.h.
 * It handles secure socket creation, SSL/TLS handshake, and data transfer
 * using the OpenSSL library.
 */

#include "https.h"
#include "debug.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#if OPENSSL_VERSION_NUMBER < 0x10100000L
#define TLS_client_method SSLv23_client_method
#endif

/* http_parse_url is declared in http.h */

// Helper function to print OpenSSL errors
static void print_ssl_error(const char *message) {
    fprintf(stderr, "HTTPS Error: %s\n", message);
    ERR_print_errors_fp(stderr);
}

// Helper function to get the IP address for a given hostname
static int resolve_hostname(const char *host, struct sockaddr_in *addr) {
    struct hostent *he;
    if ((he = gethostbyname(host)) == NULL) {
        herror("gethostbyname");
        return -1;
    }
    memcpy(&addr->sin_addr, he->h_addr_list[0], he->h_length);
    return 0;
}

// Helper function to create a regular TCP socket and connect
static error_code
create_and_connect_tcp_socket(int *sd, const char *host, int port) {
    struct sockaddr_in server_addr;

    // Create a socket
    if ((*sd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        return SR_ERROR_CANT_CREATE_SOCKET;
    }

    // Resolve hostname to IP address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if (resolve_hostname(host, &server_addr) == -1) {
        close(*sd);
        return SR_ERROR_CANT_RESOLVE_HOSTNAME;
    }

    // Connect to the server
    if (connect(*sd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("connect");
        close(*sd);
        return SR_ERROR_CONNECT_FAILED;
    }

    return SR_SUCCESS;
}

void https_init() {
#if OPENSSL_VERSION_NUMBER < 0x10100000L
    // OpenSSL 1.0.x needs explicit initialization
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
#endif
    // OpenSSL 1.1.0+ auto-initializes
}

/*
 * Main function to connect to a secure stream.
 * It creates a TCP socket, then wraps it in an SSL session.
 */
error_code
https_sc_connect(
    RIP_MANAGER_INFO *rmi,
    HTTPS_DATA **data_ptr,
	const char *url,
    const char *proxyurl,
    SR_HTTP_HEADER *info,
    const char *useragent,
    const char *if_name) {
	char headbuf[MAX_HEADER_LEN];
	URLINFO url_info;
	int ret;
	HTTPS_DATA *data;

    data = malloc(sizeof(HTTPS_DATA));
    *data_ptr = data;
	while (1) {
		debug_printf("***** HTTPS = %s *****\n", url);
		debug_printf("https_sc_connect(): calling http_parse_url\n");
		if ((ret = http_parse_url(url, &url_info)) != SR_SUCCESS) {
			return ret;
		}

	    memset(data, 0, sizeof(HTTPS_DATA));
	    memset(headbuf, 0, MAX_HEADER_LEN);

	    // Create the SSL context
		debug_printf("https_sc_connect(): creating SSL context\n");
	    const SSL_METHOD *method = TLS_client_method();
	    SSL_CTX *ctx = SSL_CTX_new(method);
	    if (ctx == NULL) {
	        print_ssl_error("SSL_CTX_new failed");
	        free(data);
	        return SR_ERROR_SSL_CTX_NEW;
	    }

	    // Set minimum TLS version to 1.2
	    SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);

	    // Create a regular TCP socket and connect to the host
		debug_printf("https_sc_connect(): creating socket\n");
	    ret = create_and_connect_tcp_socket(&data->sd, url_info.host, url_info.port);
	    if (ret != SR_SUCCESS) {
	        SSL_CTX_free(ctx);
	        free(data);
	        return ret;
	    }

	    // Create the SSL session object
		debug_printf("https_sc_connect(): creating SSL session\n");
	    data->ssl = SSL_new(ctx);
	    if (data->ssl == NULL) {
	        print_ssl_error("SSL_new failed");
	        SSL_CTX_free(ctx);
	        close(data->sd);
	        free(data);
	        return SR_ERROR_SSL_NEW;
	    }

	    // Associate the socket with the SSL session
		debug_printf("https_sc_connect(): associating socket with SSL session\n");
	    if (SSL_set_fd(data->ssl, data->sd) == 0) {
	        print_ssl_error("SSL_set_fd failed");
	        SSL_free(data->ssl);
	        SSL_CTX_free(ctx);
	        close(data->sd);
	        free(data);
	        return SR_ERROR_SSL_SET_FD;
	    }

	    // Set SNI (Server Name Indication) - required for most modern HTTPS servers
	    debug_printf("https_sc_connect(): setting SNI hostname: %s\n", url_info.host);
	    SSL_set_tlsext_host_name(data->ssl, url_info.host);

	    // Perform the SSL/TLS handshake
		debug_printf("https_sc_connect(): performing SSL/TLS handshake\n");
	    if (SSL_connect(data->ssl) <= 0) {
	        print_ssl_error("SSL_connect failed");
	        SSL_free(data->ssl);
	        SSL_CTX_free(ctx);
	        close(data->sd);
	        free(data);
	        return SR_ERROR_SSL_CONNECT;
	    }

	    // Clean up the context now that the SSL session is created
	    SSL_CTX_free(ctx);

		debug_printf("https_sc_connect(): calling http_construct_sc_request\n");
		ret = http_construct_sc_request(rmi, url, NULL, headbuf, useragent);
		if (ret != SR_SUCCESS) {
			return ret;
		}

		// Send the HTTP request over SSL
		debug_printf("https_sc_connect(): sending request via SSL_write\n");
		int ssl_ret = SSL_write(data->ssl, headbuf, strlen(headbuf));
		if (ssl_ret <= 0) {
			print_ssl_error("SSL_write failed");
			SSL_free(data->ssl);
			close(data->sd);
			free(data);
			return SR_ERROR_SEND_FAILED;
		}

		// Read and parse response headers
		debug_printf("https_sc_connect(): reading response headers\n");
		ret = https_get_sc_header(rmi, data, url, info);
		if (ret != SR_SUCCESS) {
			SSL_free(data->ssl);
			close(data->sd);
			free(data);
			return ret;
		}

		if (*info->http_location) {
			/* RECURSIVE CASE */
			debug_printf("Redirecting: %s\n", info->http_location);
			url = info->http_location;
			if ((ret = http_parse_url(url, &url_info)) != SR_SUCCESS) {
				return ret;
			}

			// clean up
			SSL_free(data->ssl);
			close(data->sd);
			free(data);

			if (!url_is_https(url)) {
				return SR_ERROR_HTTP_REDIRECT;
			}

			ret = https_sc_connect(
				rmi,
				data_ptr,
				url,
				NULL,
				info,
				useragent,
				if_name);

			return ret;
		} else {
			break;
		}
	}

	return SR_SUCCESS;
}

/*
 * NOTE: https_sc_send_request() was removed because it referenced an undefined
 * 'headers' variable and is not needed. Request sending is now handled directly
 * in https_sc_connect() via SSL_write after http_construct_sc_request().
 */

/*
 * Reads a line from the SSL connection. It's a simplified
 * version that reads byte-by-byte until a newline is found.
 */
int https_sc_read_line(HTTPS_DATA *data, char *buffer, size_t size) {
    if (data == NULL || data->ssl == NULL) {
        return -1;
    }

    int bytes_read = 0;
    char c;

    while (bytes_read < size - 1) {
        int read_result = SSL_read(data->ssl, &c, 1);
        if (read_result <= 0) {
            if (SSL_get_error(data->ssl, read_result) == SSL_ERROR_ZERO_RETURN) {
                // Connection closed gracefully
                return 0;
            }
            print_ssl_error("SSL_read failed in read_line");
            return -1;
        }

        buffer[bytes_read++] = c;
        if (c == '\n') {
            break;
        }
    }

    buffer[bytes_read] = '\0'; // Null-terminate the string
    return bytes_read;
}

/*
 * Reads a block of data from the SSL connection.
 * Loops to read until we have all requested bytes (like socklib_recvall).
 */
int https_sc_read_data(HTTPS_DATA *data, char *buffer, size_t size) {
    if (data == NULL || data->ssl == NULL) {
        return -1;
    }

    size_t total_read = 0;
    while (total_read < size) {
        int bytes_read = SSL_read(data->ssl, buffer + total_read, size - total_read);
        if (bytes_read <= 0) {
            int ssl_error = SSL_get_error(data->ssl, bytes_read);
            if (ssl_error == SSL_ERROR_ZERO_RETURN) {
                // Connection closed, return what we have
                return total_read > 0 ? (int)total_read : 0;
            }
            if (ssl_error == SSL_ERROR_WANT_READ || ssl_error == SSL_ERROR_WANT_WRITE) {
                // Non-blocking would need retry, but we're blocking
                continue;
            }
            print_ssl_error("SSL_read failed in read_data");
            return total_read > 0 ? (int)total_read : -1;
        }
        total_read += bytes_read;
    }

    return (int)total_read;
}

/*
 * Reads HTTP response headers over SSL.
 */
error_code
https_get_sc_header(
    RIP_MANAGER_INFO *rmi,
    HTTPS_DATA *data,
    const char *url,
    SR_HTTP_HEADER *info) {
    char headbuf[MAX_HEADER_LEN] = {'\0'};
    char *p = headbuf;
    int total = 0;

    // Read header line by line until empty line
    while (total < MAX_HEADER_LEN - 1) {
        int ret = https_sc_read_line(data, p, MAX_HEADER_LEN - total);
        if (ret <= 0) break;
        total += ret;
        p += ret;
        // Check for end of headers (empty line)
        if (ret == 2 && *(p-2) == '\r' && *(p-1) == '\n') break;
        if (ret == 1 && *(p-1) == '\n') break;
    }

    return http_parse_sc_header(url, headbuf, info);
}

/*
 * Closes the SSL connection and frees resources.
 */
void https_sc_close(HTTPS_DATA *data) {
    if (data == NULL) {
        return;
    }

    if (data->ssl) {
        SSL_shutdown(data->ssl);
        SSL_free(data->ssl);
    }
    if (data->sd >= 0) {
        close(data->sd);
    }
    free(data);
}

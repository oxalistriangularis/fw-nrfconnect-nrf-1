/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <stdio.h>
#include <string.h>
#include <dfu_client.h>
#include <net/socket.h>
#include <zephyr/types.h>
#include <logging/log.h>

LOG_MODULE_REGISTER(http_dfu);


#define REQUEST_TEMPLATE "GET %s HTTP/1.1\r\n"\
	"Host: %s\r\n"\
	"Connection: keep-alive\r\n"\
	"Range: bytes=%d-%d\r\n\r\n"


static int httpc_connect(const char *const host, const char *const port, u32_t family,
		  u32_t proto)
{
	int fd;

	if (host == NULL) {
		return -1;
	}

	struct addrinfo *addrinf = NULL;
	struct addrinfo hints = {
	    .ai_family = family,
	    .ai_socktype = SOCK_STREAM,
	    .ai_protocol = proto,
	};

	LOG_ERR("Requesting getaddrinfo() for %s", host);

	/* DNS resolve the port. */
	int rc = getaddrinfo(host, port, &hints, &addrinf);

	if (rc < 0 || (addrinf == NULL)) {
		LOG_ERR("getaddrinfo() failed, err %d", errno);
		return -1;
	}

	struct addrinfo *addr = addrinf;
	struct sockaddr *remoteaddr;

	int addrlen = (family == AF_INET6)
			  ? sizeof(struct sockaddr_in6)
			  : sizeof(struct sockaddr_in);

	/* Open a socket based on the local address. */

	fd = socket(family, SOCK_STREAM, proto);
	if (fd >= 0) {
		/* Look for IPv4 address of the broker. */
		while (addr != NULL) {
			remoteaddr = addr->ai_addr;

			LOG_INF("Resolved address family %d\n", addr->ai_family);
			LOG_INF("Resolved address family %d\n", remoteaddr->sa_family);

			if (remoteaddr->sa_family == family) {
				((struct sockaddr_in *)remoteaddr)->sin_port = htons(80);

				LOG_HEXDUMP_INF((const uint8_t *)remoteaddr, addr->ai_addrlen, "Resolved addr");

				/* TODO: Need to set security setting for HTTPS. */
				rc = connect(fd, remoteaddr, addrlen);
				if (rc == 0) {
					break;
				}
			}
			addr++;
		}
	}

	freeaddrinfo(addrinf);

	if (rc < 0) {
		close(fd);
		fd = -1;
	}

	return fd;
}

static void rxdata_flush(struct dfu_client_object * const dfu)
{
	if (dfu == NULL || dfu->host == NULL || dfu->callback == NULL || dfu->status == DFU_CLIENT_STATUS_IDLE) {
		return;
	}

	int flush_len = recv(dfu->fd, dfu->resp_buf, CONFIG_NRF_DFU_HTTP_MAX_RESPONSE_SIZE, 0);

	LOG_INF("rxdata_flush, len %d\n", flush_len);
	if (flush_len == -1) {
		if (errno == EAGAIN) {
			LOG_ERR("flused %d\n", flush_len);
		} else {
			/* Something is wrong on the socket! */
			dfu->callback(dfu, DFU_CLIENT_EVT_ERROR, errno);
		}
	}

	memset(dfu->resp_buf, 0, CONFIG_NRF_DFU_HTTP_MAX_RESPONSE_SIZE);

}

static int fragment_request(struct dfu_client_object * const dfu, bool flush, bool connnection_close)
{
	if (dfu == NULL || dfu->host == NULL || dfu->callback == NULL || dfu->resource == NULL) {
		LOG_ERR("fragment_request(): Invalid dfu object");
		return -1;
	}

	if (flush == true) {
		rxdata_flush(dfu);
	}

	if (connnection_close == true) {
		(void)close(dfu->fd);
		dfu->fd = -1;
		dfu->fd = httpc_connect(dfu->host, NULL, AF_INET, IPPROTO_TCP);
		if (dfu->fd < 0) {
			LOG_ERR("fragment_request(): httpc_connect() failed, err %d", errno);
			dfu->callback(dfu, DFU_CLIENT_EVT_ERROR, ECONNRESET);
			return -1;
		}
	}

	memset(dfu->req_buf, 0, CONFIG_NRF_DFU_HTTP_MAX_REQUEST_SIZE);

	int request_len = snprintf(dfu->req_buf, CONFIG_NRF_DFU_HTTP_MAX_REQUEST_SIZE, REQUEST_TEMPLATE, dfu->resource,
							dfu->host, dfu->download_size,
							(dfu->download_size + CONFIG_NRF_DFU_HTTP_MAX_FRAGMENT_SIZE - 1));

	LOG_INF("fragment_request(), request length %d, state = %d\n", request_len, dfu->status);

	if (request_len > 0) {
		dfu->status = DFU_CLIENT_STATUS_DOWNLOAD_INPROGRESS;
		LOG_INF("Request: %s", dfu->req_buf);
		return send(dfu->fd, dfu->req_buf, request_len, 0);
	} else {
		LOG_ERR("Cannot create request, buffer too small!");
		return -1;
	}
}

int dfu_client_init(struct dfu_client_object *const dfu)
{
	LOG_INF("dfu_client_init()\n");

	if (dfu == NULL || dfu->host == NULL || dfu->callback == NULL || dfu->resource == NULL) {
		return -1;
	}

	dfu->fd = -1;
	dfu->status = DFU_CLIENT_STATUS_IDLE;

	return 0;
}

int dfu_client_connect(struct dfu_client_object *const dfu)
{
	int fd;

	if (dfu == NULL || dfu->host == NULL || dfu->callback == NULL) {
		return -1;
	}

	if ((dfu->fd != -1) && (dfu->status == DFU_CLIENT_STATUS_CONNECTED)) {
		LOG_ERR("dfu_client_connect(): already connected, fd %d", dfu->fd);
		return 0;
	}

	/* TODO: Parse the post for name, port and protocol. */
	fd = httpc_connect(dfu->host, NULL, AF_INET, IPPROTO_TCP);
	if (fd < 0) {
		LOG_ERR("dfu_client_connect(): httpc_connect() failed, err %d", errno);
		return -1;
	}


	dfu->fd = fd;
	dfu->status = DFU_CLIENT_STATUS_CONNECTED;

	LOG_INF("dfu_client_connect(): Success! State %d, fd %d", dfu->status, dfu->fd);

	return 0;
}

void dfu_client_disconnect(struct dfu_client_object *const dfu)
{
	if (dfu == NULL || dfu->fd < 0) {
		return;
	}

	close(dfu->fd);
	dfu->fd = -1;
	dfu->status = DFU_CLIENT_STATUS_IDLE;
}

int dfu_client_download(struct dfu_client_object *const dfu)
{
	if (dfu == NULL || dfu->fd < 0 ||  (dfu->status != DFU_CLIENT_STATUS_CONNECTED)) {
		return -1;
	}

	dfu->firmware_size = -1;
	return fragment_request(dfu, false, false);
}

void dfu_client_process(struct dfu_client_object *const dfu)
{
	int len;

	if (dfu == NULL || dfu->fd < 0 || dfu->status != DFU_CLIENT_STATUS_DOWNLOAD_INPROGRESS) {
		return;
	}

	memset(dfu->resp_buf, 0, sizeof(dfu->resp_buf));

	len = recv(dfu->fd, dfu->resp_buf, sizeof(dfu->resp_buf), MSG_PEEK);
	LOG_INF("dfu_client_process(), fd = %d, state = %d, length = %d, errno %d\n", dfu->fd, dfu->status, len, errno);

	if (len == -1) {
		if (errno != EAGAIN) {
			dfu->status = DFU_CLIENT_ERROR;
			dfu->callback(dfu, DFU_CLIENT_EVT_ERROR, ENOTCONN);
		}
		return;
	}
	if (len == 0) {
		dfu->status = DFU_CLIENT_ERROR;
		dfu->callback(dfu, DFU_CLIENT_EVT_ERROR, ECONNRESET);
		return;
	}

	LOG_INF("Received response of size %d", len);

	int payload_size = 0;
	int total_size = 0;

	char *content_range_header = strstr(dfu->resp_buf, "Content-Range: bytes");

	if (content_range_header != NULL) {
		content_range_header += strlen("Content-Range: bytes");
		char *p_start_range = strstr(content_range_header, " ");

		if (p_start_range != NULL) {
			int start_download_size = atoi(p_start_range+1);

			LOG_DBG("Start download_size %d, expected %d", start_download_size, dfu->download_size);
		}
		char *p_totalsize = strstr(content_range_header, "/");

		if (p_totalsize != NULL) {
			total_size = atoi(p_totalsize+1);
			LOG_DBG("Total size %d", total_size);
		}
	} else{
		/* Return and wait for the header to arrive. */
		return;
	}

	char *content_length_header = strstr(dfu->resp_buf, "Content-Length: ");

	if (content_length_header != NULL) {
		content_length_header += strlen("Content-Length: ");
		payload_size = atoi(content_length_header);
	} else {
		/* Return and wait for the header to arrive. */
		return;
	}



	if (total_size)	{
		if (dfu->firmware_size == -1) {
			dfu->firmware_size = total_size;
		} else {
			if (dfu->firmware_size != total_size) {
				LOG_ERR("firmware size changed from %d to %d during downloads?!",
					dfu->firmware_size, total_size);
				dfu->status = DFU_CLIENT_ERROR;
				dfu->callback(dfu, DFU_CLIENT_EVT_ERROR, EFAULT);
			}
		}
	}

	/* Allow a full sized fragment, except the last one which can be smaller. */
	if ((payload_size == CONFIG_NRF_DFU_HTTP_MAX_FRAGMENT_SIZE) ||
		(dfu->download_size + payload_size == dfu->firmware_size)) {

		char *p_payload = strstr(dfu->resp_buf, "\r\n\r\n");
		bool connection_resume = false;

		if (p_payload != NULL) {
			p_payload += strlen("\r\n\r\n");
			int expected_payload_size = len - (int)(p_payload - dfu->resp_buf);

			if (payload_size != expected_payload_size) {
				/* Wait for entire payload. */
				LOG_INF("Expected payload %d, received %d\n", payload_size, expected_payload_size);
				return;
			}

			char *connnection_header = strstr(dfu->resp_buf, "Connection: ");

			if (connnection_header != NULL) {
				connnection_header += strlen("Connection: ");
				char *connnection_close = strstr(connnection_header, "close");
				if (connnection_close != NULL) {
					LOG_INF("Server will close connection, will need to resume\n");
					connection_resume = true;
				}
			}

			/* Parse the response, send the firmware to the modem. and
			 * generate the right events.
			 */
			dfu->fragment = p_payload;
			dfu->fragment_size = payload_size;

			/* Continue download if application returns success, else. Halt. */
			if (dfu->callback(dfu, DFU_CLIENT_EVT_DOWNLOAD_FRAG, 0) == 0) {
				dfu->download_size += payload_size;

				if (dfu->download_size == dfu->firmware_size) {
					dfu->status = DFU_CLIENT_STATUS_DOWNLOAD_COMPLETE;
					if (dfu->callback(dfu, DFU_CLIENT_EVT_DOWNLOAD_DONE, 0) != 0) {
						dfu->download_size -= payload_size;
						fragment_request(dfu, true, connection_resume);
					} else {
						rxdata_flush(dfu);
					}
				} else {
					fragment_request(dfu, true, connection_resume);
				}
			} else {
				dfu->status = DFU_CLIENT_STATUS_HALTED;
			}
		}
	}
}

/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

/**@file dfu_client.h
 *
 * @defgroup dfu_client DFU client for downloading firmware
 * @{
 * @brief DFU client interface to connect to a server and download
 * and apply new firmware to a selected target. Currently, only the
 * HTTP protocol is supported for download.
 */

#ifndef DFU_CLIENT_H__
#define DFU_CLIENT_H__

#include <zephyr/types.h>

#ifdef __cplusplus
extern "C" {
#endif

struct dfu_client_object;

/** @brief DFU download status. */
enum dfu_client_status {
	/** Indicates that the client was either never connected
	 *  to the server or is now disconnected. */
	DFU_CLIENT_STATUS_IDLE      = 0x00,
	/** Indicates that the client is connected to the server
	 *  and there is no ongoing download. */
	DFU_CLIENT_STATUS_CONNECTED = 0x01,
	/** Indicates that the client is connected to the server
	 *  and download is in progress. */
	DFU_CLIENT_STATUS_DOWNLOAD_INPROGRESS = 0x02,
	/** Indicates that the client is connected to the server
	 *  and download is complete. */
	DFU_CLIENT_STATUS_DOWNLOAD_COMPLETE = 0x03,
	/** Indicates that the firmware download is halted by the
	 *  application. This status indicates that the application
	 *  identified a failure when handling the
	 *  @ref DFU_CLIENT_EVT_DOWNLOAD_FRAG event. */
	DFU_CLIENT_STATUS_HALTED = 0x04,
	/** Indicates that an error occured and the download
	 *  cannot continue.
	 */
	DFU_CLIENT_ERROR = 0xFF
};


/** @brief DFU events. */
enum dfu_client_evt {
	/** Indicates an error during download.
	 * The application should disconnect and retry the operation
	 * when receiving this even. */
	DFU_CLIENT_EVT_ERROR = 0x00,
	/** Indicates reception of a fragment of firmware during download.
	 * The fragment field of the @ref dfu_client_object object
	 * points to the firmware fragment, and the fragment size
	 * indicates the size of the fragment. */
	DFU_CLIENT_EVT_DOWNLOAD_FRAG = 0x01,
	/** Indicates that the download is complete. */
	DFU_CLIENT_EVT_DOWNLOAD_DONE = 0x02,
};

/** @brief DFU client event handler.
 *
 * The application is notified of the status of the firmware download
 * through this event handler.
 * The application can use the return value to indicate if it handled
 * the event successfully or not.
 * This feedback is useful when, for example, a faulty fragment was
 * received or the application was unable to process a firmware fragment.
 *
 * @param[in] dfu 	The DFU instance.
 * @param[in] event     The event.
 * @param[in] status    Event status (either 0 or an errno value).
 *
 * @retval 0 If the event was handled successfully.
 *           Other values indicate that the application failed to handle
 *           the event.
 */
typedef int (*dfu_client_event_handler_t)(struct dfu_client_object *dfu, enum dfu_client_evt event,
					u32_t status);

/** @brief Firmware download object that describes the state of download. */
struct dfu_client_object {
	/** Buffer used to receive responses from the server.
	 *  This buffer can be read by the application if need be, never written to. */
	char resp_buf[CONFIG_NRF_DFU_HTTP_MAX_RESPONSE_SIZE];
	/** Buffer used to cerate requests to the server.
	 *  This buffer can be read by the application if need be, never written to. */
	char req_buf[CONFIG_NRF_DFU_HTTP_MAX_REQUEST_SIZE];
	/** Pointer to firmware fragment in the resp_buf.
	 *  The response from the server contains protocol meta-data apart from the firmware ragment.
	 *  The module on every DFU_CLIENT_EVT_DOWNLOAD_FRAG points to the firmware fragment.
	 *  This pointer should not be updated by the application.
	 */
	char *fragment;
	/** Size of the fragment, is updated on every DFU_CLIENT_EVT_DOWNLOAD_FRAG event. */
	int fragment_size;
	/** Transport file descriptor.
	 *  If negative, the transport is disconnected. */
	int fd;
	/** Total size of the firmware being downloaded.
	 * If negative, the download is in progress.
	 * If zero, the size is unknown. */
	int firmware_size;
	/** Current size of the firmware being downloaded. */
	volatile int download_size;
	/** Status of the transfer (see @ref dfu_client_status). */
	volatile int status;
	/** Server that hosts the firmware. */
	const char *host;
	/** Resource to be downloaded. */
	const char *resource;
	/** Event handler. Must not be NULL. */
	const dfu_client_event_handler_t callback;
};

/** @brief Initialize the firmware upgrade object for a given host and resource.
 *
 * The server to connect to for the firmware download is identified by
 * the host field of @p dfu. The callback field of @p dfu must contain an
 * event handler.
 *
 * @note If this method fails, do no call any other APIs for the instance.
 *
 * @param[in,out] dfu The DFU instance. Must not be NULL.
 *	  	      The target, host, resource, and callback fields must be
 *		      correctly initialized in the object instance.
 *		      The fd, status, fragment, fragment_size, download_size,
 *                    and firmware_size fields are out parameters.
 *
 * @retval 0  If the operation was successful.
 * @retval -1 Otherwise.
 */
int dfu_client_init(struct dfu_client_object *dfu);

/**@brief Establish a connection to the server.
 *
 * The server to connect to for the firmware download is identified by
 * the host field of @p dfu. The host field is expected to be a host name
 * that can be resolved to an IP address using DNS.
 *
 * @note This is a blocking call.
 *       Do not initiate a @ref dfu_client_download if this procedure fails.
 *
 * @param[in] dfu The DFU instance.
 *
 * @retval 0  If the operation was successful.
 * @retval -1 Otherwise.
 */
int dfu_client_connect(struct dfu_client_object *dfu);

/**@brief Start downloading the firmware.
 *
 * This is a blocking call used to trigger the download of a firmware image
 * identified by the resource field of @p dfu. The download is requested from
 * the server in chunks of CONFIG_NRF_DFU_HTTP_MAX_FRAGMENT_SIZE.
 *
 * This API may be used to resume an interrupted download by setting the
 * download_size field of @p dfu to the last successfully downloaded fragment
 * of the image.
 *
 * If the API succeeds, use @ref dfu_client_process to advance the download
 * until a @ref DFU_CLIENT_EVT_ERROR or a @ref DFU_CLIENT_EVT_DOWNLOAD_DONE
 * event is received in the registered callback.
 * If the API fails, disconnect using @ref dfu_client_disconnect, then
 * reconnect using @ref dfu_client_connect and restart the procedure.
 *
 * @param[in] dfu The DFU instance.
 *
 * @retval 0  If the operation was successful.
 * @retval -1 Otherwise.
 */
int dfu_client_download(struct dfu_client_object *dfu);

/**@brief Advance the firmware download.
 *
 * Call this API to advance the download state identified by the status
 * field of @p dfu. This is a blocking call. You can poll the fd field of
 * @p dfu to decide whether to call this method.
 *
 * @param[in] dfu The DFU instance.
 */
void dfu_client_process(struct dfu_client_object *dfu);

/**@brief Disconnect from the server.
 *
 * This API terminates the connection to the server. If called before
 * the download is complete, it is possible to resume the interrupted transfer
 * by reconnecting to the server using the @ref dfu_client_connect API and
 * calling @ref dfu_client_download to continue the download.
 * If you want to resume after a power cycle, you must store the download size
 * persistently and supply this value in subsequent reconnections.
 *
 * @note You should disconnect from the server as soon as the download
 * is complete.
 *
 * @param[in] dfu The DFU instance.
 *
 */
void dfu_client_disconnect(struct dfu_client_object *dfu);

#ifdef __cplusplus
}
#endif

#endif /* DFU_CLIENT_H__ */

/**@} */

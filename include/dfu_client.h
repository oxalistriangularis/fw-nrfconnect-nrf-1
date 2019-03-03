/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

/**@file dfu_client.h
 *
 * @defgroup dfu_client DFU client to download and apply new firmware
 * (or patches) to target. Currently, only HTTP protocol is supported for
 * download.
 * @{
 * @brief DFU Client interface to connect to server, download and apply new
 * firmware to a selected target.
 */

#ifndef DFU_CLIENT_H__
#define DFU_CLIENT_H__

#include <zephyr/types.h>

#ifdef __cplusplus
extern "C" {
#endif

struct dfu_client_object;

/**@defgroup dfu_client_status DFU download status.
 *
 * @brief Indicates the status of download.
 * @{
 */
enum dfu_client_status {
	DFU_CLIENT_STATUS_IDLE      = 0x00,				/**< Indicates that the client was either
													 * never connected to the server or is disconnected.
													 */
	DFU_CLIENT_STATUS_CONNECTED = 0x01,				/**< Indicates that the client is connected to the server,
													 * and there is no on going download.
													 */
	DFU_CLIENT_STATUS_DOWNLOAD_INPROGRESS = 0x02,	/**< Indicates that the client is connected to the server,
													 * and download is in progress.
													 */
	DFU_CLIENT_STATUS_DOWNLOAD_COMPLETE = 0x03,		/**< Indicates that the client is connected to the server,
													 * and download is in complete.
													 */
	DFU_CLIENT_STATUS_HALTED = 0x04					/**< Indicates that the firmware download is halted by the application.
													 * This status indicates that the application indicated a failure when
													 * handling the @ref DFU_CLIENT_EVT_DOWNLOAD_FRAG event.
													 */
};
/* @} */

/**@defgroup dfu_client_evt DFU events.
 * @brief The DFU Events.
 * @{
 */
enum dfu_client_evt {
	DFU_CLIENT_EVT_ERROR = 0x00,			/**< Indicates error during download.
											 * The application should disconnect and retry on reception of this event and reconnect.
											 */
	DFU_CLIENT_EVT_DOWNLOAD_FRAG = 0x01,	/**< Indicates reception of a fragment of firmware during download.
											 * The fragment field of the dfu object @ref dfu_client_object points to the firmware fragment
											 * and the fragment size indicates the size of the fragment.
											 */
	DFU_CLIENT_EVT_DOWNLOAD_DONE = 0x02,	/**< Indicates that the download is complete. */
};
/* @} */

/**@brief DFU client event handler.
 *
 * @details The application is notified of the status of firmware download using this event handler.
 *          The application can use the return value to indicate if it handled the event successfully or not.
 *          This feedback is useful when, for example, a fault fragment was received or the application
 *          was unable to process a fimrware fragment.
 *
 * @param[in] dfu 		The DFU instance.
 * @param[in] event     The event.
 * @param[in] status    Event status, can be 0 or an errno value.
 *
 * @retval 0 indicates the event was handled successfully.
 *         Else, the application failed to handle the event.
 */
typedef int (*dfu_client_event_handler_t)(struct dfu_client_object *dfu, enum dfu_client_evt event,
					u32_t status);

/**@brief Firmware download object describing the state of download*/
struct dfu_client_object {
	char resp_buf[CONFIG_NRF_DFU_HTTP_MAX_RESPONSE_SIZE];
	char req_buf[CONFIG_NRF_DFU_HTTP_MAX_REQUEST_SIZE];
	char *fragment;
	int fragment_size;

	int fd;			/**< Transport file descriptor.
					  *  If negative the transport is disconnected.
					  */
	int firmware_size;	/**< Total size of firmware being downloaded.
						 * If negative, download is in progress.
						 * If zero, the size is unknown.
						 */
	volatile int download_size;	/**< Size of firmware being downloaded. */
	volatile int status;		/**< Status of transfer. Will be one of \ref dfu_client_status. */
	const char *host;			   /**< The server that hosts the firmwares. */
	const char *resource;          /**< Resource to be downloaded. */

	const dfu_client_event_handler_t callback; /**< Event handler.
												* Shall not be NULL.
												*/
};

/**@brief Initialize firmware upgrade object for a given host and resource.
 *
 * @details The server to connect to for firmware download is identified by
 * the host. The caller of this method must register an event handler in
 * callback field of the instance.
 *
 * @param[inout] dfu The DFU instance. Shall not be NULL.
 *		     The target, host, resource and callback fields should be
 *		     correctly initialized in the object instance.
 *		     The fd, status, fragment, fragment_size, download_size and
 *           firmware_size fields are out parameters.
 *
 * @retval 0 if the procedure succeeded, else, -1.
 * @note If this method fails, no other APIs shall be called for the instance.
 */
int dfu_client_init(struct dfu_client_object *dfu);

/**@brief Establish connection to the server.
 *
 * @details The server to connect to for firmware download is identified by
 * the host field of the dfu object. The host field is expected to be hostname
 * that can be resolved using DNS to an IP address.
 *
 * @param[in] dfu The DFU instance.
 *
 * @retval 0 if the procedure request succeeded, else, -1.
 *
 * @note This is a blocking call.
 * @note Do not initiate a @ref dfu_client_download if this procedure fails.
 */
int dfu_client_connect(struct dfu_client_object *dfu);

/**@brief Start download of firmware.
 *
 * @breif This is a blocking call used to trigger download of a firmware image identified
 *        by the resource field of the dfu object. The download is requested from the server
 *        in chunks of CONFIG_NRF_DFU_HTTP_MAX_FRAGMENT_SIZE size.
 *        This API may be used to resume to an interrupted download by setting the download_size
 *        field to the last successfully downloaded fragment of the image.
 *        If this procedure succeeds, the application should use @ref dfu_client_process
 *        to advance the download until a DFU_CLIENT_EVT_ERROR, or a DFU_CLIENT_EVT_DOWNLOAD_DONE
 *        event is received in the registered callback.
 *        If this procedure fails, please disconnect using @ref dfu_client_disconnect,
 *        reconnect using @ref dfu_client_connect and restart the procedure.
 *
 * @param[in] dfu The DFU instance.
 *
 * @retval 0 if the procedure succeeded, else, -1.
 */
int dfu_client_download(struct dfu_client_object *dfu);

/**@brief Method to continue advance the firmware download on the current state.
 *
 * @details This API shall be called to advance the download state identified by the status
 *          field of the. This is a blocking call. The calling application may use poll on
 *          the fd in the dfu object to decide whether to call this method or not.
 *
 * @param[in] dfu The DFU instance.
 */
void dfu_client_process(struct dfu_client_object *dfu);

/**@brief Disconnect connection to the server.
 *
 * @details The connection to the server is torndown. If this API is called before
 *          the download is complete, then it is possible to resume the interrupted transfer
 *          by connecting back to the server using the @ref dfu_client_connect API and
 *          @ref dfu_client_download at a convinient opportunity. if the resumption is desired
 *          across power cycle, then the application must store the download_size persistently
 *          and supply this value in subsequent reconnections.
 * @param[in] dfu The DFU instance.
 *
 * @note Its is recommended to disconnect connection as soon as the download is complete.
 */
void dfu_client_disconnect(struct dfu_client_object *dfu);

#ifdef __cplusplus
}
#endif

#endif /* DFU_CLIENT_H__ */

/**@} */

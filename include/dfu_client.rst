.. _lib_dfu_client:

DFU Client library
#################

The DFU client library downloads a firmware for firmware upgrade. The library treats the firmware to be downloaded as an opaque object.
Therefore, the library can be used to donwload any kind of firmware object or any any target. For example, it could the firmware downloaded could be a delta image, or a full image with many merged hex files. The download is agnostic to firmware revision and firmware download location (flash address etc). The firmware by a resource on a remote server. Therefore, firmware revision management is left to the user application.

The module makes no assumption on the size of the firmware to be downloaded. This information is obtained from the server.
The firmware download is requested from the server in fragments of CONFIG_NRF_DFU_HTTP_MAX_FRAGMENT_SIZE. For example, if the firmware size to be downloaded was 60478 bytes and the CONFIG_NRF_DFU_HTTP_MAX_FRAGMENT_SIZE was configured to be 1024 bytes, then the module will request
the firmware download at least (assuming no interruptions and failures) 60 times. And all except the last fragment is allowed to be smaller than CONFIG_NRF_DFU_HTTP_MAX_FRAGMENT_SIZE. The DFU_CLIENT_EVT_DOWNLOAD_FRAG is used to notify the application of each notification


    |<------------- Firmware size ------------------------------------------------->|
	+---------------+---------------+---------------+---------------+         +---------------+
	|   Fragment 1  |   Fragment 2  |   Fragment 3  |   Fragment 4  | ....... | Fragment n    |
	+---------------+---------------+---------------+---------------+		  +---------------+
                    \                \                                                        \
                     \                \           ..............................               \
                      \                \                                                  DFU_CLIENT_EVT_DOWNLOAD_FRAG event,
                       \                \                                                 fragment length < CONFIG_NRF_DFU_HTTP_MAX_FRAGMENT_SIZE.
                        \          DFU_CLIENT_EVT_DOWNLOAD_FRAG event,
                         \         fragment length = CONFIG_NRF_DFU_HTTP_MAX_FRAGMENT_SIZE.
                          \
                           \
                      DFU_CLIENT_EVT_DOWNLOAD_FRAG event,
                      fragment length = CONFIG_NRF_DFU_HTTP_MAX_FRAGMENT_SIZE.

For HTTP, the library uses the Range-requests 'Range' header is used to request firmware fragments of size CONFIG_NRF_DFU_HTTP_MAX_FRAGMENT_SIZE. The firmware size is obtained from the the 'Content-Length' header in the response. The 'Connection: keep-alive' header is included in the request to request the server to keep the TCP connection after partial content response. In event the server includes 'Connection: close' the response, the library will reconnect to the server automatically and resume download.

The application must configure CONFIG_NRF_DFU_HTTP_MAX_FRAGMENT_SIZE optimally to its needs since it is notifed a fragment only once the entire fragment is received, with the exception of the last fragment. Too low value will request in too much protocol overhead, while too large will have an imply large RAM requirement.


Assumptions
***********
The library currently assumes:
* address family is IPv4.
* TCP transport is used for communication with the server.
* the application protocol to communicate to the server to be HTTP 1.1.
* IETF RFC 7233 is assumed to be supported by the HTTP Server.
* the library requires the CONFIG_NRF_DFU_HTTP_MAX_FRAGMENT_SIZE to be configured to contain the entire HTTP response.

Limitations
***********
* HTTPS is not supported.

API documentation
*****************

.. doxygengroup:: dfu_client
   :project: nrf
   :members:

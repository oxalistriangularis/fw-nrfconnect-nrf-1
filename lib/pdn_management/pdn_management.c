/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#include "bsd.h"
#include "nrf_socket.h"


int pdn_init_and_connect(char * apn_name)
{

    int pdn_fd = nrf_socket(NRF_AF_LTE, NRF_SOCK_MGMT, NRF_PROTO_PDN);
    if(pdn_fd >= 0) {
        // Connect to the APN.
        int err = nrf_connect(pdn_fd, apn_name, strlen(apn_name));

        if (err != 0) {
            nrf_close(pdn_fd);
        }
    }

    return pdn_fd;
}


void pdn_disconnect(int pdn_fd)
{
    nrf_close(pdn_fd);
}
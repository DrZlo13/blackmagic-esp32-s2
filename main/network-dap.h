/**
 * @file network-gdb.h
 * @author Sergey Gavrilov (who.just.the.doctor@gmail.com)
 * @version 1.0
 * @date 2021-11-25
 * 
 * GDB server API
 */

#pragma once
#include <stdint.h>

/**
 * Start DAP server
 */
void network_dap_server_init(void);

/**
 * Checks if someone is connected to the DAP server
 * @return bool
 */
bool network_dap_connected(void);

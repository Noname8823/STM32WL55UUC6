/*
 * app_config.h
 *
 *  Created on: May 29, 2026
 *      Author: tranquocvu2
 */

#ifndef INC_APP_CONFIG_H_
#define INC_APP_CONFIG_H_

#include <stdint.h>

typedef enum
{
    DEVICE_ROLE_RX = 0,
    DEVICE_ROLE_TX = 1
} DeviceRole_t;

typedef struct
{
    uint8_t node_id;
    uint8_t dest_id;
    uint8_t role;

    uint32_t frequency;
    int8_t tx_power;

    uint8_t bandwidth;
    uint8_t spreading_factor;
    uint8_t coding_rate;

    uint16_t preamble_len;
    uint8_t symbol_timeout;
} AppConfig_t;

extern AppConfig_t g_app_config;

void AppConfig_SetDefault(void);
void AppConfig_Load(void);
void AppConfig_Save(void);

#endif /* INC_APP_CONFIG_H_ */

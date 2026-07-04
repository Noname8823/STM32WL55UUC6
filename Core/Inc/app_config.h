#ifndef __APP_CONFIG_H
#define __APP_CONFIG_H

#include "main.h"
#include <stdint.h>

typedef enum
{
    DEVICE_ROLE_TX = 0,
    DEVICE_ROLE_RX = 1
} DeviceRole_t;

typedef struct
{
    uint8_t node_id;
    uint8_t dest_id;
    DeviceRole_t role;

    uint32_t frequency;
    int8_t tx_power;

    uint8_t bandwidth;
    uint8_t spreading_factor;
    uint8_t coding_rate;

    uint16_t preamble_len;
    uint16_t symbol_timeout;
} AppConfig_t;

extern AppConfig_t g_app_config;

void AppConfig_SetDefault(void);
void AppConfig_Load(void);
HAL_StatusTypeDef AppConfig_Save(void);

#endif

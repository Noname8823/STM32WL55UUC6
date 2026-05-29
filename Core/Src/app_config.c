/*
 * app_config.c
 *
 *  Created on: May 29, 2026
 *      Author: tranquocvu2
 */


#include "app_config.h"
#include "main.h"
#include <string.h>

#define APP_CONFIG_MAGIC   0xA55A1234U
#define APP_CONFIG_VERSION 1U

typedef struct
{
    uint32_t magic;
    uint32_t version;
    AppConfig_t cfg;
    uint32_t checksum;
} FlashConfigRecord_t;

AppConfig_t g_app_config;

static uint32_t AppConfig_GetFlashAddr(void)
{
    uint32_t flash_kb = *(uint16_t *)FLASHSIZE_BASE;
    return FLASH_BASE + flash_kb * 1024U - FLASH_PAGE_SIZE;
}

static uint32_t AppConfig_Checksum(const AppConfig_t *cfg)
{
    const uint8_t *p = (const uint8_t *)cfg;
    uint32_t sum = 0;

    for (uint32_t i = 0; i < sizeof(AppConfig_t); i++)
    {
        sum += p[i];
    }

    return sum;
}

void AppConfig_SetDefault(void)
{
    g_app_config.node_id = 1;
    g_app_config.dest_id = 0;
    g_app_config.role = DEVICE_ROLE_TX;

    g_app_config.frequency = 433000000;
    g_app_config.tx_power = 14;

    g_app_config.bandwidth = 0;          /* 0=125kHz, 1=250kHz, 2=500kHz */
    g_app_config.spreading_factor = 10;  /* SF7..SF12 */
    g_app_config.coding_rate = 1;        /* 1=4/5, 2=4/6, 3=4/7, 4=4/8 */

    g_app_config.preamble_len = 8;
    g_app_config.symbol_timeout = 5;
}

void AppConfig_Load(void)
{
    FlashConfigRecord_t *rec = (FlashConfigRecord_t *)AppConfig_GetFlashAddr();

    if (rec->magic == APP_CONFIG_MAGIC &&
        rec->version == APP_CONFIG_VERSION &&
        rec->checksum == AppConfig_Checksum(&rec->cfg))
    {
        memcpy(&g_app_config, &rec->cfg, sizeof(AppConfig_t));
    }
    else
    {
        AppConfig_SetDefault();
    }
}

void AppConfig_Save(void)
{
    FlashConfigRecord_t rec;
    uint32_t flash_addr = AppConfig_GetFlashAddr();
    uint32_t page_error = 0;
    FLASH_EraseInitTypeDef erase = {0};

    rec.magic = APP_CONFIG_MAGIC;
    rec.version = APP_CONFIG_VERSION;
    rec.cfg = g_app_config;
    rec.checksum = AppConfig_Checksum(&g_app_config);

    HAL_FLASH_Unlock();

    erase.TypeErase = FLASH_TYPEERASE_PAGES;
    erase.Page = (flash_addr - FLASH_BASE) / FLASH_PAGE_SIZE;
    erase.NbPages = 1;

    HAL_FLASHEx_Erase(&erase, &page_error);

    uint8_t temp[sizeof(FlashConfigRecord_t) + 8];
    memset(temp, 0xFF, sizeof(temp));
    memcpy(temp, &rec, sizeof(FlashConfigRecord_t));

    for (uint32_t i = 0; i < sizeof(FlashConfigRecord_t); i += 8)
    {
        uint64_t data64;
        memcpy(&data64, &temp[i], 8);
        HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, flash_addr + i, data64);
    }

    HAL_FLASH_Lock();
}

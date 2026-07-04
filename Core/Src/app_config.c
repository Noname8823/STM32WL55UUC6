/*
 * app_config.c
 *
 *  Created on: May 29, 2026
 *      Author: tranquocvu2
 */

#include "app_config.h"
#include "main.h"
#include <string.h>
#include <stdint.h>

#define APP_CONFIG_MAGIC       0xA55A1234U
#define APP_CONFIG_VERSION     1U

#define APP_CONFIG_ALIGN_SIZE(size)   (((size) + 7U) & ~7U)

typedef struct
{
    uint32_t magic;
    uint32_t version;
    AppConfig_t cfg;
    uint32_t checksum;
} FlashConfigRecord_t;

#define FLASH_CONFIG_RECORD_SIZE         ((uint32_t)sizeof(FlashConfigRecord_t))
#define FLASH_CONFIG_RECORD_ALIGNED_SIZE APP_CONFIG_ALIGN_SIZE(FLASH_CONFIG_RECORD_SIZE)

AppConfig_t g_app_config;

static uint32_t AppConfig_GetFlashAddr(void)
{
    uint32_t flash_kb = *(uint16_t *)FLASHSIZE_BASE;

    /*
     * Lấy page cuối cùng của Flash để lưu config.
     * Lưu ý: linker script phải chừa page này ra, không được chứa code.
     */
    return FLASH_BASE + flash_kb * 1024U - FLASH_PAGE_SIZE;
}

static uint32_t AppConfig_Checksum(const AppConfig_t *cfg)
{
    const uint8_t *p = (const uint8_t *)cfg;
    uint32_t sum = 0;

    if (cfg == NULL)
    {
        return 0;
    }

    for (uint32_t i = 0; i < sizeof(AppConfig_t); i++)
    {
        sum += p[i];
    }

    return sum;
}

static uint8_t AppConfig_IsValid(const AppConfig_t *cfg)
{
    if (cfg == NULL)
    {
        return 0;
    }

    if (cfg->node_id == 0U || cfg->node_id == 255U)
    {
        return 0;
    }

    if (cfg->role != DEVICE_ROLE_TX &&
        cfg->role != DEVICE_ROLE_RX)
    {
        return 0;
    }

    /*
     * Tạm check theo vùng bạn đang dùng.
     * Nếu sau này dùng 433MHz thì sửa range này lại.
     */
    if (cfg->frequency < 860000000U || cfg->frequency > 930000000U)
    {
        return 0;
    }

    if (cfg->tx_power > 22U)
    {
        return 0;
    }

    if (cfg->bandwidth > 2U)
    {
        return 0;
    }

    if (cfg->spreading_factor < 7U || cfg->spreading_factor > 12U)
    {
        return 0;
    }

    if (cfg->coding_rate < 1U || cfg->coding_rate > 4U)
    {
        return 0;
    }

    if (cfg->preamble_len == 0U)
    {
        return 0;
    }

    return 1;
}

void AppConfig_SetDefault(void)
{
    memset(&g_app_config, 0, sizeof(g_app_config));

    g_app_config.node_id = 1;
    g_app_config.dest_id = 0;
    g_app_config.role = DEVICE_ROLE_TX;

    /*
     * AS923.
     * TX và RX bắt buộc phải cùng frequency.
     */
    g_app_config.frequency = 923000000U;
    g_app_config.tx_power = 14U;

    g_app_config.bandwidth = 0U;          /* 0=125kHz, 1=250kHz, 2=500kHz */
    g_app_config.spreading_factor = 10U;  /* SF7..SF12 */
    g_app_config.coding_rate = 1U;        /* 1=4/5, 2=4/6, 3=4/7, 4=4/8 */

    g_app_config.preamble_len = 8U;
    g_app_config.symbol_timeout = 5U;
}

void AppConfig_Load(void)
{
    FlashConfigRecord_t *rec = (FlashConfigRecord_t *)AppConfig_GetFlashAddr();

    if (rec->magic == APP_CONFIG_MAGIC &&
        rec->version == APP_CONFIG_VERSION &&
        rec->checksum == AppConfig_Checksum(&rec->cfg) &&
        AppConfig_IsValid(&rec->cfg))
    {
        memcpy(&g_app_config, &rec->cfg, sizeof(AppConfig_t));
    }
    else
    {
        AppConfig_SetDefault();
    }
}

HAL_StatusTypeDef AppConfig_Save(void)
{
    FlashConfigRecord_t rec;
    uint32_t flash_addr = AppConfig_GetFlashAddr();
    uint32_t page_error = 0;
    FLASH_EraseInitTypeDef erase = {0};

    uint8_t temp[FLASH_CONFIG_RECORD_ALIGNED_SIZE];

    if (AppConfig_IsValid(&g_app_config) == 0U)
    {
        return HAL_ERROR;
    }

    memset(&rec, 0, sizeof(rec));

    rec.magic = APP_CONFIG_MAGIC;
    rec.version = APP_CONFIG_VERSION;
    rec.cfg = g_app_config;
    rec.checksum = AppConfig_Checksum(&g_app_config);

    memset(temp, 0xFF, sizeof(temp));
    memcpy(temp, &rec, sizeof(rec));

    if (HAL_FLASH_Unlock() != HAL_OK)
    {
        return HAL_ERROR;
    }

    erase.TypeErase = FLASH_TYPEERASE_PAGES;
    erase.Page = (flash_addr - FLASH_BASE) / FLASH_PAGE_SIZE;
    erase.NbPages = 1;

    if (HAL_FLASHEx_Erase(&erase, &page_error) != HAL_OK)
    {
        HAL_FLASH_Lock();
        return HAL_ERROR;
    }

    for (uint32_t i = 0; i < FLASH_CONFIG_RECORD_ALIGNED_SIZE; i += 8U)
    {
        uint64_t data64 = 0xFFFFFFFFFFFFFFFFULL;

        memcpy(&data64, &temp[i], 8U);

        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD,
                              flash_addr + i,
                              data64) != HAL_OK)
        {
            HAL_FLASH_Lock();
            return HAL_ERROR;
        }
    }

    if (HAL_FLASH_Lock() != HAL_OK)
    {
        return HAL_ERROR;
    }

    return HAL_OK;
}

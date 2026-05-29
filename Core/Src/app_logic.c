/*
 * app_logic.c
 *
 *  Created on: May 29, 2026
 *      Author: tranquocvu2
 */


#include "app_logic.h"
#include "app_config.h"
#include "isolated_input.h"
#include "rs485.h"
#include "rf_packet.h"
#include "app_subghz_phy.h"
#include "subghz_phy_app.h"

#include "cmsis_os2.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

extern UART_HandleTypeDef huart2;
extern osSemaphoreId_t radioBinarySemHandle;

static uint8_t seq = 0;
static uint8_t last_input_mask = 0xFF;
static uint32_t last_input_send_tick = 0;

static int16_t rx_rssi;
static int8_t rx_snr;
static uint8_t rf_rx_size;
static uint8_t rf_rx_buffer[128];

static void App_PrintConfig(void);
static void App_ProcessCommand(char *cmd);
static void App_SendInputPacket(uint8_t input_mask);
static void App_SendRs485Packet(char *text);
static void App_CheckRadioRx(void);
static void App_HandleReceivedPacket(RfPacket_t *pkt, int16_t rssi, int8_t snr);

void App_Init(void)
{
    AppConfig_Load();

    Input_Init();
    RS485_Init(&huart2);

    MX_SubGHz_Phy_Init();
    SubghzApp_ApplyConfig(&g_app_config);

    last_input_mask = Input_GetMask();

    RS485_SendString("\r\nSTM32WL55 RS485 + INPUT + RF READY\r\n");
    App_PrintConfig();
}

void App_Process(void)
{
    char line[128];

    if (RS485_GetLine(line, sizeof(line)))
    {
        if (strncmp(line, "AT+", 3) == 0)
        {
            App_ProcessCommand(line);
        }
        else
        {
            if (g_app_config.role == DEVICE_ROLE_TX)
            {
                App_SendRs485Packet(line);
            }
            else
            {
                RS485_SendString("ERR: RX node only receives RF\r\n");
            }
        }
    }

    if (g_app_config.role == DEVICE_ROLE_TX)
    {
        uint8_t now_mask = Input_GetMask();

        if (now_mask != last_input_mask)
        {
            last_input_mask = now_mask;
            App_SendInputPacket(now_mask);
            last_input_send_tick = HAL_GetTick();
        }

        if (HAL_GetTick() - last_input_send_tick > 1000)
        {
            App_SendInputPacket(now_mask);
            last_input_send_tick = HAL_GetTick();
        }
    }

    App_CheckRadioRx();

    osDelay(10);
}

static void App_CheckRadioRx(void)
{
    rf_rx_size = 0;
    memset(rf_rx_buffer, 0, sizeof(rf_rx_buffer));

    MX_SubGhz_Phy_ReceivePacketTimeout(20);

    if (osSemaphoreAcquire(radioBinarySemHandle, 50) == osOK)
    {
        if (MX_SubGhz_Phy_Get_RecvicePacket_State() == 0x01)
        {
            MX_SubGhz_Phy_Get_RecvicePacket(
                &rx_rssi,
                &rx_snr,
                rf_rx_buffer,
                &rf_rx_size
            );

            RfPacket_t *pkt = (RfPacket_t *)rf_rx_buffer;

            if (RFPacket_IsValid(pkt, rf_rx_size) &&
                RFPacket_IsForMe(pkt, g_app_config.node_id))
            {
                App_HandleReceivedPacket(pkt, rx_rssi, rx_snr);
            }
        }
    }
}

static void App_SendInputPacket(uint8_t input_mask)
{
    RfPacket_t pkt;
    memset(&pkt, 0, sizeof(pkt));

    pkt.magic = RF_PACKET_MAGIC;
    pkt.src_id = g_app_config.node_id;
    pkt.dst_id = g_app_config.dest_id;
    pkt.type = RF_PKT_INPUT;
    pkt.seq = seq++;
    pkt.input_mask = input_mask;
    pkt.len = 0;

    MX_SubGhz_Phy_SendPacket((uint8_t *)&pkt, 7);

    if (osSemaphoreAcquire(radioBinarySemHandle, 3000) == osOK)
    {
        if (MX_SubGhz_Phy_Get_SendPacket_State() == 0x01)
        {
            RS485_SendString("RF INPUT SENT\r\n");
        }
        else
        {
            RS485_SendString("RF INPUT SEND FAIL\r\n");
        }
    }
    else
    {
        RS485_SendString("RF INPUT TIMEOUT\r\n");
    }
}

static void App_SendRs485Packet(char *text)
{
    RfPacket_t pkt;
    uint8_t len = strlen(text);

    if (len > RF_PACKET_MAX_DATA)
    {
        len = RF_PACKET_MAX_DATA;
    }

    memset(&pkt, 0, sizeof(pkt));

    pkt.magic = RF_PACKET_MAGIC;
    pkt.src_id = g_app_config.node_id;
    pkt.dst_id = g_app_config.dest_id;
    pkt.type = RF_PKT_RS485;
    pkt.seq = seq++;
    pkt.input_mask = Input_GetMask();
    pkt.len = len;

    memcpy(pkt.data, text, len);

    MX_SubGhz_Phy_SendPacket((uint8_t *)&pkt, 7 + len);

    if (osSemaphoreAcquire(radioBinarySemHandle, 3000) == osOK)
    {
        if (MX_SubGhz_Phy_Get_SendPacket_State() == 0x01)
        {
            RS485_SendString("RF DATA SENT\r\n");
        }
        else
        {
            RS485_SendString("RF DATA SEND FAIL\r\n");
        }
    }
    else
    {
        RS485_SendString("RF DATA TIMEOUT\r\n");
    }
}

static void App_HandleReceivedPacket(RfPacket_t *pkt, int16_t rssi, int8_t snr)
{
    char msg[160];

    if (pkt->type == RF_PKT_INPUT)
    {
        snprintf(msg, sizeof(msg),
                 "RX INPUT FROM ID=%d IN=%d,%d,%d,%d RSSI=%d SNR=%d\r\n",
                 pkt->src_id,
                 (pkt->input_mask & 0x01) ? 1 : 0,
                 (pkt->input_mask & 0x02) ? 1 : 0,
                 (pkt->input_mask & 0x04) ? 1 : 0,
                 (pkt->input_mask & 0x08) ? 1 : 0,
                 rssi,
                 snr);

        RS485_SendString(msg);
    }
    else if (pkt->type == RF_PKT_RS485)
    {
        RS485_SendString("RX DATA FROM RF: ");
        RS485_Send(pkt->data, pkt->len);
        RS485_SendString("\r\n");
    }
}

static void App_ProcessCommand(char *cmd)
{
    char msg[128];

    if (strcmp(cmd, "AT") == 0)
    {
        RS485_SendString("OK\r\n");
    }
    else if (strcmp(cmd, "AT+GETCFG") == 0)
    {
        App_PrintConfig();
    }
    else if (strncmp(cmd, "AT+SETID=", 9) == 0)
    {
        g_app_config.node_id = atoi(&cmd[9]);
        RS485_SendString("OK SET ID\r\n");
    }
    else if (strncmp(cmd, "AT+SETDST=", 10) == 0)
    {
        g_app_config.dest_id = atoi(&cmd[10]);
        RS485_SendString("OK SET DST\r\n");
    }
    else if (strncmp(cmd, "AT+SETROLE=", 11) == 0)
    {
        if (strcmp(&cmd[11], "TX") == 0)
        {
            g_app_config.role = DEVICE_ROLE_TX;
            RS485_SendString("OK SET ROLE TX\r\n");
        }
        else if (strcmp(&cmd[11], "RX") == 0)
        {
            g_app_config.role = DEVICE_ROLE_RX;
            RS485_SendString("OK SET ROLE RX\r\n");
        }
        else
        {
            RS485_SendString("ERR ROLE\r\n");
        }
    }
    else if (strncmp(cmd, "AT+SETFREQ=", 11) == 0)
    {
        g_app_config.frequency = strtoul(&cmd[11], NULL, 10);
        SubghzApp_ApplyConfig(&g_app_config);
        RS485_SendString("OK SET FREQ\r\n");
    }
    else if (strncmp(cmd, "AT+SETBW=", 9) == 0)
    {
        g_app_config.bandwidth = atoi(&cmd[9]);
        SubghzApp_ApplyConfig(&g_app_config);
        RS485_SendString("OK SET BW\r\n");
    }
    else if (strncmp(cmd, "AT+SETSF=", 9) == 0)
    {
        g_app_config.spreading_factor = atoi(&cmd[9]);
        SubghzApp_ApplyConfig(&g_app_config);
        RS485_SendString("OK SET SF\r\n");
    }
    else if (strncmp(cmd, "AT+SETCR=", 9) == 0)
    {
        g_app_config.coding_rate = atoi(&cmd[9]);
        SubghzApp_ApplyConfig(&g_app_config);
        RS485_SendString("OK SET CR\r\n");
    }
    else if (strncmp(cmd, "AT+SETPWR=", 10) == 0)
    {
        g_app_config.tx_power = atoi(&cmd[10]);
        SubghzApp_ApplyConfig(&g_app_config);
        RS485_SendString("OK SET POWER\r\n");
    }
    else if (strcmp(cmd, "AT+SAVE") == 0)
    {
        AppConfig_Save();
        RS485_SendString("OK SAVE\r\n");
    }
    else if (strcmp(cmd, "AT+GETIN") == 0)
    {
        uint8_t mask = Input_GetMask();

        snprintf(msg, sizeof(msg),
                 "IN=%d,%d,%d,%d\r\n",
                 (mask & 0x01) ? 1 : 0,
                 (mask & 0x02) ? 1 : 0,
                 (mask & 0x04) ? 1 : 0,
                 (mask & 0x08) ? 1 : 0);

        RS485_SendString(msg);
    }
    else
    {
        RS485_SendString("ERR CMD\r\n");
    }
}

static void App_PrintConfig(void)
{
    char msg[200];

    snprintf(msg, sizeof(msg),
             "CFG ID=%d DST=%d ROLE=%s FREQ=%lu BW=%d SF=%d CR=%d PWR=%d\r\n",
             g_app_config.node_id,
             g_app_config.dest_id,
             g_app_config.role == DEVICE_ROLE_TX ? "TX" : "RX",
             g_app_config.frequency,
             g_app_config.bandwidth,
             g_app_config.spreading_factor,
             g_app_config.coding_rate,
             g_app_config.tx_power);

    RS485_SendString(msg);
}

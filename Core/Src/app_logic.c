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

#define RF_TX_TIMEOUT_MS        3000U
#define RF_ACK_TIMEOUT_MS       500U
#define RF_RETRY_MAX            3U

#define INPUT_DEBOUNCE_MS       30U
#define INPUT_HEARTBEAT_MS      10000U

static uint8_t seq = 0;
static uint8_t last_input_mask = 0xFF;
static uint32_t last_input_send_tick = 0;

static uint8_t candidate_input_mask = 0xFF;
static uint32_t input_change_tick = 0;

static int16_t rx_rssi;
static int8_t rx_snr;
static uint8_t rf_rx_size;
static uint8_t rf_rx_buffer[128];

/*
 * Dùng để chống xử lý lại packet cũ khi TX retry.
 * Ví dụ:
 * TX gửi seq=10
 * RX nhận được và gửi ACK
 * ACK bị mất
 * TX retry seq=10
 * RX nhận lại seq=10 thì vẫn ACK lại, nhưng không xử lý data lần 2.
 */
static uint8_t last_rx_seq[256];
static uint8_t last_rx_seq_valid[256];

static void App_PrintConfig(void);
static void App_ClearRadioSem(void);
static void App_ProcessCommand(char *cmd);
static void App_SendInputPacket(uint8_t input_mask);
static void App_SendRs485Packet(char *text);
static void App_CheckRadioRx(void);
static void App_HandleReceivedPacket(RfPacket_t *pkt, int16_t rssi, int8_t snr);

static uint8_t App_SendRawPacket(RfPacket_t *pkt, uint8_t size);
static uint8_t App_SendPacketReliable(RfPacket_t *pkt, uint8_t size);
static uint8_t App_WaitAck(uint8_t peer_id, uint8_t pkt_seq);
static void App_SendAck(uint8_t dst_id, uint8_t ack_seq);

static uint8_t App_IsDuplicatePacket(RfPacket_t *pkt);

void App_Init(void)
{
    AppConfig_Load();

    Input_Init();
    RS485_Init(&huart2);

    MX_SubGHz_Phy_Init();
    SubghzApp_ApplyConfig(&g_app_config);

    last_input_mask = Input_GetMask();
    candidate_input_mask = last_input_mask;
    input_change_tick = HAL_GetTick();
    last_input_send_tick = HAL_GetTick();

    memset(last_rx_seq, 0, sizeof(last_rx_seq));
    memset(last_rx_seq_valid, 0, sizeof(last_rx_seq_valid));

    App_ClearRadioSem();

    RS485_SendString("\r\nSTM32WL55 RS485 + INPUT + RF READY\r\n");
    App_PrintConfig();
}

void App_Process(void)
{
    char line[128];

    if (RS485_GetLine(line, sizeof(line)))
    {
        /*
         * Sửa chỗ này:
         * Trước đó chỉ check "AT+", nên lệnh "AT" thường sẽ không chạy.
         */
        if ((strcmp(line, "AT") == 0) || (strncmp(line, "AT+", 3) == 0))
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
        uint32_t now_tick = HAL_GetTick();
        uint8_t now_mask = Input_GetMask();

        /*
         * Debounce input:
         * Nếu input vừa đổi, chưa gửi ngay.
         * Đợi ổn định INPUT_DEBOUNCE_MS rồi mới gửi.
         */
        if (now_mask != candidate_input_mask)
        {
            candidate_input_mask = now_mask;
            input_change_tick = now_tick;
        }

        if ((candidate_input_mask != last_input_mask) &&
            ((now_tick - input_change_tick) >= INPUT_DEBOUNCE_MS))
        {
            last_input_mask = candidate_input_mask;
            App_SendInputPacket(last_input_mask);
            last_input_send_tick = now_tick;
        }

        /*
         * Heartbeat:
         * Không cần gửi lại mỗi 1s.
         * 10s gửi lại một lần để báo node còn sống.
         */
        if ((now_tick - last_input_send_tick) >= INPUT_HEARTBEAT_MS)
        {
            App_SendInputPacket(last_input_mask);
            last_input_send_tick = now_tick;
        }
    }

    App_CheckRadioRx();

    osDelay(10);
}

static void App_ClearRadioSem(void)
{
    while (osSemaphoreAcquire(radioBinarySemHandle, 0U) == osOK)
    {
    }
}

static uint8_t App_IsDuplicatePacket(RfPacket_t *pkt)
{
    if (pkt == NULL)
    {
        return 1U;
    }

    if (last_rx_seq_valid[pkt->src_id] &&
        last_rx_seq[pkt->src_id] == pkt->seq)
    {
        return 1U;
    }

    last_rx_seq_valid[pkt->src_id] = 1U;
    last_rx_seq[pkt->src_id] = pkt->seq;

    return 0U;
}

static void App_CheckRadioRx(void)
{
    rf_rx_size = 0;
    memset(rf_rx_buffer, 0, sizeof(rf_rx_buffer));

    App_ClearRadioSem();

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

    if (App_SendPacketReliable(&pkt, RF_PACKET_HEADER_SIZE))
    {
        RS485_SendString("RF INPUT ACK OK\r\n");
    }
    else
    {
        RS485_SendString("RF INPUT ACK FAIL\r\n");
    }
}

static void App_SendRs485Packet(char *text)
{
    RfPacket_t pkt;
    size_t text_len;
    uint8_t len;

    if (text == NULL)
    {
        return;
    }

    text_len = strlen(text);

    if (text_len > RF_PACKET_MAX_DATA)
    {
        text_len = RF_PACKET_MAX_DATA;
    }

    len = (uint8_t)text_len;

    memset(&pkt, 0, sizeof(pkt));

    pkt.magic = RF_PACKET_MAGIC;
    pkt.src_id = g_app_config.node_id;
    pkt.dst_id = g_app_config.dest_id;
    pkt.type = RF_PKT_RS485;
    pkt.seq = seq++;
    pkt.input_mask = Input_GetMask();
    pkt.len = len;

    memcpy(pkt.data, text, len);

    if (App_SendPacketReliable(&pkt, RF_PACKET_HEADER_SIZE + len))
    {
        RS485_SendString("RF DATA ACK OK\r\n");
    }
    else
    {
        RS485_SendString("RF DATA ACK FAIL\r\n");
    }
}

static uint8_t App_SendRawPacket(RfPacket_t *pkt, uint8_t size)
{
    if (pkt == NULL || size == 0U)
    {
        return 0U;
    }

    App_ClearRadioSem();

    MX_SubGhz_Phy_SendPacket((uint8_t *)pkt, size);

    if (osSemaphoreAcquire(radioBinarySemHandle, RF_TX_TIMEOUT_MS) != osOK)
    {
        return 0U;
    }

    return (MX_SubGhz_Phy_Get_SendPacket_State() == 0x01) ? 1U : 0U;
}

static void App_SendAck(uint8_t dst_id, uint8_t ack_seq)
{
    RfPacket_t ack;

    memset(&ack, 0, sizeof(ack));

    ack.magic = RF_PACKET_MAGIC;
    ack.src_id = g_app_config.node_id;
    ack.dst_id = dst_id;
    ack.type = RF_PKT_ACK;
    ack.seq = ack_seq;
    ack.input_mask = Input_GetMask();
    ack.len = 0;

    App_SendRawPacket(&ack, RF_PACKET_HEADER_SIZE);
}

static uint8_t App_WaitAck(uint8_t peer_id, uint8_t pkt_seq)
{
    uint32_t start_tick = HAL_GetTick();

    while ((HAL_GetTick() - start_tick) < RF_ACK_TIMEOUT_MS)
    {
        rf_rx_size = 0;
        memset(rf_rx_buffer, 0, sizeof(rf_rx_buffer));

        App_ClearRadioSem();

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
                    if (pkt->type == RF_PKT_ACK &&
                        pkt->src_id == peer_id &&
                        pkt->seq == pkt_seq)
                    {
                        return 1U;
                    }

                    /*
                     * Nếu trong lúc chờ ACK mà nhận packet khác,
                     * vẫn xử lý để không bị mất dữ liệu.
                     */
                    App_HandleReceivedPacket(pkt, rx_rssi, rx_snr);
                }
            }
        }
    }

    return 0U;
}

static uint8_t App_SendPacketReliable(RfPacket_t *pkt, uint8_t size)
{
    if (pkt == NULL || size == 0U)
    {
        return 0U;
    }

    for (uint8_t retry = 0; retry < RF_RETRY_MAX; retry++)
    {
        if (App_SendRawPacket(pkt, size) == 0U)
        {
            osDelay(50);
            continue;
        }

        /*
         * Broadcast không nên bắt ACK,
         * vì nhiều node ACK cùng lúc sẽ đụng nhau.
         */
        if (pkt->dst_id == RF_BROADCAST_ID || pkt->type == RF_PKT_ACK)
        {
            return 1U;
        }

        if (App_WaitAck(pkt->dst_id, pkt->seq))
        {
            return 1U;
        }

        /*
         * Backoff nhẹ để tránh hai node retry cùng lúc.
         */
        osDelay(50 + (HAL_GetTick() & 0x3F));
    }

    return 0U;
}

static void App_HandleReceivedPacket(RfPacket_t *pkt, int16_t rssi, int8_t snr)
{
    char msg[160];

    if (pkt == NULL)
    {
        return;
    }

    /*
     * Nếu nhận ACK thì không xử lý như data thường.
     */
    if (pkt->type == RF_PKT_ACK)
    {
        return;
    }

    /*
     * Gửi ACK lại cho packet unicast.
     * Không ACK broadcast để tránh nhiều node trả lời cùng lúc.
     *
     * Quan trọng:
     * ACK phải gửi trước khi check duplicate.
     * Vì nếu ACK cũ bị mất, TX sẽ retry packet cũ.
     * RX phải ACK lại để TX dừng retry, nhưng không xử lý data lần 2.
     */
    if (pkt->dst_id != RF_BROADCAST_ID)
    {
        App_SendAck(pkt->src_id, pkt->seq);
    }

    /*
     * Chống xử lý trùng packet khi TX retry.
     */
    if (App_IsDuplicatePacket(pkt))
    {
        return;
    }

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
    char msg[160];

    if (cmd == NULL)
    {
        return;
    }

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
        uint32_t id = strtoul(&cmd[9], NULL, 10);

        if (id == 0U || id >= RF_BROADCAST_ID)
        {
            RS485_SendString("ERR ID\r\n");
        }
        else
        {
            g_app_config.node_id = (uint8_t)id;
            RS485_SendString("OK SET ID\r\n");
        }
    }
    else if (strncmp(cmd, "AT+SETDST=", 10) == 0)
    {
        uint32_t id = strtoul(&cmd[10], NULL, 10);

        if (id > RF_BROADCAST_ID)
        {
            RS485_SendString("ERR DST\r\n");
        }
        else
        {
            g_app_config.dest_id = (uint8_t)id;
            RS485_SendString("OK SET DST\r\n");
        }
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
        uint32_t freq = strtoul(&cmd[11], NULL, 10);

        /*
         * Đang check theo vùng 860~930MHz.
         * Nếu dùng 433MHz thì sửa điều kiện này.
         */
        if (freq < 860000000U || freq > 930000000U)
        {
            RS485_SendString("ERR FREQ\r\n");
        }
        else
        {
            g_app_config.frequency = freq;
            SubghzApp_ApplyConfig(&g_app_config);
            RS485_SendString("OK SET FREQ\r\n");
        }
    }
    else if (strncmp(cmd, "AT+SETBW=", 9) == 0)
    {
        uint32_t bw = strtoul(&cmd[9], NULL, 10);

        if (bw > 2U)
        {
            RS485_SendString("ERR BW\r\n");
        }
        else
        {
            g_app_config.bandwidth = (uint8_t)bw;
            SubghzApp_ApplyConfig(&g_app_config);
            RS485_SendString("OK SET BW\r\n");
        }
    }
    else if (strncmp(cmd, "AT+SETSF=", 9) == 0)
    {
        uint32_t sf = strtoul(&cmd[9], NULL, 10);

        if (sf < 7U || sf > 12U)
        {
            RS485_SendString("ERR SF\r\n");
        }
        else
        {
            g_app_config.spreading_factor = (uint8_t)sf;
            SubghzApp_ApplyConfig(&g_app_config);
            RS485_SendString("OK SET SF\r\n");
        }
    }
    else if (strncmp(cmd, "AT+SETCR=", 9) == 0)
    {
        uint32_t cr = strtoul(&cmd[9], NULL, 10);

        if (cr < 1U || cr > 4U)
        {
            RS485_SendString("ERR CR\r\n");
        }
        else
        {
            g_app_config.coding_rate = (uint8_t)cr;
            SubghzApp_ApplyConfig(&g_app_config);
            RS485_SendString("OK SET CR\r\n");
        }
    }
    else if (strncmp(cmd, "AT+SETPWR=", 10) == 0)
    {
        uint32_t pwr = strtoul(&cmd[10], NULL, 10);

        if (pwr > 22U)
        {
            RS485_SendString("ERR POWER\r\n");
        }
        else
        {
            g_app_config.tx_power = (int8_t)pwr;
            SubghzApp_ApplyConfig(&g_app_config);
            RS485_SendString("OK SET POWER\r\n");
        }
    }
    else if (strcmp(cmd, "AT+SAVE") == 0)
    {
        /*
         * Chỗ này yêu cầu AppConfig_Save() trả về HAL_StatusTypeDef.
         * Trong app_config.h cần khai báo:
         * HAL_StatusTypeDef AppConfig_Save(void);
         */
        if (AppConfig_Save() == HAL_OK)
        {
            RS485_SendString("OK SAVE\r\n");
        }
        else
        {
            RS485_SendString("ERR SAVE\r\n");
        }
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
             (unsigned long)g_app_config.frequency,
             g_app_config.bandwidth,
             g_app_config.spreading_factor,
             g_app_config.coding_rate,
             g_app_config.tx_power);

    RS485_SendString(msg);
}

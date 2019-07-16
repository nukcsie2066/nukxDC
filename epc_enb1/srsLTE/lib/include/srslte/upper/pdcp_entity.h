/**
 *
 * \section COPYRIGHT
 *
 * Copyright 2013-2015 Software Radio Systems Limited
 *
 * \section LICENSE
 *
 * This file is part of the srsUE library.
 *
 * srsUE is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * srsUE is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * A copy of the GNU Affero General Public License can be found in
 * the LICENSE file in the top-level directory of this distribution
 * and at http://www.gnu.org/licenses/.
 *
 */

#ifndef SRSLTE_PDCP_ENTITY_H
#define SRSLTE_PDCP_ENTITY_H

#include "srslte/common/buffer_pool.h"
#include "srslte/common/log.h"
#include "srslte/common/common.h"
#include "srslte/interfaces/ue_interfaces.h"
#include "srslte/common/security.h"
#include "srslte/common/threads.h"
#include "srslte/upper/pdcp_metrics.h"

#include <linux/if.h>
#include <linux/if_packet.h>
#include <netinet/ether.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

namespace srslte {

/****************************************************************************
 * Structs and Defines
 * Ref: 3GPP TS 36.323 v10.1.0
 ***************************************************************************/

#define PDCP_CONTROL_MAC_I 0x00000000

#define PDCP_PDU_TYPE_PDCP_STATUS_REPORT                0x0
#define PDCP_PDU_TYPE_INTERSPERSED_ROHC_FEEDBACK_PACKET 0x1

#define ETH_IF	"eth0"
#define UE_ETH_MAC0	0xf4
#define UE_ETH_MAC1	0xf2
#define UE_ETH_MAC2	0x6d
#define UE_ETH_MAC3	0x18
#define UE_ETH_MAC4	0xc9
#define UE_ETH_MAC5	0x54

#define WIFI_IF	"eth0"
#define UE_WIFI_MAC0	0xf4
#define UE_WIFI_MAC1	0xf2
#define UE_WIFI_MAC2	0x6d
#define UE_WIFI_MAC3	0x18
#define UE_WIFI_MAC4	0xc9
#define UE_WIFI_MAC5	0x54

#define ETH_TYPE_ETH	0x9e66
#define ETH_TYPE_WIFI	0x9e65

typedef enum{
	ROUTE_LTE = 0,
	ROUTE_WLAN,
	ROUTE_ETH,
}route_t;

typedef enum{
    PDCP_D_C_CONTROL_PDU = 0,
    PDCP_D_C_DATA_PDU,
    PDCP_D_C_N_ITEMS,
}pdcp_d_c_t;
static const char pdcp_d_c_text[PDCP_D_C_N_ITEMS][20] = {"Control PDU",
                                                         "Data PDU"};

/****************************************************************************
 * PDCP Entity interface
 * Common interface for all PDCP entities
 ***************************************************************************/
class pdcp_entity
{
public:
  pdcp_entity();
  void init(srsue::rlc_interface_pdcp     *rlc_,
			srsue::lwaap_interface_pdcp   *lwaap_,
            srsue::rrc_interface_pdcp     *rrc_,
            srsue::gw_interface_pdcp      *gw_,
            srslte::log                   *log_,
            uint32_t                       lcid_,
            srslte_pdcp_config_t           cfg_);
  void reset();
  void reestablish();

  bool is_active();

  // Metric interface
  void get_metrics(pdcp_metrics_t &m);
  
  // RRC interface
  void write_sdu(byte_buffer_t *sdu);
  void config_security(uint8_t *k_enc_,
                       uint8_t *k_int_,
                       CIPHERING_ALGORITHM_ID_ENUM cipher_algo_,
                       INTEGRITY_ALGORITHM_ID_ENUM integ_algo_);
  void enable_integrity();
  void enable_encryption();

  // RLC & LWAAP interface
  void write_pdu(byte_buffer_t *pdu);
  
  //
  void set_lwa_ratio(uint32_t lr, uint32_t wr);
  void set_ema_ratio(uint32_t part, uint32_t whole);
  void set_report_period(uint32_t period);
  void toggle_timestamp(bool b);
  void toggle_autoconfig(bool b);
  void toggle_random(bool b);

private:
  byte_buffer_pool        *pool;
  srslte::log             *log;

  srsue::rlc_interface_pdcp 	*rlc;
  srsue::lwaap_interface_pdcp 	*lwaap;
  srsue::rrc_interface_pdcp 	*rrc;
  srsue::gw_interface_pdcp  	*gw;

  uint32_t              SN_MOD;
  const static uint32_t SHORT_SN_MOD = (1 << 7);
  const static uint32_t LONG_SN_MOD  = (1 << 12);
  
  bool                active;
  uint32_t            lcid;
  srslte_pdcp_config_t cfg;
  uint8_t             sn_len_bytes;
  bool                do_integrity;
  bool                do_encryption;
  bool                do_lwa;
  bool                do_timestamp;
  bool                do_autoconfig;
  bool                do_packet_inspection;
  bool                do_random_route;
  bool                do_ema;
  bool                do_dc;
  
  const static float  MAX_RATIO = 256;
  const static float  rate_init_lte  =  60000000; //60Mbps
  const static float  rate_init_wlan = 500000000; //500Mbps
  uint32_t            last_hrw;
  uint32_t            t_report;
  
  struct timeval      metrics_time[3];
  struct timespec     timestamp_time[3];
  struct timespec     report_time[3];
  uint32_t            rx_count;
  uint32_t            rx_sdus;
  uint32_t            rx_bytes;
  uint32_t            tx_count;
  uint32_t            tx_pdus;
  uint32_t            tx_bytes;
  uint32_t            lte_tx_bytes;
  uint32_t            lte_tx_count;
  uint32_t            wifi_tx_bytes;
  uint32_t            wifi_tx_count;
  uint32_t            reconfigure_sn;
  uint32_t            lte_ratio;
  uint32_t            wifi_ratio;
  uint32_t            total_ratio;
  uint32_t            lte_ratio_count;
  uint32_t            wifi_ratio_count;
  uint32_t            base_ratio;
  uint32_t            alpha_part;
  uint32_t            alpha_whole;
  uint32_t            ema_part;
  uint32_t            ema_whole;
  uint8_t             k_enc[32];
  uint8_t             k_int[32];
  
  std::map<uint32_t, route_t> tx_route;
  std::map<uint16_t, route_t> tx_stream_route;
  
  struct ifreq eth_idx;
  struct ifreq eth_mac;
  int eth_sock;
  
  void leaap_init();
  void leaap_write_sdu(uint32_t lcid, byte_buffer_t *sdu);
  bool leaap_write_header(ether_header *header, uint32_t lcid, byte_buffer_t *sdu);
  
  bool lwa_report_is_set(byte_buffer_t *pdu);
  void handle_lwa_report(byte_buffer_t *pdu);

  bool timestamp_is_set(byte_buffer_t *pdu);
  
  void debug_state();
  
  route_t get_route();

  CIPHERING_ALGORITHM_ID_ENUM cipher_algo;
  INTEGRITY_ALGORITHM_ID_ENUM integ_algo;

  void integrity_generate(uint8_t  *msg,
                          uint32_t  msg_len,
                          uint8_t  *mac);

  bool integrity_verify(uint8_t  *msg,
                        uint32_t  count,
                        uint32_t  msg_len,
                        uint8_t  *mac);

  void cipher_encrypt(uint8_t  *msg,
                      uint32_t  msg_len,
                      uint8_t  *ct);

  void cipher_decrypt(uint8_t  *ct,
                      uint32_t  count,
                      uint32_t  ct_len,
                      uint8_t  *msg);

  uint8_t  get_bearer_id(uint8_t lcid);
  
  uint8_t  get_gcd(uint32_t x, uint32_t y);
};

/****************************************************************************
 * Pack/Unpack helper functions
 * Ref: 3GPP TS 36.323 v10.1.0
 ***************************************************************************/

void pdcp_pack_control_pdu(uint32_t sn, byte_buffer_t *sdu);
void pdcp_unpack_control_pdu(byte_buffer_t *sdu, uint32_t *sn);

void pdcp_pack_data_pdu_short_sn(uint32_t sn, byte_buffer_t *sdu);
void pdcp_unpack_data_pdu_short_sn(byte_buffer_t *sdu, uint32_t *sn);
void pdcp_pack_data_pdu_long_sn(uint32_t sn, byte_buffer_t *sdu);
void pdcp_unpack_data_pdu_long_sn(byte_buffer_t *sdu, uint32_t *sn);

void pdcp_pack_data_pdu_long_sn_timestamp(uint32_t sn, uint8_t timestamp, byte_buffer_t *sdu);
void pdcp_unpack_data_pdu_long_sn_timestamp(byte_buffer_t *sdu, uint32_t *sn, uint8_t *timestamp);

} // namespace srslte


#endif // SRSLTE_PDCP_ENTITY_H

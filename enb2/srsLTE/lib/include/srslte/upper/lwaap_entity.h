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

#ifndef LWAAP_ENTITY_H
#define LWAAP_ENTITY_H

#include "srslte/common/buffer_pool.h"
#include "srslte/common/log.h"
#include "srslte/common/common.h"
#include "srslte/interfaces/ue_interfaces.h"
#include "srslte/common/threads.h"
#include "srslte/upper/lwaap_metrics.h"

#include <sys/ioctl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <linux/if_packet.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <linux/if.h>
#include <netinet/ether.h>

namespace srslte {

/****************************************************************************
 * Structs and Defines
 * Ref: 3GPP TS 36.360 v14.0.0
 ***************************************************************************/

#define WLAN_IF	"enxf4f26d1dc389"
#define UE_MAC0	0x98
#define UE_MAC1	0xde
#define UE_MAC2	0xd0
#define UE_MAC3	0x13
#define UE_MAC4	0x9b
#define UE_MAC5	0x1d

#define ETH_TYPE_WIFI	 0x9e65
#define LWAAP_HEADER_LEN 1

/****************************************************************************
 * LWAAP Entity interface
 * Common interface for all LWAAP entities
 ***************************************************************************/
class lwaap_entity : public thread
{
public:
  lwaap_entity();
  void init(srsue::pdcp_interface_lwaap   *pdcp_,
			srsue::gw_interface_lwaap     *gw_,
            srsue::rrc_interface_lwaap    *rrc_,
            srslte::log                   *log_,
            uint32_t                       lcid_,
            srslte_lwaap_config_t           cfg_);
  void reset();

  bool is_active();

  // Metric interface
  void get_metrics(lwaap_metrics_t &m);
  
  // RRC interface
  void enable_reordering();
  
  // PDCP interface
  void write_sdu(byte_buffer_t *sdu);

private:

  static const int LWAAP_THREAD_PRIO = 7;

  byte_buffer_pool        *pool;
  srslte::log             *log;

  srsue::pdcp_interface_lwaap 	*pdcp;
  srsue::gw_interface_lwaap     *gw;
  srsue::rrc_interface_lwaap 	*rrc;

  bool                active;
  bool                running;
  uint32_t            lcid;
  srslte_lwaap_config_t cfg;

  uint32_t            rx_count;
  uint32_t            tx_count;
  
  long                ul_tput_bytes;
  long                dl_tput_bytes;
  struct timeval      metrics_time[3];
  
  struct ifreq ifr;
  struct ifreq if_idx;
  struct ifreq if_mac;
  int sockfd;
  int sockopt;
  
  void run_thread();
  
  void lwaap_write_sdu(uint32_t lcid, byte_buffer_t *sdu);
  bool lwaap_write_header(ether_header *header, uint32_t lcid, byte_buffer_t *sdu);
  bool lwaap_read_header(byte_buffer_t *pdu);

  uint8_t  get_bearer_id(uint8_t lcid);
};

} // namespace srslte


#endif // LWAAP_ENTITY_H

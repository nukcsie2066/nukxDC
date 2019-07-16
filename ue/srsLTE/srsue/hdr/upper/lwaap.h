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

#ifndef LWAAP_H
#define LWAAP_H

#include "srslte/common/buffer_pool.h"
#include "srslte/common/log.h"
#include "srslte/common/common.h"
#include "srslte/common/msg_queue.h"
#include "srslte/common/interfaces_common.h"
#include "srslte/interfaces/ue_interfaces.h"
#include "srslte/common/threads.h"
#include "lwaap_metrics.h"

#include <linux/if.h>
#include <arpa/inet.h>
#include <linux/if_packet.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/ether.h>

#define WIFI_IF		"eth2"
#define ENB_MAC0	0xf4
#define ENB_MAC1	0xf2
#define ENB_MAC2	0x6d
#define ENB_MAC3	0x1d
#define ENB_MAC4	0xc3
#define ENB_MAC5	0x89

#define ETH_TYPE_WIFI	 0x9e65
#define LWAAP_HEADER_LEN 1

namespace srsue {

class lwaap
    :public lwaap_interface_pdcp
    ,public thread
{
public:
  lwaap();
  void init(srsue::pdcp_interface_lwaap *pdcp_, srslte::log *lwaap_log_, srslte::srslte_lwaap_config_t);
  void stop();
  
  void get_metrics(lwaap_metrics_t &m);

  // PDCP interface
  void write_sdu(uint32_t lcid, srslte::byte_buffer_t *sdu);
  
private:

  static const int LWAAP_THREAD_PRIO = 7;

  pdcp_interface_lwaap  *pdcp;

  srslte::byte_buffer_pool   *pool;

  srslte::log                *lwaap_log;
  srslte::srslte_lwaap_config_t cfg;
  bool                running;
  bool                run_enable;
  struct ifreq        ifr;
  struct ifreq        if_idx;
  struct ifreq        if_mac;
  int32               sockfd;
  int32               sockopt;

  long                ul_tput_bytes;
  long                dl_tput_bytes;
  struct timeval      metrics_time[3];

  void                run_thread();
  
  bool lwaap_write_header(ether_header *header, uint32_t lcid, srslte::byte_buffer_t *sdu);
  bool lwaap_read_header(srslte::byte_buffer_t *pdu, uint8_t *drbid);
};

} // namespace srsue

#endif // LWAAP_H

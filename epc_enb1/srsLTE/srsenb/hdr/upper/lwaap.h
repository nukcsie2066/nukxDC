/**
 *
 * \section COPYRIGHT
 *
 * Copyright 2013-2017 Software Radio Systems Limited
 *
 * \section LICENSE
 *
 * This file is part of srsLTE.
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

#include <map>
#include "srslte/interfaces/ue_interfaces.h"
#include "srslte/interfaces/enb_interfaces.h"
#include "srslte/upper/lwaap.h"
#include "srslte/upper/lwaap_metrics.h"
#include "common_enb.h"

#ifndef LWAAP_ENB_H
#define LWAAP_ENB_H

namespace srsenb {
  
class lwaap : public lwaap_interface_pdcp,
              public lwaap_interface_gtpu,
              public lwaap_interface_rrc
{
public:
 
  void init(pdcp_interface_lwaap *pdcp_, gtpu_interface_lwaap *gtpu_, rrc_interface_lwaap *rrc_, srslte::log *lwaap_log_);
  void stop(); 
  
  // Metric interface
  void get_metrics(srslte::lwaap_metrics_t metrics[ENB_METRICS_MAX_USERS]);
  
  // lwaap_interface_pdcp / lwaap_interface_gtpu
  void write_sdu(uint16_t rnti, uint32_t lcid, srslte::byte_buffer_t *sdu);
  
  // lwaap_interface_rrc
  void reset(uint16_t rnti);
  void add_user(uint16_t rnti); 
  void rem_user(uint16_t rnti);
  
private: 
  
  class user_interface : public srsue::pdcp_interface_lwaap, 
	                     public srsue::gw_interface_lwaap,
		                 public srsue::rrc_interface_lwaap, 
		                 public srsue::ue_interface
  {
  public: 
	void write_pdu(uint32_t lcid, srslte::byte_buffer_t *sdu);
	uint16_t rnti; 

	srsenb::pdcp_interface_lwaap *pdcp; 
	srsenb::gtpu_interface_lwaap *gtpu;
	srsenb::rrc_interface_lwaap  *rrc;
	srslte::lwaap                *lwaap; 
  }; 
  
  std::map<uint32_t,user_interface> users; 
  
  pdcp_interface_lwaap 	*pdcp;
  gtpu_interface_lwaap  *gtpu;
  rrc_interface_lwaap  	*rrc;
  srslte::log         	*log_h;
  srslte::byte_buffer_pool *pool;
};

}

#endif // LWAAP_ENB_H

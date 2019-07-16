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

#include "srsenb/hdr/upper/lwaap.h"

namespace srsenb {
  
void lwaap::init(pdcp_interface_lwaap* pdcp_, gtpu_interface_lwaap* gtpu_, rrc_interface_lwaap* rrc_, srslte::log* lwaap_log_)
{
  pdcp  = pdcp_; 
  gtpu  = gtpu_;
  rrc   = rrc_; 
  log_h = lwaap_log_;
  
  pool = srslte::byte_buffer_pool::get_instance();
}

void lwaap::stop()
{
  for(std::map<uint32_t, user_interface>::iterator iter=users.begin(); iter!=users.end(); ++iter) {
    rem_user((uint32_t) iter->first);
  }
  users.clear();
}

void lwaap::get_metrics(srslte::lwaap_metrics_t metrics[ENB_METRICS_MAX_USERS])
{
  int i = 0;
  for(std::map<uint32_t, user_interface>::iterator iter=users.begin(); iter!=users.end(); ++iter, ++i) {
    users[iter->first].lwaap->get_metrics(metrics[i]);
  }
}

void lwaap::add_user(uint16_t rnti)
{
  if (users.count(rnti) == 0) {
    srslte::lwaap *obj = new srslte::lwaap;     
    obj->init(&users[rnti], &users[rnti], &users[rnti], log_h, 3/*DRB1*/);
    users[rnti].rnti  = rnti; 
    users[rnti].pdcp  = pdcp; 
	users[rnti].gtpu  = gtpu;
    users[rnti].rrc   = rrc; 
    users[rnti].lwaap = obj;
	log_h->console("LWAAP add user rnti=0x%x\n", rnti);
  }
}

void lwaap::rem_user(uint16_t rnti)
{
  if (users.count(rnti)) {
	users[rnti].lwaap->stop();
	delete users[rnti].lwaap; 
	users[rnti].lwaap = NULL;
	users.erase(rnti);
  }
}

void lwaap::reset(uint16_t rnti)
{
  if (users.count(rnti)) {
    users[rnti].lwaap->reset();
  }
}

void lwaap::write_sdu(uint16_t rnti, uint32_t lcid, srslte::byte_buffer_t* sdu)
{
  if (users.count(rnti) == 0) {
    log_h->console("Not exist user rnti=0x%x, forced to add user\n", rnti);
    add_user(rnti);
  }
	
  if (users.count(rnti)) {
    users[rnti].lwaap->write_sdu(lcid, sdu);
  } else {
    pool->deallocate(sdu);
  }
}

void lwaap::user_interface::write_pdu(uint32_t lcid, srslte::byte_buffer_t* sdu)
{
  pdcp->write_pdu(rnti, lcid, sdu);
}

void lwaap::user_interface::write_sdu(uint32_t lcid, srslte::byte_buffer_t* sdu)
{
  gtpu->write_sdu(rnti, lcid, sdu);
}

}

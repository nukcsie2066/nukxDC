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


#include "srslte/upper/lwaap.h"

namespace srslte {

lwaap::lwaap()
{
  pdcp = NULL;
  rrc = NULL;
  lwaap_log = NULL;
  lcid = 0;
}

void lwaap::init(srsue::pdcp_interface_lwaap *pdcp_, srsue::gw_interface_lwaap *gw_, srsue::rrc_interface_lwaap *rrc_, log *lwaap_log_, uint32_t lcid_)
{
  pdcp      = pdcp_;
  gw        = gw_;
  rrc       = rrc_;
  lwaap_log = lwaap_log_;
  lcid      = lcid_;

  // Default config
  srslte_lwaap_config_t cnfg;

  lwaap_array[lcid].init(pdcp, gw, rrc, lwaap_log, lcid, cnfg);
}

// Metric interface
void lwaap::get_metrics(lwaap_metrics_t &m)
{
  lwaap_array[lcid].get_metrics(m);
}

void lwaap::stop()
{
}

void lwaap::reset()
{
}

/*******************************************************************************
  PDCP interface
*******************************************************************************/
void lwaap::write_sdu(uint32_t lcid, byte_buffer_t *sdu)
{
  //if(valid_lcid(lcid))
    lwaap_array[lcid].write_sdu(sdu);
}

/*******************************************************************************
  Helpers
*******************************************************************************/
bool lwaap::valid_lcid(uint32_t lcid)
{
  if(lcid >= SRSLTE_N_RADIO_BEARERS) {
    lwaap_log->error("Radio bearer id must be in [0:%d] - %d", SRSLTE_N_RADIO_BEARERS, lcid);
    return false;
  }
  if(!lwaap_array[lcid].is_active()) {
    lwaap_log->error("LWAAP entity for logical channel %d has not been activated\n", lcid);
    return false;
  }
  return true;
}

} // namespace srsue

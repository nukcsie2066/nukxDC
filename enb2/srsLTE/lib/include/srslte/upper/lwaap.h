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

#include "srslte/common/log.h"
#include "srslte/common/common.h"
#include "srslte/interfaces/ue_interfaces.h"
#include "srslte/upper/lwaap_entity.h"
#include "srslte/upper/lwaap_metrics.h"

namespace srslte {

class lwaap
    :public srsue::lwaap_interface_pdcp
    ,public srsue::lwaap_interface_gw
    ,public srsue::lwaap_interface_rrc
{
public:
  lwaap();
  virtual ~lwaap(){}
  void init(srsue::pdcp_interface_lwaap *pdcp_,
			srsue::gw_interface_lwaap *gw_,
            srsue::rrc_interface_lwaap *rrc_,
            log *lwaap_log_,
            uint32_t lcid_);
  void stop();

  // Metric interface
  void get_metrics(lwaap_metrics_t &m);
  
  // RRC interface
  void reset();

  // PDCP interface
  void write_sdu(uint32_t lcid, byte_buffer_t *sdu);

private:
  srsue::pdcp_interface_lwaap 	*pdcp;
  srsue::gw_interface_lwaap  	*gw;
  srsue::rrc_interface_lwaap 	*rrc;

  log                       *lwaap_log;
  lwaap_entity               lwaap_array[SRSLTE_N_RADIO_BEARERS];
  uint32_t                   lcid; // default LCID that is maintained active by PDCP instance

  bool valid_lcid(uint32_t lcid);
};

} // namespace srsue


#endif // LWAAP_H

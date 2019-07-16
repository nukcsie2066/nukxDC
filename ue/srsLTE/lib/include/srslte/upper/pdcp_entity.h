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

#include <map>
#include <pthread.h>

#include "srslte/common/buffer_pool.h"
#include "srslte/common/log.h"
#include "srslte/common/common.h"
#include "srslte/common/timers.h"
#include "srslte/common/tti_sync_cv.h"
#include "srslte/common/threads.h"
#include "srslte/interfaces/ue_interfaces.h"
#include "srslte/common/security.h"
#include "pdcp_metrics.h"
#include "srslte/common/threads.h"

namespace srslte {

/****************************************************************************
 * Structs and Defines
 * Ref: 3GPP TS 36.323 v10.1.0
 ***************************************************************************/

#define PDCP_CONTROL_MAC_I 0x00000000

#define PDCP_PDU_TYPE_PDCP_STATUS_REPORT                0x0
#define PDCP_PDU_TYPE_INTERSPERSED_ROHC_FEEDBACK_PACKET 0x1

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
class pdcp_entity : public srslte::timer_callback
{
public:
  pdcp_entity();
  void init(srsue::rlc_interface_pdcp     *rlc_,
			srsue::lwaap_interface_pdcp   *lwaap_,
            srsue::rrc_interface_pdcp     *rrc_,
            srsue::gw_interface_pdcp      *gw_,
            mac_interface_timers          *mac_timers_,
            srslte::log                   *log_,
            uint32_t                       lcid_,
            srslte_pdcp_config_t           cfg_);
  void reset();
  void reestablish();

  bool is_active();

  // RRC interface
  void write_sdu(byte_buffer_t *sdu);
  void config_security(uint8_t *k_enc_,
                       uint8_t *k_int_,
                       CIPHERING_ALGORITHM_ID_ENUM cipher_algo_,
                       INTEGRITY_ALGORITHM_ID_ENUM integ_algo_);
  void enable_integrity();
  void enable_encryption();

  // RLC interface
  void write_pdu(byte_buffer_t *pdu);
  
  // Timer callback interface
  void timer_expired(uint32_t timer_id);
  
  void generate_report();
  
  void get_metrics(pdcp_metrics_t &m);
  
  // Control interface
  void set_reorder_timeout(uint32_t timeout);
  void set_reorder_window(uint32_t window);
  void set_report_period(uint32_t period);
  void toggle_reorder(bool enable);
  void toggle_report(bool enable);
  void toggle_lwa(bool enable);
  void toggle_elwa(bool enable);
  void toggle_discard(bool enable);
  void toggle_duplicate(bool enable);

private:
  byte_buffer_pool        *pool;
  srslte::log             *log;

  srsue::rlc_interface_pdcp   *rlc;
  srsue::lwaap_interface_pdcp *lwaap;
  srsue::rrc_interface_pdcp   *rrc;
  srsue::gw_interface_pdcp    *gw;
  mac_interface_timers        *mac_timers;

  bool                active;
  uint32_t            lcid;
  srslte_pdcp_config_t cfg;
  uint8_t             sn_len_bytes;
  bool                do_integrity;
  bool                do_encryption;
  
  bool                do_lwa;
  bool                do_elwa;
  bool                do_report;
  bool                do_reordering;
  bool                do_discard;
  bool                do_duplicate;
  
  bool                cfg_lwa;
  bool                cfg_elwa;
  bool                cfg_report;
  bool                cfg_reordering;
  bool                cfg_discard;
  bool                cfg_duplicate;
  int32_t             cfg_t_report;
  int32_t             cfg_t_reordering;
  
  typedef struct {
    uint8_t  tx_diff;
	long     rx_diff;
	timespec rx_time;
  } diff_time_t;
  
  timespec            timestamp_time[3];
  std::map<uint32_t, diff_time_t> diff_time;
  
  void get_diff_time(timespec *beginning, timespec *end, timespec *diff);
  
  srslte::timers::timer *reordering_timer;
  uint32_t               reordering_timer_id;
  int32_t                t_reordering;
  
  srslte::timers::timer *report_timer;
  uint32_t               report_timer_id;
  int32_t                t_report;
  uint32_t               fms; //First missing SN
  /* Class to reordering timer with normal priority */
/*  class timer_thread : public thread {
  public:
    timer_thread(srslte::timers::timer *t) : timer(t),running(false) {start();}
    void stop();
  private:
    void run_thread();
    srslte::timers::timer *timer;
    bool running; 
  };
  timer_thread   timer_thread;
*/
  
  uint32_t            hfn;
  uint32_t            submitted_sn;
  uint32_t            next_sn;
  uint32_t            reordering_sn;
  uint32_t            reordering_cnt;
  uint32_t            delayed_cnt;
  uint32_t            expired_cnt;
  uint32_t            duplicate_cnt;
  uint32_t            outoforder_cnt;
  uint32_t            report_cnt;
  
  uint32_t              RX_MOD;
  const static uint32_t SHORT_SN_MOD = (1 << 7);
  const static uint32_t LONG_SN_MOD  = (1 << 12);
  
  uint32_t              REORDERING_WINDOW_SIZE;
  const static uint32_t REORDERING_WINDOW_SHORT_SN = (1 << (7 - 1));
  const static uint32_t REORDERING_WINDOW_LONG_SN  = (1 << (12 - 1));
  std::map<uint32_t, byte_buffer_t *> rx_window;
  
  // Mutexes
  pthread_mutex_t       mutex;
  
  uint32_t            rx_count;
  uint32_t            tx_count;
  uint8_t             k_enc[32];
  uint8_t             k_int[32];

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
  
  bool inside_reordering_window(uint32_t sn);
  bool timestamp_is_set(byte_buffer_t *pdu);
  void generate_ack(uint32_t sn);
  void debug_state();
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

void pdcp_pack_data_pdu_long_sn_ack(uint32_t sn, byte_buffer_t *sdu);
void pdcp_unpack_data_pdu_long_sn_ack(byte_buffer_t *sdu, uint32_t *sn);

} // namespace srslte


#endif // SRSLTE_PDCP_ENTITY_H

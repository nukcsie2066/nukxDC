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


#include "srslte/upper/pdcp_entity.h"
#include "srslte/common/security.h"

#define RX_MOD_BASE(x) (x-next_sn-REORDERING_WINDOW_SIZE)%RX_MOD

namespace srslte {

pdcp_entity::pdcp_entity()
  :active(false)
  ,tx_count(0)
/*,timer_thread(&reordering_timer)*/
{
  pool = byte_buffer_pool::get_instance();
  log = NULL;
  rlc = NULL;
  lwaap = NULL;
  rrc = NULL;
  gw = NULL;
  reordering_timer = NULL;
  lcid = 0;
  reordering_timer_id = 0;
  sn_len_bytes   = 0;
  do_integrity   = false;
  do_encryption  = false;
  cfg_lwa        = true;
  cfg_elwa       = true;
  cfg_report     = true;
  cfg_reordering = true;
  cfg_discard    = false;
  cfg_duplicate  = false;
  cfg_t_report   = 1000;
  cfg_t_reordering = 100;
  rx_count = 0;
  cipher_algo = CIPHERING_ALGORITHM_ID_EEA0;
  integ_algo = INTEGRITY_ALGORITHM_ID_EIA0;
  
  pthread_mutex_init(&mutex, NULL);
}

void pdcp_entity::init(srsue::rlc_interface_pdcp      *rlc_,
					   srsue::lwaap_interface_pdcp    *lwaap_,
                       srsue::rrc_interface_pdcp      *rrc_,
                       srsue::gw_interface_pdcp       *gw_,
					   srslte::mac_interface_timers   *mac_timers_,
                       srslte::log                    *log_,
                       uint32_t                       lcid_,
                       srslte_pdcp_config_t           cfg_)
{
  rlc           = rlc_;
  lwaap         = lwaap_;
  rrc           = rrc_;
  gw            = gw_;
  log           = log_;
  lcid          = lcid_;
  cfg           = cfg_;
  active        = true;
  tx_count      = 0;
  rx_count      = 0;
  do_integrity  = false;
  do_encryption = false;

  mac_timers            = mac_timers_;
  reordering_timer_id   = mac_timers->timer_get_unique_id();
  reordering_timer      = mac_timers->timer_get(reordering_timer_id);
  report_timer_id       = mac_timers->timer_get_unique_id();
  report_timer          = mac_timers->timer_get(report_timer_id);
  
  cfg.sn_len    = 0;
  sn_len_bytes  = 0;

  if(cfg.is_control) {
    cfg.sn_len   = 5;
    sn_len_bytes = 1;
  }
  if(cfg.is_data) {
    cfg.sn_len   = 12;
    sn_len_bytes = 2;
  }
  
  if(cfg.is_data && lcid == 3) {
	do_lwa         = cfg_lwa;
	do_elwa        = cfg_elwa;
	do_report      = cfg_report;
	do_reordering  = cfg_reordering;
	do_discard     = cfg_discard;
	do_duplicate   = cfg_duplicate;
	t_report       = cfg_t_report;
    t_reordering   = cfg_t_reordering;
	next_sn        = 0;
    submitted_sn   = 0;
	reordering_sn  = 0;
	fms            = 0;
	reordering_cnt = 0;
	delayed_cnt    = 0;
	expired_cnt    = 0;
	duplicate_cnt  = 0;
	outoforder_cnt = 0;
	RX_MOD         = LONG_SN_MOD;
	REORDERING_WINDOW_SIZE = REORDERING_WINDOW_LONG_SN;
	
	if(do_report) {
	  report_timer->set(this, t_report);
	  report_timer->run();
	}
  }

  log->debug("Init %s\n", rrc->get_rb_name(lcid).c_str());
}

// Reestablishment procedure: 36.323 5.2
void pdcp_entity::reestablish() {
  // For SRBs
  if (cfg.is_control) {
    tx_count = 0;
    rx_count = 0;
  } else {
    if (rlc->rb_is_um(lcid)) {
      tx_count = 0;
      rx_count = 0;
    }
  }
}

void pdcp_entity::reset()
{
  active      = false;
  if(log)
    log->debug("Reset %s\n", rrc->get_rb_name(lcid).c_str());
}

bool pdcp_entity::is_active()
{
  return active;
}

void pdcp_entity::get_metrics(pdcp_metrics_t &m)
{
  m.reordering_cnt = reordering_cnt;
  m.delayed_cnt    = delayed_cnt;
  m.expired_cnt    = expired_cnt;
  m.duplicate_cnt  = duplicate_cnt;
  m.outoforder_cnt = outoforder_cnt;
  m.report_cnt     = report_cnt;
  
  reordering_cnt = 0;
  delayed_cnt = 0;
  expired_cnt = 0;
  duplicate_cnt = 0;
  outoforder_cnt = 0;
  report_cnt = 0;
}

void pdcp_entity::timer_expired(uint32_t timer_id)
{
  if(reordering_timer_id == timer_id)
  {
    pthread_mutex_lock(&mutex);
  
    // Status report
    log->warning("Timeout PDU SN: %d\n", submitted_sn);
    debug_state();
    //log->console("Timeout PDU SN: %d, Window %.1f %% used\n", submitted_sn, ((float)rx_window.size() / REORDERING_WINDOW_SIZE) * 100);
    expired_cnt++;
	fms = submitted_sn;
	
	if (!do_report) {
      generate_report();
	}

	// First catch up with lower edge of reordering window
    while(!inside_reordering_window(submitted_sn))
    {
      if(rx_window.count(submitted_sn)) {
		gw->write_pdu(lcid, rx_window[submitted_sn]);
		rx_window.erase(submitted_sn);
		rx_count++;
	  }
	  submitted_sn = (submitted_sn + 1) % RX_MOD;
	}
	
	// Now update submitted_sn until we reach an SN we haven't yet received
    while(RX_MOD_BASE(submitted_sn) < RX_MOD_BASE(reordering_sn))
    {
	  submitted_sn = (submitted_sn + 1) % RX_MOD;

	  // Update submitted_sn until we reach an SN we haven't yet received
	  while (rx_window.count(submitted_sn)) {
	    gw->write_pdu(lcid, rx_window[submitted_sn]);
	    rx_window.erase(submitted_sn);
	    submitted_sn = (submitted_sn + 1) % RX_MOD;
	    rx_count++;
	  }
    }
  
    reordering_timer->stop();
    if(do_reordering && RX_MOD_BASE(next_sn) > RX_MOD_BASE(submitted_sn))
    {
	  reordering_timer->set(this, t_reordering);
	  reordering_timer->run();
	  reordering_sn = next_sn;
	  reordering_cnt++;
    }

    debug_state();
    pthread_mutex_unlock(&mutex);
  } else if(report_timer_id == timer_id) {
    generate_report();
	
	report_timer->stop();
	report_timer->set(this, t_report);
	report_timer->run();
  }
}

void pdcp_entity::generate_report()
{
  byte_buffer_t *sdu = pool_allocate;
  
  // Make room and add FMS
  sdu->msg[sdu->N_bytes++] = (fms >> 8) & 0x7F;
  sdu->msg[sdu->N_bytes++] = fms & 0xFF;
  
  // Make room and add HFN & HRW
  sdu->msg[sdu->N_bytes++] = (hfn >> 9) & 0xFF;
  sdu->msg[sdu->N_bytes++] = (hfn >> 1) & 0xFF;
  sdu->msg[sdu->N_bytes++] = ((next_sn >> 8) & 0x7F) | ((hfn << 7) & 0x80);
  sdu->msg[sdu->N_bytes++] = next_sn & 0xFF;
  hfn = 0;
  
  // Count NMP
  uint32_t sn = (submitted_sn + 1) % RX_MOD, nmp = 0, nbits = 0, rx_status = 0;
  while(RX_MOD_BASE(sn) < RX_MOD_BASE(next_sn))
  {
	if (rx_window.find(sn) == rx_window.end()) {
	  nmp++;
	  // TODO: Dynamic adjust max length
	  if (nbits < 32) {
	    rx_status |= 1 << (31 - nbits);
	  }
	}
	sn = (sn + 1) % RX_MOD;
	
	if (nbits < 32)
	  nbits++;
  }
  // Make room and add NMP
  sdu->msg[sdu->N_bytes++] = (nmp >> 8) & 0x7F;
  sdu->msg[sdu->N_bytes++] = nmp & 0xFF;
  
  // Make room and add length of RWS
  sdu->msg[sdu->N_bytes++] = (nbits + 7) / 8;
  
  // Make room and add RWS
  sdu->msg[sdu->N_bytes++] = (rx_status >> 24) & 0xFF;
  sdu->msg[sdu->N_bytes++] = (rx_status >> 16) & 0xFF;
  sdu->msg[sdu->N_bytes++] = (rx_status >>  8) & 0xFF;
  sdu->msg[sdu->N_bytes++] =  rx_status        & 0xFF;
  
  if (cfg.is_data) {
    if (12 == cfg.sn_len) {
      pdcp_pack_data_pdu_long_sn(tx_count, sdu);
    } else {
      pdcp_pack_data_pdu_short_sn(tx_count, sdu);
    }
  }
  
  // Add report flag
  sdu->msg[0] |= 0x20;

  if(do_encryption) {
    cipher_encrypt(&sdu->msg[sn_len_bytes],
                    sdu->N_bytes-sn_len_bytes,
                   &sdu->msg[sn_len_bytes]);
    log->info_hex(sdu->msg, sdu->N_bytes, "TX %s SDU (encrypted)", rrc->get_rb_name(lcid).c_str());
  }
  tx_count++;
  report_cnt++;

  // TODO: send rrc report
  if(do_elwa) {
    lwaap->write_sdu(lcid, sdu);
  } else {
    rlc->write_sdu(lcid, sdu);
  }
}

// RRC interface
void pdcp_entity::write_sdu(byte_buffer_t *sdu)
{
  log->info_hex(sdu->msg, sdu->N_bytes,
        "TX %s SDU, SN: %d, do_integrity = %s, do_encryption = %s",
        rrc->get_rb_name(lcid).c_str(), tx_count,
        (do_integrity) ? "true" : "false", (do_encryption) ? "true" : "false");

  if (cfg.is_control) {
    pdcp_pack_control_pdu(tx_count, sdu);
    if(do_integrity) {
      integrity_generate(sdu->msg,
                         sdu->N_bytes-4,
                         &sdu->msg[sdu->N_bytes-4]);
    }
  }

  if (cfg.is_data) {
    if (12 == cfg.sn_len) {
      pdcp_pack_data_pdu_long_sn(tx_count, sdu);
    } else {
      pdcp_pack_data_pdu_short_sn(tx_count, sdu);
    }
  }

  if(do_encryption) {
    cipher_encrypt(&sdu->msg[sn_len_bytes],
                   sdu->N_bytes-sn_len_bytes,
                   &sdu->msg[sn_len_bytes]);
    log->info_hex(sdu->msg, sdu->N_bytes, "TX %s SDU (encrypted)", rrc->get_rb_name(lcid).c_str());
  }
  tx_count++;

  if (cfg.is_data && do_elwa) {
	lwaap->write_sdu(lcid, sdu);
  } else {
    rlc->write_sdu(lcid, sdu);
  }
}

void pdcp_entity::config_security(uint8_t *k_enc_,
                                  uint8_t *k_int_,
                                  CIPHERING_ALGORITHM_ID_ENUM cipher_algo_,
                                  INTEGRITY_ALGORITHM_ID_ENUM integ_algo_)
{
  for(int i=0; i<32; i++)
  {
    k_enc[i] = k_enc_[i];
    k_int[i] = k_int_[i];
  }
  cipher_algo = cipher_algo_;
  integ_algo  = integ_algo_;
}

void pdcp_entity::enable_integrity()
{
  do_integrity = true;
}

void pdcp_entity::enable_encryption()
{
  do_encryption = true;
}

// RLC interface
void pdcp_entity::write_pdu(byte_buffer_t *pdu)
{
  log->info_hex(pdu->msg, pdu->N_bytes, "RX %s PDU, do_integrity = %s, do_encryption = %s",
                rrc->get_rb_name(lcid).c_str(), (do_integrity) ? "true" : "false", (do_encryption) ? "true" : "false");

  // Sanity check
  if(pdu->N_bytes <= sn_len_bytes) {
    pool->deallocate(pdu);
    return;
  }

  // Handle DRB messages
  if (cfg.is_data) {
    uint32_t sn;
    if (do_encryption) {
      cipher_decrypt(&(pdu->msg[sn_len_bytes]),
                     rx_count,
                     pdu->N_bytes - sn_len_bytes,
                     &(pdu->msg[sn_len_bytes]));
      log->info_hex(pdu->msg, pdu->N_bytes, "RX %s PDU (decrypted)", rrc->get_rb_name(lcid).c_str());
    }
    if(12 == cfg.sn_len)
    {
	  if (timestamp_is_set(pdu)) {
		pdcp_unpack_data_pdu_long_sn_ack(pdu, &sn);
		generate_ack(sn);
	  } else {
        pdcp_unpack_data_pdu_long_sn(pdu, &sn);
	  }
    } else {
      pdcp_unpack_data_pdu_short_sn(pdu, &sn);
    }
    log->info_hex(pdu->msg, pdu->N_bytes, "RX %s PDU SN: %d", rrc->get_rb_name(lcid).c_str(), sn);
	
	// Out-of-order, SN went backward
	if (RX_MOD_BASE(sn) < RX_MOD_BASE(next_sn)) {
	  outoforder_cnt++;
	}
	
	// TODO: check rlc mode is am
	if (do_lwa && do_reordering) {
	  pthread_mutex_lock(&mutex);

	  if((RX_MOD_BASE(sn) >= RX_MOD_BASE(next_sn-REORDERING_WINDOW_SIZE) &&
		RX_MOD_BASE(sn) <  RX_MOD_BASE(submitted_sn)) /*||
		(sn - submitted_sn) % RX_MOD > REORDERING_WINDOW_SIZE*/)
	  {
		log->info("SN: %d outside rx window [%d:%d] - discarding\n",
				sn, submitted_sn, next_sn);
		if (do_discard) {
		  pool->deallocate(pdu);
		} else {
		  // Directly send delayed pdu to upper layer
		  gw->write_pdu(lcid, pdu);
		  rx_count++;
		}
		delayed_cnt++;
		pthread_mutex_unlock(&mutex);
		return;
	  }
	  
	  if (rx_window.count(sn))
	  {
		log->info("Discarding duplicate SN: %d\n", sn);
		duplicate_cnt++;
		if (do_duplicate) {
		  pool->deallocate(pdu);
		  pthread_mutex_unlock(&mutex);
		  return;
		} else {
		  // Pass stored pdu to upper layer
		  gw->write_pdu(lcid, rx_window[sn]);
		  rx_window.erase(sn);
		  rx_count++;
		}
	  }
	  
	  rx_window[sn] = pdu;

	  // Update next_sn
	  if (!inside_reordering_window(sn)) {
		if((sn + 1) % RX_MOD < next_sn) {
		  hfn++;
		}
		next_sn = (sn + 1) % RX_MOD;
	  }

	  // First catch up with lower edge of reordering window
      while(!inside_reordering_window(submitted_sn))
      {
        if(rx_window.count(submitted_sn)) {
		  gw->write_pdu(lcid, rx_window[submitted_sn]);
		  rx_window.erase(submitted_sn);
		  rx_count++;
	    }
	    submitted_sn = (submitted_sn + 1) % RX_MOD;
	  }
	
	  // Now update submitted_sn until we reach an SN we haven't yet received
	  while (rx_window.count(submitted_sn)) {
	    gw->write_pdu(lcid, rx_window[submitted_sn]);
	    rx_window.erase(submitted_sn);
	    submitted_sn = (submitted_sn + 1) % RX_MOD;
	    rx_count++;
	  }
	  
	  // Update reordering variables and timers
	  if (reordering_timer->is_running()) {
		if (RX_MOD_BASE(reordering_sn) <= RX_MOD_BASE(submitted_sn) ||
			(!inside_reordering_window(reordering_sn) && reordering_sn != next_sn)) {
		  reordering_timer->stop();
		}
	  } 
	  
	  if (!reordering_timer->is_running()) {
		if (RX_MOD_BASE(next_sn) > RX_MOD_BASE(submitted_sn)) {
		  reordering_sn = next_sn;
		  reordering_timer->set(this, t_reordering);
		  reordering_timer->run();
		  reordering_cnt++;
		}  
	  }
	  
	  debug_state();
	  pthread_mutex_unlock(&mutex);
	} else {
      if (!inside_reordering_window(sn))
		next_sn = (sn + 1) % RX_MOD;
		
      gw->write_pdu(lcid, pdu);
	  rx_count++;
	}
  } else {
    // Handle SRB messages
    if (cfg.is_control) {
      uint32_t sn;
      if (do_encryption) {
        cipher_decrypt(&(pdu->msg[sn_len_bytes]),
                       rx_count,
                       pdu->N_bytes - sn_len_bytes,
                       &(pdu->msg[sn_len_bytes]));
        log->info_hex(pdu->msg, pdu->N_bytes, "RX %s PDU (decrypted)", rrc->get_rb_name(lcid).c_str());
      }

      if (do_integrity) {
        integrity_verify(pdu->msg,
                         rx_count,
                         pdu->N_bytes - 4,
                         &(pdu->msg[pdu->N_bytes - 4]));
      }

      pdcp_unpack_control_pdu(pdu, &sn);
      log->info_hex(pdu->msg, pdu->N_bytes, "RX %s PDU SN: %d", rrc->get_rb_name(lcid).c_str(), sn);
    }
    // pass to RRC
    rrc->write_pdu(lcid, pdu);
	rx_count++;
  }
}

void pdcp_entity::integrity_generate( uint8_t  *msg,
                                      uint32_t  msg_len,
                                      uint8_t  *mac)
{
  uint8_t bearer;

  switch(integ_algo)
  {
  case INTEGRITY_ALGORITHM_ID_EIA0:
    break;
  case INTEGRITY_ALGORITHM_ID_128_EIA1:
    security_128_eia1(&k_int[16],
                      tx_count,
                      get_bearer_id(lcid),
                      cfg.direction,
                      msg,
                      msg_len,
                      mac);
    break;
  case INTEGRITY_ALGORITHM_ID_128_EIA2:
    security_128_eia2(&k_int[16],
                      tx_count,
                      get_bearer_id(lcid),
                      cfg.direction,
                      msg,
                      msg_len,
                      mac);
    break;
  default:
    break;
  }
}

bool pdcp_entity::integrity_verify(uint8_t  *msg,
                                   uint32_t  count,
                                   uint32_t  msg_len,
                                   uint8_t  *mac)
{
  uint8_t mac_exp[4] = {0x00};
  uint8_t i = 0;
  bool isValid = true;

  switch(integ_algo)
  {
  case INTEGRITY_ALGORITHM_ID_EIA0:
    break;
  case INTEGRITY_ALGORITHM_ID_128_EIA1:
    security_128_eia1(&k_int[16],
                      count,
                      get_bearer_id(lcid),
                      (cfg.direction == SECURITY_DIRECTION_DOWNLINK) ? (SECURITY_DIRECTION_UPLINK) : (SECURITY_DIRECTION_DOWNLINK),
                      msg,
                      msg_len,
                      mac_exp);
    break;
  case INTEGRITY_ALGORITHM_ID_128_EIA2:
    security_128_eia2(&k_int[16],
                      count,
                      get_bearer_id(lcid),
                      (cfg.direction == SECURITY_DIRECTION_DOWNLINK) ? (SECURITY_DIRECTION_UPLINK) : (SECURITY_DIRECTION_DOWNLINK),
                      msg,
                      msg_len,
                      mac_exp);
    break;
  default:
    break;
  }

  switch(integ_algo)
  {
  case INTEGRITY_ALGORITHM_ID_EIA0:
    break;
  case INTEGRITY_ALGORITHM_ID_128_EIA1: // Intentional fall-through
  case INTEGRITY_ALGORITHM_ID_128_EIA2:
    for(i=0; i<4; i++){
      if(mac[i] != mac_exp[i]){
        log->error_hex(mac_exp, 4, "MAC mismatch (expected)");
        log->error_hex(mac,     4, "MAC mismatch (found)");
        isValid = false;
        break;
      }
    }
    if (isValid){
      log->info_hex(mac_exp, 4, "MAC match (expected)");
      log->info_hex(mac,     4, "MAC match (found)");
    }
    break;
  default:
    break;
  }

  return isValid;
}

void pdcp_entity::cipher_encrypt(uint8_t  *msg,
                                 uint32_t  msg_len,
                                 uint8_t  *ct)
{
  byte_buffer_t ct_tmp;
  switch(cipher_algo)
  {
  case CIPHERING_ALGORITHM_ID_EEA0:
    break;
  case CIPHERING_ALGORITHM_ID_128_EEA1:
    security_128_eea1(&(k_enc[16]),
                      tx_count,
                      get_bearer_id(lcid),
                      cfg.direction,
                      msg,
                      msg_len,
                      ct_tmp.msg);
    memcpy(ct, ct_tmp.msg, msg_len);
    break;
  case CIPHERING_ALGORITHM_ID_128_EEA2:
    security_128_eea2(&(k_enc[16]),
                      tx_count,
                      get_bearer_id(lcid),
                      cfg.direction,
                      msg,
                      msg_len,
                      ct_tmp.msg);
    memcpy(ct, ct_tmp.msg, msg_len);
    break;
  default:
    break;
  }
}

void pdcp_entity::cipher_decrypt(uint8_t  *ct,
                                 uint32_t  count,
                                 uint32_t  ct_len,
                                 uint8_t  *msg)
{
  byte_buffer_t msg_tmp;
  switch(cipher_algo)
  {
  case CIPHERING_ALGORITHM_ID_EEA0:
    break;
  case CIPHERING_ALGORITHM_ID_128_EEA1:
    security_128_eea1(&(k_enc[16]),
                      count,
                      get_bearer_id(lcid),
                      (cfg.direction == SECURITY_DIRECTION_DOWNLINK) ? (SECURITY_DIRECTION_UPLINK) : (SECURITY_DIRECTION_DOWNLINK),
                      ct,
                      ct_len,
                      msg_tmp.msg);
    memcpy(msg, msg_tmp.msg, ct_len);
    break;
  case CIPHERING_ALGORITHM_ID_128_EEA2:
    security_128_eea2(&(k_enc[16]),
                      count,
                      get_bearer_id(lcid),
                      (cfg.direction == SECURITY_DIRECTION_DOWNLINK) ? (SECURITY_DIRECTION_UPLINK) : (SECURITY_DIRECTION_DOWNLINK),
                      ct,
                      ct_len,
                      msg_tmp.msg);
    memcpy(msg, msg_tmp.msg, ct_len);
    break;
  default:
    break;
  }
}

void pdcp_entity::get_diff_time(timespec *beginning, timespec *end, timespec *diff)
{
  diff->tv_sec = end->tv_sec - beginning->tv_sec;
  diff->tv_nsec = end->tv_nsec - beginning->tv_nsec;
  if (diff->tv_nsec < 0) {
    diff->tv_sec--;
    diff->tv_nsec += 1000000000;
  }
}

uint8_t pdcp_entity::get_bearer_id(uint8_t lcid)
{
  #define RB_ID_SRB2 2
  if(lcid <= RB_ID_SRB2) {
    return lcid - 1;
  } else {
    return lcid - RB_ID_SRB2 - 1;
  }
}

bool pdcp_entity::inside_reordering_window(uint32_t sn)
{
  if(RX_MOD_BASE(sn) >= RX_MOD_BASE(next_sn-REORDERING_WINDOW_SIZE) &&
	 RX_MOD_BASE(sn) <  RX_MOD_BASE(next_sn))
  {
    return true;
  }else{
	return false;
  }
}

bool pdcp_entity::timestamp_is_set(byte_buffer_t *pdu)
{
  return (pdu->N_bytes ? pdu->msg[0] & 0x10 : false);
}

void pdcp_entity::generate_ack(uint32_t sn)
{
  // Ack received SN
  byte_buffer_t sdu;
  sdu.msg[sdu.N_bytes++] = (sn >> 8) & 0xFF;
  sdu.msg[sdu.N_bytes++] =  sn       & 0xFF;
  
  pdcp_pack_data_pdu_long_sn_ack(tx_count, &sdu);

  if(do_encryption) {
	cipher_encrypt(&sdu.msg[sn_len_bytes],
				    sdu.N_bytes-sn_len_bytes,
				   &sdu.msg[sn_len_bytes]);
	log->info_hex(sdu.msg, sdu.N_bytes, "TX %s SDU (encrypted)", rrc->get_rb_name(lcid).c_str());
  }
  tx_count++;

  if (cfg.is_data && do_elwa) {
	lwaap->write_sdu(lcid, &sdu);
  } else {
	rlc->write_sdu(lcid, &sdu);
  }
}

void pdcp_entity::debug_state()
{
  log->debug("submitted sn = %d, reordering sn = %d, next sn = %d \n",
			 submitted_sn, reordering_sn, next_sn);
}


/********************************************************
 *
 * Class to run timers with normal priority
 *
 *******************************************************/
/*void pdcp_entity::timer_thread::run_thread()
{
  running=true; 
  while(running) {
    usleep(10000);
    timer->step();
  }
}

void pdcp_entity::timer_thread::stop()
{
  running=false;
  wait_thread_finish();
}
*/

void pdcp_entity::set_reorder_timeout(uint32_t timeout)
{
  log->console("Change reordering timeout %d to %d\n", t_reordering, timeout);
  t_reordering = cfg_t_reordering = timeout;
}

void pdcp_entity::set_reorder_window(uint32_t window)
{
  log->console("Change reordering window %d to %d\n", REORDERING_WINDOW_SIZE, window);
  REORDERING_WINDOW_SIZE = window;
}

void pdcp_entity::set_report_period(uint32_t period)
{
  log->console("Change report period %d to %d\n", t_report, period);
  t_report = cfg_t_report = period;
}

void pdcp_entity::toggle_reorder(bool enable)
{
  do_reordering = cfg_reordering = enable;
}

void pdcp_entity::toggle_report(bool enable)
{
  do_report = cfg_report = enable;
}

void pdcp_entity::toggle_lwa(bool enable)
{
  do_lwa = cfg_lwa = enable;
}

void pdcp_entity::toggle_elwa(bool enable)
{
  do_elwa = cfg_elwa = enable;
}

void pdcp_entity::toggle_discard(bool enable)
{
  do_discard = cfg_discard = enable;
}

void pdcp_entity::toggle_duplicate(bool enable)
{
  do_duplicate = cfg_duplicate = enable;
}


/****************************************************************************
 * Pack/Unpack helper functions
 * Ref: 3GPP TS 36.323 v10.1.0
 ***************************************************************************/

void pdcp_pack_control_pdu(uint32_t sn, byte_buffer_t *sdu)
{
  // Make room and add header
  sdu->msg--;
  sdu->N_bytes++;
  *sdu->msg = sn & 0x1F;

  // Add MAC
  sdu->msg[sdu->N_bytes++] = (PDCP_CONTROL_MAC_I >> 24) & 0xFF;
  sdu->msg[sdu->N_bytes++] = (PDCP_CONTROL_MAC_I >> 16) & 0xFF;
  sdu->msg[sdu->N_bytes++] = (PDCP_CONTROL_MAC_I >>  8) & 0xFF;
  sdu->msg[sdu->N_bytes++] =  PDCP_CONTROL_MAC_I        & 0xFF;

}

void pdcp_unpack_control_pdu(byte_buffer_t *pdu, uint32_t *sn)
{
  // Strip header
  *sn = *pdu->msg & 0x1F;
  pdu->msg++;
  pdu->N_bytes--;

  // Strip MAC
  pdu->N_bytes -= 4;

  // TODO: integrity check MAC
}

void pdcp_pack_data_pdu_short_sn(uint32_t sn, byte_buffer_t *sdu)
{
  // Make room and add header
  sdu->msg--;
  sdu->N_bytes++;
  sdu->msg[0] = (PDCP_D_C_DATA_PDU << 7) | (sn & 0x7F);
}

void pdcp_unpack_data_pdu_short_sn(byte_buffer_t *sdu, uint32_t *sn)
{
  // Strip header
  *sn  = sdu->msg[0] & 0x7F;
  sdu->msg++;
  sdu->N_bytes--;
}

void pdcp_pack_data_pdu_long_sn(uint32_t sn, byte_buffer_t *sdu)
{
  // Make room and add header
  sdu->msg     -= 2;
  sdu->N_bytes += 2;
  sdu->msg[0] = (PDCP_D_C_DATA_PDU << 7) | ((sn >> 8) & 0x0F);
  sdu->msg[1] = sn & 0xFF;
}

void pdcp_unpack_data_pdu_long_sn(byte_buffer_t *sdu, uint32_t *sn)
{
  // Strip header
  *sn  = (sdu->msg[0] & 0x0F) << 8;
  *sn |= sdu->msg[1];
  sdu->msg     += 2;
  sdu->N_bytes -= 2;
}

void pdcp_pack_data_pdu_long_sn_ack(uint32_t sn, byte_buffer_t *sdu)
{
  // Make room and add header
  sdu->msg     -= 2;
  sdu->N_bytes += 2;
  sdu->msg[0] = (PDCP_D_C_DATA_PDU << 7) | ((sn >> 8) & 0x0F);
  sdu->msg[1] = sn & 0xFF;
	
  // Add ack flag
  sdu->msg[0] |= 0x10;
}

void pdcp_unpack_data_pdu_long_sn_ack(byte_buffer_t *sdu, uint32_t *sn)
{
  pdcp_unpack_data_pdu_long_sn(sdu, sn);
}

}

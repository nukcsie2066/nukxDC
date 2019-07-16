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

namespace srslte {

pdcp_entity::pdcp_entity()
  :active(false)
  ,tx_count(0)
{
  pool = byte_buffer_pool::get_instance();
  log = NULL;
  rlc = NULL;
  lwaap = NULL;
  rrc = NULL;
  gw = NULL;
  lcid = 0;
  sn_len_bytes = 0;
  do_integrity = false;
  do_encryption = false;
  do_lwa = false;
  do_dc = false;
  rx_count = 0;
  cipher_algo = CIPHERING_ALGORITHM_ID_EEA0;
  integ_algo = INTEGRITY_ALGORITHM_ID_EIA0;
}

void pdcp_entity::init(srsue::rlc_interface_pdcp      *rlc_,
					   srsue::lwaap_interface_pdcp    *lwaap_,
                       srsue::rrc_interface_pdcp      *rrc_,
                       srsue::gw_interface_pdcp       *gw_,
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
  lte_tx_bytes  = 0;
  wifi_tx_bytes = 0;
  wifi_tx_count = 0;
  do_integrity  = false;
  do_encryption = false;
  do_timestamp  = false;
  do_autoconfig = false;
  do_packet_inspection = false;
  do_random_route = false;
  do_ema = true;
  
  cfg.sn_len    = 0;
  sn_len_bytes  = 0;
  if(cfg.is_control) {
    cfg.sn_len   = 5;
    sn_len_bytes = 1;
  }
  if(cfg.is_data) {
	cfg.sn_len   = 12;
	sn_len_bytes = 2;
	if(12 == cfg.sn_len) {
	  SN_MOD = LONG_SN_MOD;
	} else {
	  SN_MOD = SHORT_SN_MOD;
	}

	log->console("Data LCID %d\n", lcid);
	// Temporary
	if (3 == lcid) {
	  clock_gettime(CLOCK_MONOTONIC, &report_time[1]);
	  do_dc = true;
	  do_lwa = false;
	  set_lwa_ratio(1, 1);
	  std::srand(time(NULL));
	  last_hrw       = 0;
	  // Default alpha is 1/2
	  alpha_part     = 1;
	  alpha_whole    = 2;
	  ema_part       = 1;
	  ema_whole      = 1;
	  t_report       = 1000; // 1s
	}
  }

  log->debug("Init %s\n", rrc->get_rb_name(lcid).c_str());
}

void pdcp_entity::leaap_init() {
  /* Open RAW socket to send on */
  if ((eth_sock = socket(AF_PACKET, SOCK_RAW, IPPROTO_RAW)) == -1) {
    perror("socket");
	exit(EXIT_FAILURE);
  }
	
  /* Get the index of the interface to send on */
  memset(&eth_idx, 0, sizeof(struct ifreq));
  strncpy(eth_idx.ifr_name, ETH_IF, IFNAMSIZ - 1);
  log->console("IF IDX %s\n", eth_idx.ifr_name);
  if (ioctl(eth_sock, SIOCGIFINDEX, &eth_idx) < 0) {
    perror("SIOCGIFINDEX");
	exit(EXIT_FAILURE);
  }
	
  /* Get the MAC address of the interface to send on */
  memset(&eth_mac, 0, sizeof(struct ifreq));
  strncpy(eth_mac.ifr_name, ETH_IF, IFNAMSIZ - 1);
  log->console("IF MAC %s\n", eth_mac.ifr_name);
  if (ioctl(eth_sock, SIOCGIFHWADDR, &eth_mac) < 0) {
    perror("SIOCGIFHWADDR");
    exit(EXIT_FAILURE);
  }
}

void pdcp_entity::leaap_write_sdu(uint32_t lcid, byte_buffer_t *sdu) {
  log->info_hex(sdu->msg, sdu->N_bytes, "TX SDU, LCID: %d, n_bytes=%d", lcid, sdu->N_bytes);

  /* Ethernet header */
  ether_header header;
  header.ether_shost[0] = ((uint8_t *)&eth_mac.ifr_hwaddr.sa_data)[0];
  header.ether_shost[1] = ((uint8_t *)&eth_mac.ifr_hwaddr.sa_data)[1];
  header.ether_shost[2] = ((uint8_t *)&eth_mac.ifr_hwaddr.sa_data)[2];
  header.ether_shost[3] = ((uint8_t *)&eth_mac.ifr_hwaddr.sa_data)[3];
  header.ether_shost[4] = ((uint8_t *)&eth_mac.ifr_hwaddr.sa_data)[4];
  header.ether_shost[5] = ((uint8_t *)&eth_mac.ifr_hwaddr.sa_data)[5];
  header.ether_dhost[0] = UE_ETH_MAC0;
  header.ether_dhost[1] = UE_ETH_MAC1;
  header.ether_dhost[2] = UE_ETH_MAC2;
  header.ether_dhost[3] = UE_ETH_MAC3;
  header.ether_dhost[4] = UE_ETH_MAC4;
  header.ether_dhost[5] = UE_ETH_MAC5;

  /* Ethertype field */
  header.ether_type = htons(ETH_TYPE_ETH);

  sockaddr_ll ueaddr;
  /* Index of the network device */
  ueaddr.sll_ifindex = eth_idx.ifr_ifindex;
  /* Address length*/
  ueaddr.sll_halen = ETH_ALEN;
  /* Destination MAC */
  ueaddr.sll_addr[0] = UE_ETH_MAC0;
  ueaddr.sll_addr[1] = UE_ETH_MAC1;
  ueaddr.sll_addr[2] = UE_ETH_MAC2;
  ueaddr.sll_addr[3] = UE_ETH_MAC3;
  ueaddr.sll_addr[4] = UE_ETH_MAC4;
  ueaddr.sll_addr[5] = UE_ETH_MAC5;

  leaap_write_header(&header, lcid, sdu);

  /* Send packet */
  if (sendto(eth_sock, sdu->msg, sdu->N_bytes, MSG_EOR, (struct sockaddr*)&ueaddr, sizeof(struct sockaddr_ll)) < 0) {
    perror("sendto");
	log->console("sendto l %d\n", sdu->N_bytes);
  }
	
  pool->deallocate(sdu);
}

bool pdcp_entity::leaap_write_header(ether_header *header, uint32_t lcid, byte_buffer_t *sdu)
{
  sdu->msg      -= sizeof(ether_header) + 1; // 1 Byte for lcid
  sdu->N_bytes  += sizeof(ether_header) + 1;

  uint8_t *ptr = sdu->msg;

  *ptr        = header->ether_dhost[0];
  *(ptr + 1)  = header->ether_dhost[1];
  *(ptr + 2)  = header->ether_dhost[2];
  *(ptr + 3)  = header->ether_dhost[3];
  *(ptr + 4)  = header->ether_dhost[4];
  *(ptr + 5)  = header->ether_dhost[5];
  *(ptr + 6)  = header->ether_shost[0];
  *(ptr + 7)  = header->ether_shost[1];
  *(ptr + 8)  = header->ether_shost[2];
  *(ptr + 9)  = header->ether_shost[3];
  *(ptr + 10) = header->ether_shost[4];
  *(ptr + 11) = header->ether_shost[5];
  ptr += 12;

  *ptr        = header->ether_type & 0xFF;
  *(ptr + 1)  = (header->ether_type >> 8) & 0xFF;
  ptr += 2;
  *ptr        = lcid & 0xFF;
	
  return true;
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
/*
  gettimeofday(&metrics_time[2], NULL);
  get_time_interval(metrics_time);
  double secs = (double) metrics_time[0].tv_sec+metrics_time[0].tv_usec*1e-6;
	
  m.dl_tput_bps = (tx_bytes*8)/secs;
  m.ul_tput_bps = (rx_bytes*8)/secs;
  log->info("RX throughput: %4.6f Mbps. TX throughput: %4.6f Mbps.\n",
			  m.dl_tput_bps/1e6, m.ul_tput_bps/1e6);
  
  m.tx_pdus      = tx_pdus;
  m.rx_sdus      = rx_sdus;
*/
  m.lte_ratio    = lte_ratio;
  m.wifi_ratio   = wifi_ratio;
/*
  m.lte_tx_pdus  = lte_tx_count *1;
  m.wifi_tx_pdus = wifi_tx_count *1;

  memcpy(&metrics_time[1], &metrics_time[2], sizeof(struct timeval));
  tx_pdus  = 0;
  tx_bytes = 0;
  rx_sdus  = 0;
  rx_bytes = 0;
  lte_tx_count   = 0;
  wifi_tx_count  = 0;
*/
}

// RRC interface
void pdcp_entity::write_sdu(byte_buffer_t *sdu)
{
  uint8_t version=0;
  uint32_t dest_ip;
  uint16_t dest_port;
  struct in_addr  dest_addr;
	
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
    // Packet inspection
    if (do_packet_inspection) {
	  // IP address
	  version = sdu->msg[0]>>4;
	  ((uint8_t*)&dest_ip)[0] = sdu->msg[16];
	  ((uint8_t*)&dest_ip)[1] = sdu->msg[17];
	  ((uint8_t*)&dest_ip)[2] = sdu->msg[18];
	  ((uint8_t*)&dest_ip)[3] = sdu->msg[19];
	  
	  dest_addr.s_addr = dest_ip;
	  
	  // Port number
	  ((uint8_t*)&dest_port)[0] = sdu->msg[22];
	  ((uint8_t*)&dest_port)[1] = sdu->msg[23];
	  
	  //log->console("IP version: %d\n", version);
	  log->console("IP to %s, %d\n", inet_ntoa(dest_addr), dest_port);
	}

    // Add PDCP header
    if(12 == cfg.sn_len) {
	  if(do_timestamp) {
		clock_gettime(CLOCK_MONOTONIC, &timestamp_time[2]);
		get_timestamp_interval(timestamp_time);
		// Convert nsec to usec
		uint32_t timestamp = timestamp_time[0].tv_nsec / 1000;
		// Ignore diff time larger than 254
		if(timestamp > 254) {
          timestamp = 255;
		}
        pdcp_pack_data_pdu_long_sn_timestamp(tx_count, timestamp, sdu);
	  }
	  else {
		pdcp_pack_data_pdu_long_sn(tx_count, sdu);
	  }
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
  
  if (cfg.is_data) {
    tx_bytes += sdu->N_bytes;
    tx_pdus++;
    if (do_lwa && get_route() == ROUTE_WLAN) {
      tx_route[tx_count % SN_MOD] = ROUTE_WLAN;
	  wifi_tx_count++;
	  wifi_tx_bytes += sdu->N_bytes;
	  lwaap->write_sdu(lcid, sdu);
    } else if (do_dc) {
	  gw->write_sdu(lcid, sdu);
	} else {
      tx_route[tx_count % SN_MOD] = ROUTE_LTE;
	  lte_tx_count++;
	  lte_tx_bytes += sdu->N_bytes;
	  rlc->write_sdu(lcid, sdu);
	}
  } else {
	rlc->write_sdu(lcid, sdu);
  }
  tx_count++;
  //debug_state();
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
	bool     is_lwa_report = false;
	
	rx_bytes += pdu->N_bytes;
	
    if (do_encryption) {
      cipher_decrypt(&(pdu->msg[sn_len_bytes]),
                     rx_count,
                     pdu->N_bytes - sn_len_bytes,
                     &(pdu->msg[sn_len_bytes]));
      log->info_hex(pdu->msg, pdu->N_bytes, "RX %s PDU (decrypted)", rrc->get_rb_name(lcid).c_str());
    }
	
	// Need change to rrc report
    is_lwa_report = lwa_report_is_set(pdu);
	
    if (12 == cfg.sn_len)
    {
      pdcp_unpack_data_pdu_long_sn(pdu, &sn);
    } else {
      pdcp_unpack_data_pdu_short_sn(pdu, &sn);
    }
	
    log->info_hex(pdu->msg, pdu->N_bytes, "RX %s PDU SN: %d", rrc->get_rb_name(lcid).c_str(), sn);
	if (is_lwa_report) {
      handle_lwa_report(pdu);
	} else {
      gw->write_pdu(lcid, pdu);
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
    rrc->write_pdu(lcid, pdu);
  }
  rx_sdus++;
  rx_count++;
}

bool pdcp_entity::lwa_report_is_set(byte_buffer_t *pdu)
{
  return (pdu->N_bytes ? pdu->msg[0] & 0x20 : false);
}

void pdcp_entity::handle_lwa_report(byte_buffer_t *pdu)
{
  if (pdu->N_bytes < 8) {
    pool->deallocate(pdu);
	return;
  }

  clock_gettime(CLOCK_MONOTONIC, &report_time[2]);
  get_timestamp_interval(report_time);
  memcpy(&report_time[1], &report_time[2], sizeof(struct timespec));
  
  // First Missing SN
  uint32_t fms = (pdu->msg[0] & 0x7F) << 8;
  fms |= pdu->msg[1];
  
  //log->console("FMS %d, RS %d, TS %d\n", fms, reconfigure_sn % SN_MOD, tx_count % SN_MOD);
  if (do_autoconfig && tx_count >= reconfigure_sn) {
	// Hyper Frame Number
	uint32_t hfn = pdu->msg[2] << 9;
	hfn |= pdu->msg[3] << 1;
	hfn |= pdu->msg[4] >> 7;
	  
    // Highest Received WLAN SN
    uint32_t hrw = (pdu->msg[4] & 0x7F) << 8;
    hrw |= pdu->msg[5];
  
    // Number of Missing PDUs
    uint32_t nmp = (pdu->msg[6] & 0x7F) << 8;
    nmp |= pdu->msg[7];
/*  
    uint8_t  rws_len   = pdu->msg[8];
    uint8_t  rxed_lte  = 0;
    uint8_t  rxed_wlan = 0;
    uint8_t  unrx_lte  = 0;
    uint8_t  unrx_wlan = 0;
    uint32_t rws       = 0;
	uint32_t msn       = 0;
  
    // RWS start with 7th byte
    for (uint8_t i = 0; i < rws_len && i < 4; i++) {
      rws <<= 8;
      rws  |= pdu->msg[i + 9];
	
	  for (uint8_t b = 0; b < 8; b++) {
		msn = (fms + i * 8 + b + 1) % SN_MOD;
	    if ((pdu->msg[i + 9] >> (7 - b)) & 1) {
		  if (tx_route[msn] == ROUTE_LTE) {
		    unrx_lte++;
		  } else {
            unrx_wlan++;
		  }
	    } else {
		  if (tx_route[msn] == ROUTE_LTE) {
		    rxed_lte++;
		  } else {
		    rxed_wlan++;
		  }
	    }
		
		if (msn == hrw) {
		  break;
		}
	  }
    }
    log->console("%s MS %d, SN[%d-%d], RWS len=%d, UnRx LTE=(%d/%d), UnRx WiFi=(%d/%d)\n", tx_route[fms] == ROUTE_LTE ? "LTE" : "WiFi", fms,
										  (fms + 1) % SN_MOD, hrw, rws_len, unrx_lte, unrx_lte + rxed_lte, unrx_wlan, unrx_wlan + rxed_wlan);
*/  
	//log->console("FMS %4d, HFN %4d, HRW %4d, NMP %4d\n", fms, hfn, hrw, nmp);
	if (false) {
/*	  if (rxed_lte < unrx_lte) {
	    rxed_lte /= 2;
	  } else if (tx_route[fms] == ROUTE_LTE && rxed_lte) {
	    rxed_lte--;
	  }
	  if (rxed_wlan < unrx_wlan) {
	    unrx_wlan /= 2;
	  } else if (tx_route[fms] == ROUTE_WLAN && rxed_wlan) {
	    rxed_wlan--;
	  }
	
      if (rxed_lte == 0 && rxed_wlan == 0) {
	    // Disconnet ?
	  } else if (rxed_lte == 0) {
	    set_lwa_ratio(0, 1);
	  } else if (rxed_wlan == 0) {
	    set_lwa_ratio(1, 0);
	  } else {
	    uint8_t gcd = get_gcd(rxed_lte, rxed_wlan);
	    set_lwa_ratio(rxed_lte / gcd, rxed_wlan / gcd);
	  }
*/	} else /*if (fms != hrw)*/ {
	  // Get number of lost lte in report
	  uint32_t sn = fms, lost_lte = 0;
	  while (sn != hrw) {
		if (tx_route[sn] == ROUTE_LTE) {
		  lost_lte++;
		}
		sn = (sn + 1) % SN_MOD;
	  }
	
	  float max_ratio = 2.0f;
	  uint32_t lost_wlan = 0, ack_wlan = 0, ack_lte = 0, pdu_avg_size = 1500;
	  // Bits-in-flight
	  uint32_t bif_wlan = wifi_tx_count, bif_lte = lte_tx_count;
	  float rate_wlan, rate_lte, uplink_latency = 0, tx_rate_wlan = 0;
	  if (wifi_tx_count > 0) {
		pdu_avg_size = wifi_tx_bytes / wifi_tx_count * 8;
		tx_rate_wlan = (float) wifi_tx_count / (report_time[0].tv_sec + (float) report_time[0].tv_nsec / 1000000000);
		wifi_tx_bytes = wifi_tx_count = 0;
	    if (nmp > lost_lte) {
	      lost_wlan = (nmp - lost_lte);
	    }
	  
		if (hrw != last_hrw || hfn > 0) {
		  hfn <<= cfg.sn_len;
		  hfn  += hrw - last_hrw;
		  ack_lte  = hfn * lte_ratio / total_ratio;
		  ack_wlan = hfn * wifi_ratio / total_ratio;
		  last_hrw = hrw;
		  
		  if (bif_wlan > ack_wlan) {
			bif_wlan -= ack_wlan;
		  } else {
			bif_wlan = 0;
		  }
		}
		
	    if (ack_wlan > lost_wlan) {
		  ack_wlan -= lost_wlan;
	    } else {
	      ack_wlan = 0;
	    }
		
		if (bif_wlan > uplink_latency * tx_rate_wlan) {
		  bif_wlan -= uplink_latency * tx_rate_wlan;
		} else {
		  bif_wlan = 0;
		}
		//ack_wlan *= pdu_avg_size;
		//bif_wlan *= pdu_avg_size;
	  }
	  
	  // Calculate WLAN rate
	  if (ack_wlan == 0/* || bif_wlan == 0*/) {
	    rate_wlan = rate_init_wlan / pdu_avg_size;
	  } else {
		rate_wlan = (float) ack_wlan / (report_time[0].tv_sec + (float) report_time[0].tv_nsec / 1000000000); //bitrate
	  }
	  
	  // Caluculate uplink latency(s)
	  if (report_time[0].tv_sec * 1000 + report_time[0].tv_nsec / 1000000 > t_report) {
		uplink_latency  = report_time[0].tv_sec * 1000 + report_time[0].tv_nsec / 1000000 - t_report;
		uplink_latency /= 1000;
	  }

	  // Calculate LTE bits-in-flight
	  if (lte_tx_count > 0) {
	    pdu_avg_size = lte_tx_bytes / lte_tx_count * 8;
		lte_tx_bytes = lte_tx_count = 0;
		
		if (bif_lte > ack_lte) {
		  bif_lte -= ack_lte;
		} else {
		  bif_lte = 0;
		}
		
		//ack_lte *= pdu_avg_size;
		//bif_lte *= pdu_avg_size;
	  }
	  
	  // Calculate LTE rate(get RLC status report)
	  if (ack_lte == 0/* || bif_lte == 0*/) {
		rate_lte = rate_init_lte / pdu_avg_size;
		// If LTE not activated, try to transmit a part of WLAN bits-in-flight
		if (lte_ratio == 0) {
		  bif_lte = bif_wlan * (max_ratio - 1) / max_ratio;
		}
	  } else {
		rate_lte = (float) ack_lte / (report_time[0].tv_sec + (float) report_time[0].tv_nsec / 1000000000);
		if (rate_lte > rate_init_lte / pdu_avg_size) {
		  bif_lte += (rate_lte - rate_init_lte / pdu_avg_size) * (report_time[0].tv_sec + (float) report_time[0].tv_nsec / 1000000000);
		  rate_lte = rate_init_lte / pdu_avg_size;
		}
	  }

	  // Packet size is 1500 bytes(1 PDU)
	  uint32_t pdu_size = 1;
	  float delay_lte = (float) (pdu_size + bif_lte + rlc->get_buffer_state(lcid) * 8 / pdu_avg_size) / rate_lte * 1000; //ms
	  float delay_wlan = (float) (pdu_size + bif_wlan) / rate_wlan * 1000;

	  /*if (delay_lte > max_ratio * delay_wlan) {
	    delay_lte  = (float)MAX_RATIO / 1000;
	    delay_wlan = 1.0f / 1000;
	  }*/

	  //log->console("*HFN %8d, Ratio %8.2f, Uplink * WLAN TX %8.2f, lte:wlan=%4d:%d\n", hfn, delay_lte / delay_wlan, uplink_latency * tx_rate_wlan, lte_ratio, wifi_ratio);
	  //log->console(" - LTE  Ack %10d, Bif %10d, Rate %8.2f, Delay %8.3f ms\n", ack_lte, bif_lte, rate_lte / 1000000 * pdu_avg_size, delay_lte);
	  //log->console(" - WLAN Ack %10d, Bif %10d, Rate %8.2f, Delay %8.3f ms\n", ack_wlan, bif_wlan, rate_wlan / 1000000 * pdu_avg_size, delay_wlan);
	  if (do_ema) {
		ema_part   = alpha_part * (delay_wlan * 1000) * ema_whole + (alpha_whole - alpha_part) * ema_part * (delay_lte * 1000);
		ema_whole *= alpha_whole * (delay_lte * 1000);
		
		while (ema_part >= MAX_RATIO && ema_whole >= MAX_RATIO) {
		  ema_part  /= MAX_RATIO;
		  ema_whole /= MAX_RATIO;
		}
		
        //log->console("*EMA %d/%d, DLTE %6d ms, DWLAN %6d ms\n", ema_part, ema_whole, (int)(delay_lte * 1000), (int)(delay_wlan * 1000));
		if (ema_whole > max_ratio * ema_part) {
		  set_lwa_ratio(0, 1);
		} else if (ema_whole >= ema_part) {
		  set_lwa_ratio(ema_part, ema_whole);
		}
	  } else {
		while (delay_wlan * 1000 >= MAX_RATIO && delay_lte * 1000 >= MAX_RATIO) {
		  delay_lte  /= MAX_RATIO;
		  delay_wlan /= MAX_RATIO;
		}
	    set_lwa_ratio((uint32_t)(delay_wlan * 1000), (uint32_t)(delay_lte * 1000));
	  }
	}
  }

  pool->deallocate(pdu);
}

bool pdcp_entity::timestamp_is_set(byte_buffer_t *pdu)
{
  return (pdu->N_bytes ? pdu->msg[0] & 0x10 : false);
}

void pdcp_entity::set_lwa_ratio(uint32_t lr, uint32_t wr)
{
  if (lr == 0 && wr == 0) {
    return;
  }
  
  if (do_lwa) {
	if (lte_ratio == lr && wifi_ratio == wr) {
	  return;
	}
	
	lte_ratio   = lr;
    wifi_ratio  = wr;
	
	// Update base ratio
	if (lte_ratio == 0) {
	  base_ratio = wifi_ratio;
	} else if (wifi_ratio == 0) {
	  base_ratio = lte_ratio;
	} else {
	  base_ratio = get_gcd(lte_ratio, wifi_ratio);
	  if (base_ratio > 1) {
	    lte_ratio  /= base_ratio;
		wifi_ratio /= base_ratio;
	  }
	  base_ratio = (lte_ratio < wifi_ratio ? lte_ratio : wifi_ratio);
	}
	
	total_ratio = lte_ratio + wifi_ratio;
    
	// Reset ratio count
	lte_ratio_count  = 0;
	wifi_ratio_count = 0;
	reconfigure_sn   = tx_count;
	
	//log->console("Now change ratio to %d:%d\n", lr, wr);
  }
}

void pdcp_entity::set_ema_ratio(uint32_t part, uint32_t whole)
{
  if (part == 0 && whole == 0) {
	do_ema = false;
    return;
  }
  
  if (!do_ema) {
    do_ema = true;
  }
  
  alpha_part  = part;
  alpha_whole = whole;
  
  // Reset EMA
  ema_part  = 1;
  ema_whole = 1;
}

void pdcp_entity::set_report_period(uint32_t period)
{
  t_report = period;
}

void pdcp_entity::toggle_timestamp(bool b)
{
  do_timestamp = b;
}

void pdcp_entity::toggle_autoconfig(bool b)
{
  do_autoconfig = b;
}

void pdcp_entity::toggle_random(bool b)
{
  do_random_route = b;
}

void pdcp_entity::debug_state()
{
  log->console("TX LTE %d(%d) WiFi %d(%d)\n",
			   tx_count - wifi_tx_count, lte_tx_bytes, wifi_tx_count, wifi_tx_bytes);
}

route_t pdcp_entity::get_route()
{
  if (do_random_route) {
	if (std::rand()/((RAND_MAX + 1u)/total_ratio) < lte_ratio) {
	  return ROUTE_LTE;
	} else {
	  return ROUTE_WLAN;
	}
  }
	
  if (lte_ratio_count >= base_ratio) {
	lte_ratio_count -= base_ratio;
	return ROUTE_LTE;
  } else if (wifi_ratio_count >= base_ratio) {
	wifi_ratio_count -= base_ratio;
	return ROUTE_WLAN;
  } else {
	lte_ratio_count  += lte_ratio;
	wifi_ratio_count += wifi_ratio;
  }
  return get_route();
/* Old version
  if (tx_count % (lte_ratio + wifi_ratio) >= lte_ratio)
    return ROUTE_WLAN;
  return ROUTE_LTE;
*/
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


uint8_t pdcp_entity::get_bearer_id(uint8_t lcid)
{
  #define RB_ID_SRB2 2
  if(lcid <= RB_ID_SRB2) {
    return lcid - 1;
  } else {
    return lcid - RB_ID_SRB2 - 1;
  }
}

uint8_t pdcp_entity::get_gcd(uint32_t x, uint32_t y)
{
  uint32_t tmp;
  while (y != 0) {
    tmp = x % y;
	x = y;
	y = tmp;
  }
  return x;
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

void pdcp_pack_data_pdu_long_sn_timestamp(uint32_t sn, uint8_t timestamp, byte_buffer_t *sdu)
{
  // Make room and add header
  sdu->msg     -= 3;
  sdu->N_bytes += 3;
  sdu->msg[0] = (PDCP_D_C_DATA_PDU << 7) | ((sn >> 8) & 0x0F);
  sdu->msg[1] = sn & 0xFF;
  
  // Add timestamp flag
  sdu->msg[0] |= 0x10;
  sdu->msg[2]  = timestamp & 0xFF;
}

void pdcp_unpack_data_pdu_long_sn_timestamp(byte_buffer_t *sdu, uint32_t *sn, uint8_t *timestamp)
{
  // Strip header
  *sn  = (sdu->msg[0] & 0x0F) << 8;
  *sn |= sdu->msg[1];
  *timestamp = sdu->msg[2];
  sdu->msg     += 3;
  sdu->N_bytes -= 3;
}

}

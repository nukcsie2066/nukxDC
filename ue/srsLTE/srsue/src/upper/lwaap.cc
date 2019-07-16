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


#include "srsue/hdr/upper/lwaap.h"

namespace srsue {

lwaap::lwaap()
{
}

void lwaap::init(pdcp_interface_lwaap *pdcp_, srslte::log *lwaap_log_, srslte::srslte_lwaap_config_t cfg_)
{
  pool    	= srslte::byte_buffer_pool::get_instance();
  pdcp    	= pdcp_;
  lwaap_log	= lwaap_log_;
  cfg     	= cfg_;
  run_enable = true;

  gettimeofday(&metrics_time[1], NULL);
  dl_tput_bytes = 0;
  ul_tput_bytes = 0;

  memset(&ifr, 0, sizeof(struct ifreq));

  // Open PF_PACKET socket, listening for EtherType ETHER_TYPE_WIFI
  if ((sockfd = socket(PF_PACKET, SOCK_RAW, htons(ETH_TYPE_WIFI))) == -1) {
	  lwaap_log->error("listener: socket");	
	  return;
  }

  // Set interface to promiscuous mode
  strncpy(ifr.ifr_name, WIFI_IF, IFNAMSIZ - 1);
  ioctl(sockfd, SIOCGIFFLAGS, &ifr);
  ifr.ifr_flags |= IFF_PROMISC;
  ioctl(sockfd, SIOCSIFFLAGS, &ifr);
  // Allow the socket to be reused - incase connection is closed prematurely
  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &sockopt, sizeof sockopt) == -1) {
	  lwaap_log->error("setsockopt");
	  close(sockfd);
	  return;
  }
  // Bind to device
  if (setsockopt(sockfd, SOL_SOCKET, SO_BINDTODEVICE, WIFI_IF, IFNAMSIZ - 1) == -1)	{
	  lwaap_log->error("SO_BINDTODEVICE");
	  close(sockfd);
	  return;
  }
  
  // Get the index of the interface to send on
  memset(&if_idx, 0, sizeof(struct ifreq));
  strncpy(if_idx.ifr_name, WIFI_IF, IFNAMSIZ - 1);
  if (ioctl(sockfd, SIOCGIFINDEX, &if_idx) < 0) {
	perror("SIOCGIFINDEX");
	exit(EXIT_FAILURE);
  }

  // Get the MAC address of the interface to send on
  memset(&if_mac, 0, sizeof(struct ifreq));
  strncpy(if_mac.ifr_name, WIFI_IF, IFNAMSIZ - 1);
  if (ioctl(sockfd, SIOCGIFHWADDR, &if_mac) < 0) {
	perror("SIOCGIFHWADDR");
	exit(EXIT_FAILURE);
  }
  lwaap_log->console("LWAAP MAC %x:%x:%x:%x:%x:%x\n",
			  ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[0],
			  ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[1],
			  ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[2],
			  ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[3],
			  ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[4],
			  ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[5]);
  
  start(LWAAP_THREAD_PRIO);
}

void lwaap::stop()
{
  if(run_enable)
  {
    run_enable = false;

    close(sockfd);
      
    // Wait thread to exit gracefully otherwise might leave a mutex locked
    int cnt=0;
    while(running && cnt<100) {
      usleep(10000);
      cnt++;
    }
    if (running) {
      thread_cancel();
    }
    wait_thread_finish();
  }
}

void lwaap::get_metrics(lwaap_metrics_t &m)
{
  
  gettimeofday(&metrics_time[2], NULL);
  get_time_interval(metrics_time);
  double secs = (double) metrics_time[0].tv_sec+metrics_time[0].tv_usec*1e-6;
  
  m.dl_tput_brate = (dl_tput_bytes*8)/secs;
  m.dl_tput_bits  =  dl_tput_bytes*8;
  m.ul_tput_brate = (ul_tput_bytes*8)/secs;
  m.ul_tput_bits  =  ul_tput_bytes*8;
  lwaap_log->info("RX throughput: %4.6f Mbps. TX throughput: %4.6f Mbps.\n",
               m.dl_tput_brate/(double)1e6, m.ul_tput_brate/(double)1e6);

  memcpy(&metrics_time[1], &metrics_time[2], sizeof(struct timeval));
  dl_tput_bytes = 0;
  ul_tput_bytes = 0;
}

/*******************************************************************************
  PDCP interface
*******************************************************************************/
void lwaap::write_sdu(uint32_t lcid, srslte::byte_buffer_t *sdu)
{
  // Ethernet header
  ether_header header;
  header.ether_shost[0] = ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[0];
  header.ether_shost[1] = ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[1];
  header.ether_shost[2] = ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[2];
  header.ether_shost[3] = ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[3];
  header.ether_shost[4] = ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[4];
  header.ether_shost[5] = ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[5];
  header.ether_dhost[0] = ENB_MAC0;
  header.ether_dhost[1] = ENB_MAC1;
  header.ether_dhost[2] = ENB_MAC2;
  header.ether_dhost[3] = ENB_MAC3;
  header.ether_dhost[4] = ENB_MAC4;
  header.ether_dhost[5] = ENB_MAC5;

  // Ethertype field
  header.ether_type = htons(ETH_TYPE_WIFI);

  sockaddr_ll ueaddr;
  // Index of the network device
  ueaddr.sll_ifindex = if_idx.ifr_ifindex;
  // Address length
  ueaddr.sll_halen = ETH_ALEN;
  // Destination MAC
  ueaddr.sll_addr[0] = ENB_MAC0;
  ueaddr.sll_addr[1] = ENB_MAC1;
  ueaddr.sll_addr[2] = ENB_MAC2;
  ueaddr.sll_addr[3] = ENB_MAC3;
  ueaddr.sll_addr[4] = ENB_MAC4;
  ueaddr.sll_addr[5] = ENB_MAC5;
  
  lwaap_write_header(&header, lcid, sdu);
  
  ul_tput_bytes += sdu->N_bytes;

  // Send packet
  if (sendto(sockfd, sdu->msg, sdu->N_bytes, MSG_EOR, (struct sockaddr*)&ueaddr, sizeof(struct sockaddr_ll)) < 0) {
    perror("sendto");
	lwaap_log->console("sendto l %d\n", sdu->N_bytes);
  }
  
  pool->deallocate(sdu);
}

bool lwaap::lwaap_write_header(ether_header *header, uint32_t lcid, srslte::byte_buffer_t *sdu)
{
  sdu->msg      -= sizeof(ether_header) + LWAAP_HEADER_LEN; // 1 Byte for lcid
  sdu->N_bytes  += sizeof(ether_header) + LWAAP_HEADER_LEN;

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

/***********************/
/*    LWAAP Receive    */
/***********************/
void lwaap::run_thread()
{
  int32                   N_bytes;
  uint8_t                 drbid;
  srslte::byte_buffer_t  *pdu = pool_allocate;

  lwaap_log->info("LWAAP IP packet receiver thread run_enable\n");

  running = true;
  while(run_enable)
  {
    N_bytes = recvfrom(sockfd, pdu->msg, SRSLTE_MAX_BUFFER_SIZE_BYTES-SRSLTE_BUFFER_HEADER_OFFSET, 0, NULL, NULL);
    
    if(N_bytes > 0)
    {
      pdu->N_bytes = N_bytes;

      // Check the packet is for me
      if (lwaap_read_header(pdu, &drbid)) {        
		lwaap_log->info_hex(pdu->msg, pdu->N_bytes, "RX PDU");
		
		// Send PDU directly to PDCP
        pdu->set_timestamp();
		dl_tput_bytes += N_bytes;
        pdcp->write_pdu(drbid, pdu);

        do {
          pdu = pool_allocate;
          if (!pdu) {
            printf("Not enough buffers in pool\n");
            usleep(100000);
          }
        } while(!pdu);
      }
    }else{
      lwaap_log->error("Failed to read from TUN interface - lwaap receive thread exiting.\n");
      break;
    }
  }
  running = false;
  lwaap_log->info("LWAAP IP receiver thread exiting.\n");
}

bool lwaap::lwaap_read_header(srslte::byte_buffer_t *pdu, uint8_t *drbid)
{
  uint8_t *ptr            = pdu->msg;
  struct ether_header *eh = (struct ether_header *) ptr;

  pdu->msg      += sizeof(struct ether_header) + LWAAP_HEADER_LEN;
  pdu->N_bytes  -= sizeof(struct ether_header) + LWAAP_HEADER_LEN;

  // Check the packet is for me
  if (eh->ether_dhost[0] == ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[0] &&
	  eh->ether_dhost[1] == ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[1] &&
	  eh->ether_dhost[2] == ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[2] &&
	  eh->ether_dhost[3] == ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[3] &&
	  eh->ether_dhost[4] == ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[4] &&
	  eh->ether_dhost[5] == ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[5]) {
    // Check the ether type is WLAN
    if (eh->ether_type == htons(ETH_TYPE_WIFI)) {
	  ptr   += sizeof(struct ether_header);
	  *drbid = *ptr;
      return true;
    }
  }

  return false;
}

} // namespace srsue

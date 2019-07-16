
#ifndef ENB_PDCP_METRICS_H
#define ENB_PDCP_METRICS_H


namespace srslte {

struct pdcp_metrics_t
{
  double dl_tput_bps;
  double ul_tput_bps;
  int tx_pdus;	// Count from gtpu
  int rx_sdus;	// Count from rlc & lwaap
  int lte_ratio;
  int wifi_ratio;
  int lte_tx_pdus;
  int wifi_tx_pdus;
};

} // namespace srslte

#endif // ENB_PDCP_METRICS_H

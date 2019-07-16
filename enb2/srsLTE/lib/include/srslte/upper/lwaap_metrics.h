
#ifndef ENB_LWAAP_METRICS_H
#define ENB_LWAAP_METRICS_H


namespace srslte {

struct lwaap_metrics_t
{
  double dl_tput_bps;
  double ul_tput_bps;
  int    dl_tput_bits;
  int    ul_tput_bits;
};

} // namespace srslte

#endif // ENB_LWAAP_METRICS_H


#ifndef ENB_GTPU_METRICS_H
#define ENB_GTPU_METRICS_H


namespace srsenb {

struct gtpu_metrics_t
{
  double dl_tput_bps;
  double ul_tput_bps;
  int    dl_tput_bits;
  int    ul_tput_bits;
};

} // namespace srsenb

#endif // ENB_GTPU_METRICS_H

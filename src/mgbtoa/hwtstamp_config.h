#ifndef _HWTSTAMP_CONFIG_H_
#define _HWTSTAMP_CONFIG_H_

#define TX_TYPE_OFF    0
#define RX_FILTER_ALL  1

int hwtstamp_config_set(const char *ifname, int tx_type, int rx_filter);
int hwtstamp_config_get(const char *ifname, int *tx_type, int *rx_filter);

static int tx_type_save, rx_filter_save;

static inline int hwtstamp_config_save(const char *ifname)
{
	return hwtstamp_config_get(ifname, &tx_type_save, &rx_filter_save);
}

static inline int hwtstamp_config_restore(const char *ifname)
{
	return hwtstamp_config_set(ifname, tx_type_save, rx_filter_save);
}

static inline int hwtstamp_config_set_hwtstamp(const char *ifname)
{
	return hwtstamp_config_set(ifname, TX_TYPE_OFF, RX_FILTER_ALL);
}

#endif

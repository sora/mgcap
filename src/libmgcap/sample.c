#include <stdio.h>
#include "mgcap.h"

int main(int argc, char* argv[]) {
	mgcap_t *mg = new_mgcap();
	int rc = mgcap_set_device(mg, "enp0s3");	
	printf("mscap_set_device() = %d\n", rc);
	
	void *ptr;
	mgcap_hdr hdr;
	uint8_t *base, *p;
	
	while (0 <= (rc = mgcap_next(mg, &ptr, &hdr))) {
		printf("pktlen=%u, ts=%lu:", hdr.pktlen_, hdr.timestamp_);
		base = (uint8_t*)ptr;
		for (p = base; p - base < 32; p++) {
			printf(" %02X", *p);
		}
		printf("\n");
	}

	destroy_mgcap(mg);
}

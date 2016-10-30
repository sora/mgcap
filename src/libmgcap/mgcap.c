#include "mgcap.h"

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>

// #define MGC_SNAPLEN (10 + 96)
#define MGC_SNAPLEN (128)
#define MGCAP_ERR_LEN (256)

struct mgcap_ {
	int fd_;
	char* dev_path_;
	char* errmsg_;
	uint8_t* cursol_;
	uint8_t ibuf_[MGC_SNAPLEN * 1024];  // number of max input packets: 1024
	int read_size_;
};


mgcap_t* new_mgcap() {
	mgcap_t* mg = malloc(sizeof(mgcap_t));
	mg->fd_ = -1;
	mg->dev_path_ = NULL;
	mg->errmsg_ = calloc(MGCAP_ERR_LEN, sizeof(char));
	mg->cursol_ = NULL;
	return mg;
}

void destroy_mgcap(mgcap_t* mg) {
	assert(mg);

	if (mg->dev_path_) {
		free(mg->dev_path_);
	}
	
	if (mg->fd_ >= 0) {
		close(mg->fd_);
	}

	free(mg->errmsg_);
	free(mg);
}

int mgcap_set_device(mgcap_t* mg, const char* dev_name) {
	if (mg->dev_path_) {
		free(mg->dev_path_);
	}
	if (mg->fd_ != 0) {
		close(mg->fd_);
	}

	static const char* dev_prefix = "/dev/mgcap/";
	const int p_len = strlen(dev_prefix) + strlen(dev_name) + 1;
	mg->dev_path_ = calloc(p_len, sizeof(char));
	snprintf(mg->dev_path_, p_len, "%s%s", dev_prefix, dev_name);

	mg->fd_ = open(mg->dev_path_, O_RDONLY);
	// printf("%d\n", mg->fd_);
	if (mg->fd_ < 0) {
		strerror_r(errno, mg->errmsg_, MGCAP_ERR_LEN);
		return -1;
	}

	return 0;
}


int mgcap_next(mgcap_t* mg, void** pktptr, mgcap_hdr* hdr) {
	if (mg->cursol_ == NULL) {
		int rc;
		while (0 >= (rc = read(mg->fd_, &(mg->ibuf_[0]), sizeof(mg->ibuf_)))) {
			usleep(100);
		}

		mg->read_size_ = rc;
		printf("read: %d\n", mg->read_size_);
		
		if (mg->read_size_ <= 0) {
			strerror_r(errno, mg->errmsg_, MGCAP_ERR_LEN);
			return -1;			
		}
		mg->cursol_ = &(mg->ibuf_[0]);
		// printf("read size: %d (%d, %d)\n", mg->read_size_, mg->read_size_ / MGC_SNAPLEN,
		// mg->read_size_ % MGC_SNAPLEN);
	}
	
	memcpy(hdr, mg->cursol_, sizeof(mgcap_hdr));
	mg->cursol_ += sizeof(mgcap_hdr);
	*pktptr = mg->cursol_;
	mg->cursol_ += MGC_SNAPLEN - sizeof(mgcap_hdr);
 
	// mg->cursol_ += sizeof(uint16_t) + sizeof(uint32_t) + sizeof(uint32_t) + hdr->pktlen_;
	if (&(mg->ibuf_[0]) + mg->read_size_ <= mg->cursol_) {
		mg->cursol_ = NULL;
	}
	
	return 0;
}



/*****************************************************************************
 ****************************************************************************/
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <malloc.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netinet/in.h>
#include <assert.h>

#include "sock.h"

struct _sock {
	int    sd; /* base socket descriptor */
	int    cd; /* connected descriptor */
};

static void
_init_sock(sock_t s) {
	s->sd = s->cd = -1;
}

sock_t
sock_init(bool server, int port) {
	struct sockaddr_in     saddr;
	int                    sd; /* socket descriptor */
	sock_t  s = malloc(sizeof(struct _sock));
	if (!s) {
		printf("Out of memory\n");
		return NULL;
	}
	_init_sock(s);

	/* create socket */
	if (0 > (sd = socket(AF_INET, SOCK_STREAM, 0))) {
		printf("Fail to create socket\n"
		       "    %s\n",
		       strerror(errno));
		goto bail;
	}

	{ /* Just scope */
		struct hostent*        he;
		memset(&saddr, sizeof(saddr), 0);
		saddr.sin_family = AF_INET;
		saddr.sin_port = htons(port);
		he = gethostbyname("localhost");
		memcpy(&saddr.sin_addr.s_addr, he->h_addr, he->h_length);
	}

	if (server) {
		s->sd = sd;
		/*
		 * init server socket
		 * : bind -> listen -> accept
		 */
		if (0 > bind(s->sd, (struct sockaddr*)&saddr, sizeof(saddr))) {
			printf("Fail to binding socket to localhost\n"
			       "    %s\n",
			       strerror(errno));
			goto bail;
		}

		if (0 > listen(s->sd, 1)) {
			printf("Fail to listen socket\n"
			       "    %s\n",
			       strerror(errno));
			goto bail;
		}

		if (0 > (s->cd = accept(s->sd, NULL, NULL))) {
			printf("Fail to accept\n"
			       "    %s\n",
			       strerror(errno));
			goto bail;
		}
	} else {
		s->cd = sd;
		/*
		 * init client socket
		 * : connect
		 */
		if (connect(s->cd, (struct sockaddr*)&saddr, sizeof(saddr))) {
			printf("Fail to connect\n"
			       "    %s\n",
			       strerror(errno));
			goto bail;
		}
	}

	return s;

 bail:
	sock_deinit(s);
	return NULL;
}

int
sock_deinit(sock_t s) {
	if (s) {
		if(s->cd >= 0) {
			shutdown(s->cd, SHUT_RDWR);
			close(s->cd);
		}

		if(s->sd >= 0)
			close(s->sd);

		free(s);
	}
	return 0;
}

int
sock_recv(sock_t s, void* user,
          int (*rcv)(void*, unsigned char*, unsigned int)) {
	int              ret = 0;
	unsigned char*   rcvb = NULL; /* receive buffer */

	assert(s && rcv);

#define __check_recv()							\
	do {								\
		if (br < 0) {						\
			printf("Fail to receive from accepted socket\n"	\
			       "    %s\n", strerror(errno));		\
			ret = -1;					\
			goto done;					\
		} else if (!br) {					\
			/* peer performs shudown */			\
			ret = 0;					\
			goto done;					\
		}							\
	} while (0)

	/*
	 * Protocal
	 * [size(4bytes)][data...]
	 * @size : sizeof pure data. (exclude self)
	 */
	while (1) {
		uint32_t sz;
		int      br; /* bytes received */
		rcvb = NULL;
		br = recv(s->cd, &sz, 4, 0);
		__check_recv();
		if (br < 4) {
			printf("Unsupported protocoal!\n");
			ret = -1;
			break;
		}

		sz = ntohl(sz);
		rcvb = malloc(sz);
		if (!rcvb) {
			printf("Out of memory\n");
			ret = -1;
			break;
		}

		br = 0;
		while (sz - br > 0) {
			br += recv(s->cd, rcvb + br, sz - br, 0);
			__check_recv();
		}

		if (0 > (*rcv)(user, rcvb, sz)) {
			/*
			 * 'd' is alread passed to 'rcv'.
			 * Prevent d from duplicated 'free'
			 */
			rcvb = NULL;
			/* end of communication */
			ret = 0;
			break;
		}
	}

#undef __check_recv

 done:
	if (rcvb)
		free(rcvb);
	return ret;
}

int
sock_send(sock_t s, unsigned char* b, unsigned int bsz) {
	uint32_t     nsz; /* network value of bsz */
	unsigned int bs = 0;
	nsz = htonl(bsz);
	if (0 > send(s->cd, &nsz, sizeof(uint32_t), 0))
		/* fail to send */
		return -1;

	while(bs < bsz)
		bs += send(s->cd, b+bs, bsz-bs, 0);

	return bsz;
}

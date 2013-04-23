/*****************************************************************************
 * ppmap : Port Pipe MAPer
 ****************************************************************************/

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>

#include "sock.h"

/* #define DEBUG */

#ifdef DEBUG
#define dpr(a, x...) printf(a, ##x)
#else /* DEBUG */
#define dpr(a, x...)
#endif /* DEBUG */

struct _thdarg {
	sock_t      s;
	const char* pipe; /* pipe path */
};


static bool rdtask_alive = true;
static bool wrtask_alive = true;

#ifdef ANDROID_NDK
/* android ndk doesn't support pthread_cancel */
static void pthread_cancel(pthread_t thd) {}
#endif /* ANDROID_NDK */

static int
_write_to_pipe(void* user, unsigned char* data, unsigned int sz) {
	int      pd = (int)(long)user;
	if (sz != write(pd, data, sz)) {
		printf("Fail to write to pipe\n");
		return -1;
	}
	free(data);
	dpr("--- write to pipe : %d bytes\n", sz);
	return 0;
}

static void*
_read_pipe_task(void* arg) {
#define BUFSZ (1024 * 1024)
	struct _thdarg* ta = arg;
	int             pd; /* pipe descriptor */
	unsigned char*  buf = NULL;
	ssize_t         br;

	if ( -1 == (pd = open(ta->pipe, O_RDONLY))) {
		printf("Fail to open client's write pipe.\n");
		goto done;
	}

	buf = malloc(BUFSZ); /* 1M buffer */
	if (!buf) {
		printf("Out of memory.\n");
		goto done;
	}

	while (true) {
		br = read(pd, buf, BUFSZ);
		dpr("--- read from pipe : %ld bytes\n", br);
		if (0 == br) {
			/* EOF */
			close(pd);
			/*
			if (-1 == (pd = open(ta->pipe, O_RDONLY))) {
				printf("Fail to open client's write pipe.\n");
				goto done;
			}
			continue;
			*/
			break;
		} else if (-1 == br) {
			printf("Fail to read from pipe\n"
			       "    %s\n",
			       strerror(errno));
			break;
		}

		if (br != sock_send(ta->s, buf, br)) {
			printf("Fail to send through socket\n");
			break;
		}
		dpr("--- sent to socket...\n");
	}

 done:
	if (buf)
		free(buf);

	rdtask_alive = false;
	return NULL;
#undef BUFSZ
}


static void*
_write_pipe_task(void* arg) {
	struct _thdarg* ta = arg;
	int             pd; /* pipe descriptor */

	pd = open(ta->pipe, O_WRONLY);
	if (-1 == pd) {
		printf("Fail to open client's read pipe.\n");
		goto done;
	}

	sock_recv(ta->s, (void*)(long)pd, &_write_to_pipe);

 done:
	wrtask_alive = false;
	return NULL;
}

static void _usage(void) __attribute__ ((noreturn));
static void
_usage(void) {
	printf("ppmap <type> <port> <rd pipe> <wr pipe>\n"
	       "    type : operation type. Can be one of [s, c]\n"
	       "        s : server\n"
	       "        c : client\n"
	       "    rd pipe : pipe for client to read from port\n"
	       "    wr pipe : pipe for client to write to port\n");
	exit(2);
}

int
main(int argc, const char* argv[]) {
	bool     server;
	int      port;
	sock_t   s = NULL;

	pthread_t       rdthd = 0;
	struct _thdarg  rdarg;
	pthread_t       wrthd = 0;
	struct _thdarg  wrarg;

	if (5 != argc)
		_usage();

	if (!strcmp(argv[1], "s"))
		server = true;
	else if (!strcmp(argv[1], "c"))
		server = false;
	else {
		printf("Invalid operation type.\n");
		_usage();
	}

	port = atoi(argv[2]);
	if (port <= 0 || port > 65535) {
		printf("Invalid port number.\n");
		return 1;
	}

	if (!(s = sock_init(server, port))) {
		printf("Fail to initialize socket.\n");
		goto bail;
	}

	rdtask_alive = wrtask_alive = true;

	rdarg.s = s;
	rdarg.pipe = argv[4]; /* write pipe (read for ppmap) */
	if (pthread_create(&rdthd, NULL, &_read_pipe_task, &rdarg)) {
		printf("Fail to start thread\n");
		goto bail;
	}

	dpr(">>> read task thread created\n");

	wrarg.s = s;
	wrarg.pipe = argv[3]; /* read pipe (write for ppmap) */
	if (pthread_create(&wrthd, NULL, &_write_pipe_task, &wrarg)) {
		printf("Fail to start thread\n");
		goto bail;
	}

	dpr(">>> write task thread created\n");

	while (rdtask_alive && wrtask_alive);
		sleep(1);

	if (rdtask_alive)
		pthread_cancel(rdthd);
	if (wrtask_alive)
		pthread_cancel(wrthd);

	return 0;

 bail:
	if (rdthd)
		pthread_cancel(rdthd);
	if (wrthd)
		pthread_cancel(wrthd);
	return 1;
}

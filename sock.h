/*****************************************************************************
 ****************************************************************************/
#ifndef __SOCk_h__
#define __SOCk_h__

struct _sock;
typedef struct _sock* sock_t;

sock_t sock_init(bool server, int port);
int    sock_deinit(sock_t s);
int    sock_recv(sock_t s, void* user,
		 int (*rcv)(void*, unsigned char*, unsigned int));
int    sock_send(sock_t s, unsigned char* b, unsigned int bsz);

#endif /* __SOCk_h__ */

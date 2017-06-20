/*
 * Copyright (c) 2005 Topspin Communications.  All rights reserved.
 * Copyright (c) 2008 Intel Corporation.  All rights reserved.
 *
 * This software is available to you under the OpenIB.org BSD license
 * below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AWV
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */


#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <malloc.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <infiniband/verbs.h>
#include <sys/socket.h>
#include <sys/time.h>
// Interprocess communication access
#include <sys/ipc.h>
// Shared memory facility
#include <sys/shm.h>

enum {
	PINGPONG_RECV_WRID = 1,
	PINGPONG_SEND_WRID = 2,
};

struct pingpong_context {
	struct ibv_context		*context;
	struct ibv_comp_channel *channel;
	struct ibv_pd			*pd;
	struct ibv_mr			*mr;
	struct ibv_cq			*cq;
	struct ibv_qp			*qp;
	struct ibv_ah			*ah;
	void					*buf;
	int						size;
	int						rx_depth;
	int						pending;
};

struct pingpong_dest {
	int lid;
	int qpn;
	int psn;
};

static int pp_connect_ctx(struct pingpong_context *ctx, int port, int my_psn,
			  int sl, struct pingpong_dest *dest)
{
	struct ibv_ah_attr ah_attr;
	struct ibv_qp_attr attr;

	memset(&ah_attr, 0, sizeof ah_attr);
	ah_attr.is_global     = 0;
	ah_attr.dlid          = (uint16_t) dest->lid;
	ah_attr.sl            = (uint8_t) sl;
	ah_attr.src_path_bits = 0;
	ah_attr.port_num      = (uint8_t) port;

	attr.qp_state = IBV_QPS_RTR;

	if (ibv_modify_qp(ctx->qp, &attr, IBV_QP_STATE)) {
		fprintf(stderr, "Failed to modify QP to RTR\n");
		return 1;
	}

	attr.qp_state = IBV_QPS_RTS;
	attr.sq_psn	  = my_psn;

	if (ibv_modify_qp(ctx->qp, &attr,
			  IBV_QP_STATE | IBV_QP_SQ_PSN)) {
		fprintf(stderr, "Failed to modify QP to RTS\n");
		return 1;
	}

	ctx->ah = ibv_create_ah(ctx->pd, &ah_attr);
	if (!ctx->ah) {
		fprintf(stderr, "Failed to create AH\n");
		return 1;
	}

	return 0;
}

static struct pingpong_dest *pp_client_exch_dest(const char *servername, int port,
						 const struct pingpong_dest *my_dest)
{
	struct addrinfo *res, *t;
	struct addrinfo hints;
	char service[6];
	char msg[sizeof "0000:000000:000000"];
	int n;
	SOCKET sockfd = INVALID_SOCKET;
	struct pingpong_dest *rem_dest = NULL;

	memset(&hints, 0, sizeof hints);
	hints.ai_family   = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	sprintf(service, "%d\0", port);

	n = getaddrinfo(servername, service, &hints, &res);

	if (n != 0) {
		fprintf(stderr, "%s for %s:%d\n", gai_strerror(n), servername, port);
		return NULL;
	}

	for (t = res; t; t = t->ai_next) {
		sockfd = socket(t->ai_family, t->ai_socktype, t->ai_protocol);
		if (sockfd != INVALID_SOCKET) {
			if (!connect(sockfd, t->ai_addr, t->ai_addrlen))
				break;
			closesocket(sockfd);
			sockfd = INVALID_SOCKET;
		}
	}

	freeaddrinfo(res);

	if (sockfd == INVALID_SOCKET) {
		fprintf(stderr, "Couldn't connect to %s:%d\n", servername, port);
		return NULL;
	}

	sprintf(msg, "%04x:%06x:%06x", my_dest->lid, my_dest->qpn, my_dest->psn);
	if (send(sockfd, msg, sizeof msg, 0) != sizeof msg) {
		fprintf(stderr, "Couldn't send local address\n");
		goto out;
	}

	if (recv(sockfd, msg, sizeof msg, 0) != sizeof msg) {
		perror("client recv");
		fprintf(stderr, "Couldn't recv remote address\n");
		goto out;
	}

	send(sockfd, "done", sizeof "done", 0);

	rem_dest = malloc(sizeof *rem_dest);
	if (!rem_dest)
		goto out;

	sscanf(msg, "%x:%x:%x", &rem_dest->lid, &rem_dest->qpn, &rem_dest->psn);

out:
	closesocket(sockfd);
	return rem_dest;
}

static struct pingpong_dest *pp_server_exch_dest(struct pingpong_context *ctx,
						 int ib_port, int port, int sl,
						 const struct pingpong_dest *my_dest)
{
	struct addrinfo *res, *t;
	struct addrinfo hints;
	char service[6];
	char msg[sizeof "0000:000000:000000"];
	int n;
	SOCKET sockfd = INVALID_SOCKET, connfd;
	struct pingpong_dest *rem_dest = NULL;

	memset(&hints, 0, sizeof hints);
	hints.ai_flags    = AI_PASSIVE;
	hints.ai_family   = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	sprintf(service, "%d\0", port);

	n = getaddrinfo(NULL, service, &hints, &res);

	if (n != 0) {
		fprintf(stderr, "%s for port %d\n", gai_strerror(n), port);
		return NULL;
	}

	for (t = res; t; t = t->ai_next) {
		sockfd = socket(t->ai_family, t->ai_socktype, t->ai_protocol);
		if (sockfd != INVALID_SOCKET) {
			n = 0;
			setsockopt(sockfd, IPPROTO_IPV6, IPV6_V6ONLY, (char *) &n, sizeof n);
			n = 1;
			setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char *) &n, sizeof n);

			if (!bind(sockfd, t->ai_addr, t->ai_addrlen))
				break;
			closesocket(sockfd);
			sockfd = INVALID_SOCKET;
		}
	}

	freeaddrinfo(res);

	if (sockfd == INVALID_SOCKET) {
		fprintf(stderr, "Couldn't listen to port %d\n", port);
		return NULL;
	}

	listen(sockfd, 1);
	connfd = accept(sockfd, NULL, 0);
	closesocket(sockfd);
	if (connfd == INVALID_SOCKET) {
		fprintf(stderr, "accept() failed\n");
		return NULL;
	}

	n = recv(connfd, msg, sizeof msg, 0);
	if (n != sizeof msg) {
		perror("server recv");
		fprintf(stderr, "%d/%d: Couldn't recv remote address\n", n, (int) sizeof msg);
		goto out;
	}

	rem_dest = malloc(sizeof *rem_dest);
	if (!rem_dest)
		goto out;

	sscanf(msg, "%x:%x:%x", &rem_dest->lid, &rem_dest->qpn, &rem_dest->psn);

	if (pp_connect_ctx(ctx, ib_port, my_dest->psn, sl, rem_dest)) {
		fprintf(stderr, "Couldn't connect to remote QP\n");
		free(rem_dest);
		rem_dest = NULL;
		goto out;
	}

	sprintf(msg, "%04x:%06x:%06x", my_dest->lid, my_dest->qpn, my_dest->psn);
	if (send(connfd, msg, sizeof msg, 0) != sizeof msg) {
		fprintf(stderr, "Couldn't send local address\n");
		free(rem_dest);
		rem_dest = NULL;
		goto out;
	}

	recv(connfd, msg, sizeof msg, 0);

out:
	closesocket(connfd);
	return rem_dest;
}

static struct pingpong_context *pp_init_ctx(struct ibv_device *ib_dev, int size,
					    int rx_depth, int port,
					    int use_event)
{
	struct pingpong_context *ctx;

	ctx = malloc(sizeof *ctx);
	if (!ctx)
		return NULL;

	ctx->size     = size;
	ctx->rx_depth = rx_depth;

	ctx->buf = malloc(size + 40);
	if (!ctx->buf) {
		fprintf(stderr, "Couldn't allocate work buf.\n");
		return NULL;
	}

	memset(ctx->buf, 0, size + 40);

	ctx->context = ibv_open_device(ib_dev);
	if (!ctx->context) {
		fprintf(stderr, "Couldn't get context for %s\n",
			ibv_get_device_name(ib_dev));
		return NULL;
	}

	if (use_event) {
		ctx->channel = ibv_create_comp_channel(ctx->context);
		if (!ctx->channel) {
			fprintf(stderr, "Couldn't create completion channel\n");
			return NULL;
		}
	} else
		ctx->channel = NULL;

	ctx->pd = ibv_alloc_pd(ctx->context);
	if (!ctx->pd) {
		fprintf(stderr, "Couldn't allocate PD\n");
		return NULL;
	}

	ctx->mr = ibv_reg_mr(ctx->pd, ctx->buf, size + 40, IBV_ACCESS_LOCAL_WRITE);
	if (!ctx->mr) {
		fprintf(stderr, "Couldn't register MR\n");
		return NULL;
	}

	ctx->cq = ibv_create_cq(ctx->context, rx_depth + 1, NULL,
				ctx->channel, 0);
	if (!ctx->cq) {
		fprintf(stderr, "Couldn't create CQ\n");
		return NULL;
	}

	{
		struct ibv_qp_init_attr attr;

		memset(&attr, 0, sizeof attr);
		attr.send_cq = ctx->cq;
		attr.recv_cq = ctx->cq;
		attr.cap.max_send_wr  = 1;
		attr.cap.max_recv_wr  = rx_depth;
		attr.cap.max_send_sge = 1;
		attr.cap.max_recv_sge = 1;
		attr.qp_type = IBV_QPT_UD,

		ctx->qp = ibv_create_qp(ctx->pd, &attr);
		if (!ctx->qp)  {
			fprintf(stderr, "Couldn't create QP\n");
			return NULL;
		}
	}

	{
		struct ibv_qp_attr attr;
		
		memset(&attr, 0, sizeof attr);
		attr.qp_state        = IBV_QPS_INIT;
		attr.pkey_index      = 0;
		attr.port_num        = (uint8_t) port;
		attr.qkey            = 0x11111111;

		if (ibv_modify_qp(ctx->qp, &attr,
				  IBV_QP_STATE              |
				  IBV_QP_PKEY_INDEX         |
				  IBV_QP_PORT               |
				  IBV_QP_QKEY)) {
			fprintf(stderr, "Failed to modify QP to INIT\n");
			return NULL;
		}
	}

	return ctx;
}

int pp_close_ctx(struct pingpong_context *ctx)
{
	if (ibv_destroy_qp(ctx->qp)) {
		fprintf(stderr, "Couldn't destroy QP\n");
		return 1;
	}

	if (ibv_destroy_cq(ctx->cq)) {
		fprintf(stderr, "Couldn't destroy CQ\n");
		return 1;
	}

	if (ibv_dereg_mr(ctx->mr)) {
		fprintf(stderr, "Couldn't deregister MR\n");
		return 1;
	}

	if (ibv_destroy_ah(ctx->ah)) {
		fprintf(stderr, "Couldn't destroy AH\n");
		return 1;
	}

	if (ibv_dealloc_pd(ctx->pd)) {
		fprintf(stderr, "Couldn't deallocate PD\n");
		return 1;
	}

	if (ctx->channel) {
		if (ibv_destroy_comp_channel(ctx->channel)) {
			fprintf(stderr, "Couldn't destroy completion channel\n");
			return 1;
		}
	}

	if (ibv_close_device(ctx->context)) {
		fprintf(stderr, "Couldn't release context\n");
		return 1;
	}

	free(ctx->buf);
	free(ctx);

	return 0;
}

static int pp_post_recv(struct pingpong_context *ctx, int n)
{
	struct ibv_sge list;
	struct ibv_recv_wr wr;
	struct ibv_recv_wr *bad_wr;
	int i;

	list.addr	= (uintptr_t) ctx->buf;
	list.length = ctx->size + 40;
	list.lkey	= ctx->mr->lkey;
	wr.next		= NULL;
	wr.wr_id	= PINGPONG_RECV_WRID;
	wr.sg_list  = &list;
	wr.num_sge  = 1;

	for (i = 0; i < n; ++i)
		if (ibv_post_recv(ctx->qp, &wr, &bad_wr))
			break;

	return i;
}

static int pp_post_send(struct pingpong_context *ctx, uint32_t qpn)
{
	struct ibv_sge list;
	struct ibv_send_wr wr;
	struct ibv_send_wr *bad_wr;

	list.addr	  = (uintptr_t) ctx->buf + 40;
	list.length   = ctx->size;
	list.lkey	  = ctx->mr->lkey;
	wr.next		  = NULL;
	wr.wr_id	  = PINGPONG_SEND_WRID;
	wr.sg_list    = &list;
	wr.num_sge    = 1;
	wr.opcode     = IBV_WR_SEND;
	wr.send_flags = IBV_SEND_SIGNALED;
	wr.wr.ud.ah			 = ctx->ah;
	wr.wr.ud.remote_qpn  = qpn;
	wr.wr.ud.remote_qkey = 0x11111111;

	return ibv_post_send(ctx->qp, &wr, &bad_wr);
}

static void usage(const char *argv0)
{
	printf("Usage:\n");
	printf("  %s            start a server and wait for connection\n", argv0);
	printf("  %s -h <host>  connect to server at <host>\n", argv0);
	printf("\n");
	printf("Options:\n");
	printf("  -p <port>     listen on/connect to port <port> (default 18515)\n");
	printf("  -d <dev>      use IB device <dev> (default first device found)\n");
	printf("  -i <port>     use port <port> of IB device (default 1)\n");
	printf("  -s <size>     size of message to exchange (default 2048)\n");
	printf("  -r <dep>      number of receives to post at a time (default 500)\n");
	printf("  -n <iters>    number of exchanges (default 1000)\n");
	printf("  -e            sleep on CQ events (default poll)\n");
}

int __cdecl main(int argc, char *argv[])
{
	struct ibv_device		**dev_list;
	struct ibv_device		*ib_dev;
	struct pingpong_context *ctx;
	struct pingpong_dest    my_dest;
	struct pingpong_dest    *rem_dest;
	LARGE_INTEGER			start, end, freq;
	char                    *ib_devname = NULL;
	char                    *servername = NULL;
	int                     port = 18515;
	int                     ib_port = 1;
	int                     size = 2048;
	int                     rx_depth = 500;
	int                     iters = 1000;
	int                     use_event = 0;
	int                     routs;
	int                     rcnt, scnt;
	int                     num_cq_events = 0;
	int                     sl = 0;
	WORD					version;
	WSADATA					data;
	int						err;

	srand((unsigned int) time(NULL));
	version = MAKEWORD(2, 2);
	err = WSAStartup(version, &data);
	if (err)
		return -1;

	while (1) {
		int c;

		c = getopt(argc, argv, "h:p:d:i:s:r:n:l:e");
		if (c == -1)
			break;

		switch (c) {
		case 'p':
			port = strtol(optarg, NULL, 0);
			if (port < 0 || port > 65535) {
				usage(argv[0]);
				return 1;
			}
			break;

		case 'd':
			ib_devname = _strdup(optarg);
			break;

		case 'i':
			ib_port = strtol(optarg, NULL, 0);
			if (ib_port < 0) {
				usage(argv[0]);
				return 1;
			}
			break;

		case 's':
			size = strtol(optarg, NULL, 0);
			break;

		case 'r':
			rx_depth = strtol(optarg, NULL, 0);
			break;

		case 'n':
			iters = strtol(optarg, NULL, 0);
			break;

		case 'l':
			sl = strtol(optarg, NULL, 0);
			break;

		case 'e':
			++use_event;
			break;

		case 'h':
			servername = _strdup(optarg);
			break;

		default:
			usage(argv[0]);
			return 1;
		}
	}

	dev_list = ibv_get_device_list(NULL);
	if (!dev_list) {
		fprintf(stderr, "No IB devices found\n");
		return 1;
	}

	if (!ib_devname) {
		ib_dev = *dev_list;
		if (!ib_dev) {
			fprintf(stderr, "No IB devices found\n");
			return 1;
		}
	} else {
		int i;
		for (i = 0; dev_list[i]; ++i)
			if (!strcmp(ibv_get_device_name(dev_list[i]), ib_devname))
				break;
		ib_dev = dev_list[i];
		if (!ib_dev) {
			fprintf(stderr, "IB device %s not found\n", ib_devname);
			return 1;
		}
	}

	ctx = pp_init_ctx(ib_dev, size, rx_depth, ib_port, use_event);
	if (!ctx)
		return 1;

	routs = pp_post_recv(ctx, ctx->rx_depth);
	if (routs < ctx->rx_depth) {
		fprintf(stderr, "Couldn't post receive (%d)\n", routs);
		return 1;
	}

	if (use_event)
		if (ibv_req_notify_cq(ctx->cq, 0)) {
			fprintf(stderr, "Couldn't request CQ notification\n");
			return 1;
		}

	my_dest.lid = pp_get_local_lid(ctx->context, ib_port);
	my_dest.qpn = ctx->qp->qp_num;
	my_dest.psn = rand() & 0xffffff;
	if (!my_dest.lid) {
		fprintf(stderr, "Couldn't get local LID\n");
		return 1;
	}

	printf("  local address:  LID 0x%04x, QPN 0x%06x, PSN 0x%06x\n",
	       my_dest.lid, my_dest.qpn, my_dest.psn);

	if (servername)
		rem_dest = pp_client_exch_dest(servername, port, &my_dest);
	else
		rem_dest = pp_server_exch_dest(ctx, ib_port, port, sl, &my_dest);

	if (!rem_dest)
		return 1;

	printf("  remote address: LID 0x%04x, QPN 0x%06x, PSN 0x%06x\n",
	       rem_dest->lid, rem_dest->qpn, rem_dest->psn);

	if (servername)
		if (pp_connect_ctx(ctx, ib_port, my_dest.psn, sl, rem_dest))
			return 1;

	ctx->pending = PINGPONG_RECV_WRID;

	if (servername) {
		if (pp_post_send(ctx, rem_dest->qpn)) {
			fprintf(stderr, "Couldn't post send\n");
			return 1;
		}
		ctx->pending |= PINGPONG_SEND_WRID;
	}

	if (!QueryPerformanceCounter(&start)) {
		perror("QueryPerformanceCounter");
		return 1;
	}

	rcnt = scnt = 0;
	while (rcnt < iters || scnt < iters) {
		if (use_event) {
			struct ibv_cq *ev_cq;
			void          *ev_ctx;

			if (ibv_get_cq_event(ctx->channel, &ev_cq, &ev_ctx)) {
				fprintf(stderr, "Failed to get cq_event\n");
				return 1;
			}

			++num_cq_events;

			if (ev_cq != ctx->cq) {
				fprintf(stderr, "CQ event for unknown CQ %p\n", ev_cq);
				return 1;
			}

			if (ibv_req_notify_cq(ctx->cq, 0)) {
				fprintf(stderr, "Couldn't request CQ notification\n");
				return 1;
			}
		}

		{
			struct ibv_wc wc[2];
			int ne, i;

			do {
				ne = ibv_poll_cq(ctx->cq, 2, wc);
				if (ne < 0) {
					fprintf(stderr, "poll CQ failed %d\n", ne);
					return 1;
				}
			} while (!use_event && ne < 1);

			for (i = 0; i < ne; ++i) {
				if (wc[i].status != IBV_WC_SUCCESS) {
					fprintf(stderr, "Failed status %s (%d) for wr_id %d\n",
						ibv_wc_status_str(wc[i].status),
						wc[i].status, (int) wc[i].wr_id);
					return 1;
				}

				switch ((int) wc[i].wr_id) {
				case PINGPONG_SEND_WRID:
					++scnt;
					break;

				case PINGPONG_RECV_WRID:
					if (--routs <= 1) {
						routs += pp_post_recv(ctx, ctx->rx_depth - routs);
						if (routs < ctx->rx_depth) {
							fprintf(stderr,
								"Couldn't post receive (%d)\n",
								routs);
							return 1;
						}
					}

					++rcnt;
					break;

				default:
					fprintf(stderr, "Completion for unknown wr_id %d\n",
						(int) wc[i].wr_id);
					return 1;
				}

				ctx->pending &= ~(int) wc[i].wr_id;
				if (scnt < iters && !ctx->pending) {
					if (pp_post_send(ctx, rem_dest->qpn)) {
						fprintf(stderr, "Couldn't post send\n");
						return 1;
					}
					ctx->pending = PINGPONG_RECV_WRID |
						       PINGPONG_SEND_WRID;
				}
			}
		}
	}

	if (!QueryPerformanceCounter(&end) ||
		!QueryPerformanceFrequency(&freq)) {
		perror("QueryPerformanceCounter/Frequency");
		return 1;
	}

	{
		double sec = (double) (end.QuadPart - start.QuadPart) / (double) freq.QuadPart;
		long long bytes = (long long) size * iters * 2;

		printf("%I64d bytes in %.2f seconds = %.2f Mbit/sec\n",
		       bytes, sec, bytes * 8. / 1000000. / sec);
		printf("%d iters in %.2f seconds = %.2f usec/iter\n",
		       iters, sec, sec * 1000000. / iters);
	}

	ibv_ack_cq_events(ctx->cq, num_cq_events);

	if (pp_close_ctx(ctx))
		return 1;

	ibv_free_device_list(dev_list);
	free(rem_dest);

	return 0;
}

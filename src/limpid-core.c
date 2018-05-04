/******************************************************************************

                  Copyright (c) 2017 Siddharth Chandrasekaran

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.

    Author : Siddharth Chandrasekaran
    Email  : siddharth@embedjournal.com
    Date   : Thu Oct 19 06:02:01 IST 2017

******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

#include <limpid.h>

#define is_char(x) (((x) >= 'A' && (x) <= 'Z') || ((x) >= 'a' && (x) <='z'))
#define to_lower(x) do { if (is_char(x)) { x |= 0x20 } } while(0)

typedef int (*processor_t)(lchunk_t *, lchunk_t **);

// CLI Handler
int limpid_register_cli_handle(const char *trigger, int (*handler)(int, char **, string_t **));
int limpid_process_cli_cmd(lchunk_t *cmd, lchunk_t **resp);

processor_t processor[5] = {
	limpid_process_cli_cmd,
	NULL,
};

void say(const char *fmt, ...)
{
	va_list arg;
	fprintf(stderr, "Limpid: ");
	va_start(arg, fmt);
	vfprintf(stderr, fmt, arg);
	va_end(arg);
}

static void *limpid_listener(void *arg)
{
	struct sockaddr_un sock_serv, cli_addr;

	assert(arg);

	limpid_t *ctx = (limpid_t *) arg;
	if ((ctx->fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
		say("failed to created server fd\n");
		return NULL;
	}

	sock_serv.sun_family = AF_UNIX;
	strcpy(sock_serv.sun_path, ctx->path);
	unlink(ctx->path);
	socklen_t len = sizeof(sock_serv.sun_family) + strlen(ctx->path);

	if (bind(ctx->fd, (const struct sockaddr *)&sock_serv, len) < 0) {
		say("failed at bind!\n");
		return NULL;
	}
	if (listen(ctx->fd, 5) < 0) {
		say("failed at listen!\n");
		return NULL;
	}
	while (1) {
		ctx->client_fd = accept(ctx->fd, (struct sockaddr *)&cli_addr, &len);
		if (ctx->client_fd < 0) {
			say("failed at accept\n");
			continue;
		}
		printf("limpid: accepted connection from a clinet\n");
		lchunk_t *cmd=NULL, *resp=NULL;
		lchunk_t *err = limpid_make_chunk(TYPE_RESPONSE, NULL, NULL, 0);
		if (limpid_receive(ctx, &cmd) == 0) {
			if (cmd->type >= LHANDLE_CLI && cmd->type <= LHANDLE_SENTINEL)
				processor[cmd->type](cmd, &resp);
			free(cmd);
		}
		limpid_send(ctx, resp ? resp : err);
		if (resp) free(resp);
		free(err);
		close(ctx->client_fd);
		printf("limpid: closed connection to client\n");
	}
	return NULL;
}

limpid_t *limpid_server_init(const char *path)
{
	limpid_t *ctx = malloc(sizeof(limpid_t));
	assert(ctx);

	ctx->type = LIMPID_SERVER;
	ctx->path = strdup(path);
	pthread_t server_thread;
	pthread_create(&server_thread, NULL, limpid_listener, (void *)ctx);
	return ctx;
}

// Client side

limpid_t *limpid_connnect(const char *path)
{
	socklen_t sock_len;
	struct sockaddr_un serv_addr;

	limpid_t *ctx = malloc(sizeof(limpid_t));

	assert(ctx);
	assert(path);

	ctx->type = LIMPID_CLIENT;
	ctx->path = NULL;

	if ((ctx->fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
		free(ctx);
		return NULL;
	}

	serv_addr.sun_family = AF_UNIX;
	strcpy(serv_addr.sun_path, path);

	sock_len = sizeof(serv_addr.sun_family) + strlen(serv_addr.sun_path);
	if (connect(ctx->fd, (const struct sockaddr *)&serv_addr, sock_len) != 0) {
		fprintf(stderr, "limpid: failed to connect to server\n");
		close(ctx->fd);
		free(ctx);
		exit(EXIT_FAILURE);
	}
	ctx->path = strdup(path);
	return ctx;
}

int limpid_send(limpid_t *ctx, lchunk_t *c)
{
	int ret = 0, fd, len;

	assert(ctx);
	assert(c);

	len = sizeof(lchunk_t) + c->length;
	fd = (ctx->type == LIMPID_SERVER) ? ctx->client_fd : ctx->fd;
	if ((ret = write(fd, c, len)) < 0) {
		say("Error at write");
	}

	return ret;
}

int limpid_receive(limpid_t *ctx, lchunk_t **c)
{
	int fd;
	volatile int read1, read2=-1;

	assert(ctx);

	fd = (ctx->type == LIMPID_SERVER) ? ctx->client_fd : ctx->fd;
	do {
		// Prevent partial reads by waiting for data
		// to stabilize.
		ioctl(fd, FIONREAD, &read1);
		if (read1 > 0) {
			usleep(100);
			ioctl(fd, FIONREAD, &read2);
		} else {
			usleep(10);
		}
	} while (read1 != read2);

	void *read_data = malloc(read1);
	if (read_data == NULL) {
		say("Alloc error!\n");
		return -1;
	}

	if ((read(fd, read_data, read1)) < 0) {
		say("Error at read\n");
		return -1;
	}
	*c = (lchunk_t *) read_data;
	return 0;
}

void limpid_disconnect(limpid_t *ctx)
{
	close(ctx->fd);
	if (ctx->path)
		free(ctx->path);
	free(ctx);
}

lchunk_t *limpid_make_chunk(int type, const char *trigger, void *data, int len)
{
	int i;

	lchunk_t *c = calloc(1, sizeof(lchunk_t)+len);
	assert (c != NULL);

	c->type = type;
	if (trigger)
		strncpy(c->trigger, trigger, LIMPID_TRIGGER_MAXLEN-1);
	c->length = len;
	for (i=0; i<len; i++) {
		c->data[i] = *((uint8_t *)data + i);
	}
	return c;
}

void limpid_register(lhandle_t *h)
{
	switch (h->type) {
	case LHANDLE_CLI:
		limpid_register_cli_handle(h->trigger, h->cli_handle);
		break;
	default:
		break;
	}
}

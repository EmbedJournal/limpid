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

#ifndef _LIMPID_H
#define _LIMPID_H

#include <stdint.h>

#include <limpid/common.h>
#include <limpid/config.h>

enum lchunk_type_e {
	TYPE_COMMAND,
	TYPE_RESPONSE,
	TYPE_COMPLETION,
};

typedef struct {
	int type;       // see below
	int fd;         // client or server fd
	int client_fd;  // write-to-fd for server
	char *path;     // path to socket.
} limpid_t;

// limpid_t::type
#define LIMPID_SERVER  0
#define LIMPID_CLIENT  1

typedef struct {
	int type;
	char trigger[LIMPID_TRIGGER_MAXLEN];
	int length;
	uint8_t data[0];
} lchunk_t;

limpid_t *limpid_connnect(const char *path);
void limpid_disconnect(limpid_t *ctx);

lchunk_t *limpid_make_chunk(int type, const char *trigger, void *data, int len);

int limpid_send(limpid_t *ctx, lchunk_t *c);
int limpid_receive(limpid_t *ctx, lchunk_t **c);

#endif

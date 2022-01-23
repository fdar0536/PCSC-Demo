/*
 * Copyright (c) 2022 fdar0536
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#pragma once

#include <inttypes.h>

#include "winscard.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct PCSC
{
    SCARDCONTEXT ctx;

    SCARDHANDLE cardHandle;

    const SCARD_IO_REQUEST *protocol;
} PCSC;

uint8_t PCSC_init(PCSC *);

void PCSC_fin(PCSC *);

uint8_t PCSC_connectToCard(PCSC *, const char *);

void PCSC_disconnectToCard(PCSC *);

char *PCSC_readerList(PCSC *);

uint8_t PCSC_ready(PCSC *);

uint8_t PCSC_transmit(PCSC *, uint8_t *, size_t, uint8_t *, size_t *);

#ifdef __cplusplus
}
#endif

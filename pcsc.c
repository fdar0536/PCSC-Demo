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

#include <stdio.h>

#include "pcsc.h"

#ifndef _WIN32
#define SCardConnectA     SCardConnect
#define SCardListReadersA SCardListReaders
#define SCardStatusA      SCardStatus
#endif

static uint8_t PCSC_checkCTX(PCSC *in)
{
    if (!in) return 1;

    if (SCardIsValidContext(in->ctx) != SCARD_S_SUCCESS)
    {
        SCardReleaseContext(in->ctx);
        LONG rv = SCardEstablishContext(SCARD_SCOPE_SYSTEM,
                                        NULL,
                                        NULL,
                                        &in->ctx);
        if (rv != SCARD_S_SUCCESS)
        {
            fprintf(stderr, "SCardEstablishContext failed: 0x%lX\n", rv);
            return 1;
        }
    }

    return 0;
}

uint8_t PCSC_init(PCSC *in)
{
    if (!in) return 1;
    memset(in, 0, sizeof(PCSC));

    LONG rv = SCardEstablishContext(SCARD_SCOPE_SYSTEM,
                                    NULL,
                                    NULL,
                                    &in->ctx);
    if (rv != SCARD_S_SUCCESS)
    {
        fprintf(stderr, "SCardEstablishContext failed: 0x%lX\n", rv);
        return 1;
    }

    return 0;
}

void PCSC_fin(PCSC *in)
{
    if (!in) return;
    PCSC_disconnectToCard(in);
    (void)SCardReleaseContext(in->ctx);
}

uint8_t PCSC_connectToCard(PCSC *in, const char *card)
{
    if (!in || !card) return 1;

    if (PCSC_checkCTX(in))
    {
        return 1;
    }

    DWORD protocol;
    LONG rv = SCardConnectA(in->ctx, card,
                            SCARD_SHARE_EXCLUSIVE,
                            SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1,
                            &in->cardHandle,
                            &protocol);
    if (rv != SCARD_S_SUCCESS)
    {
        fprintf(stderr, "SCardConnectA failed: 0x%lX\n", rv);
        PCSC_disconnectToCard(in);
        return 1;
    }

    switch(protocol)
    {
    case SCARD_PROTOCOL_T0:
        in->protocol = SCARD_PCI_T0;
        break;
    case SCARD_PROTOCOL_T1:
        in->protocol = SCARD_PCI_T1;
        break;
    default:
        fputs("PCSC::connectToCard: Unknown protocol.", stderr);
        PCSC_disconnectToCard(in);
        return 1;
    }

    return 0;
}

void PCSC_disconnectToCard(PCSC *in)
{
    if (!in) return;

    LONG rv = SCardDisconnect(in->cardHandle, SCARD_UNPOWER_CARD);
    if (rv != SCARD_S_SUCCESS)
    {
        fprintf(stderr, "SCardDisconnect failed: 0x%lX\n", rv);
    }
}

char *PCSC_readerList(PCSC *in)
{
    if (!in) return NULL;

    DWORD dwReaders = SCARD_AUTOALLOCATE;
    LONG rv;
    char *ret = NULL;

    if (PCSC_checkCTX(in))
    {
        return NULL;
    }

    rv = SCardListReadersA(in->ctx, NULL, NULL, &dwReaders);
    if (!dwReaders || rv != SCARD_S_SUCCESS)
    {
        fprintf(stderr, "SCardListReaders failed: 0x%lX\n", rv);
        return NULL;
    }

    ret = calloc(dwReaders, sizeof(char));
    if (!ret)
    {
        fputs("PCSC_readerList: fail to allocate memory.", stderr);
        return NULL;
    }

    rv = SCardListReadersA(in->ctx, NULL, ret, &dwReaders);
    if (!dwReaders || rv != SCARD_S_SUCCESS)
    {
        fprintf(stderr, "SCardListReaders failed: 0x%lX\n", rv);
        free(ret);
        return NULL;
    }

    return ret;
}

uint8_t PCSC_ready(PCSC *in)
{
    if (!in) return 0;

    char tmpBuf1[512];
    DWORD tmpBuf1Len = 512;
    DWORD state, protocol;
    unsigned char tmpBuf2[33];
    DWORD tmpBuf2Len = 33;

    LONG rv = SCardStatusA(in->cardHandle,
                           tmpBuf1,
                           &tmpBuf1Len,
                           &state,
                           &protocol,
                           tmpBuf2,
                           &tmpBuf2Len);
    if (rv != SCARD_S_SUCCESS)
    {
        fprintf(stderr, "SCardStatusA failed: 0x%lX\n", rv);
        return 0;
    }

    return (state == SCARD_SPECIFIC);
}

uint8_t PCSC_transmit(PCSC *in, uint8_t *data, size_t dataLen, uint8_t *output, size_t *outputLen)
{
    if (!in) return 1;

    uint8_t outBuf[4096];
    DWORD outLen = 4096;
    uint8_t *tmpPtr = data;
    uint8_t getResponse[5] = {0x90, 0xC0, 0x0};

retry:
    LONG rv = SCardTransmit(in->cardHandle,
                            in->protocol,
                            tmpPtr,
                            dataLen,
                            NULL,
                            outBuf,
                            &outLen);
    if (rv != SCARD_S_SUCCESS)
    {
        fprintf(stderr, "SCardTransmit failed: 0x%lX\n", rv);
        return 1;
    }

    DWORD resLen = outLen;
    if (outLen)
    {
        if (outBuf[outLen - 2] == 0x61)
        {
            tmpPtr = getResponse;
            tmpPtr[4] = outBuf[outLen - 1];
            outLen = resLen;
            dataLen = 5;
            goto retry;
        }

        int64_t res = (outBuf[outLen - 2] << 8) | outBuf[outLen - 1];

        if (res != 0x9000)
        {
            return 1;
        }

        *outputLen = outLen - 2;
        memcpy(output, outBuf, *outputLen);

        return 0;
    }

    return 1;
}

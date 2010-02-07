/* -*- c-file-style: "ruby"; indent-tabs-mode: nil -*- */
#include <stdio.h>
#include <string.h>
#include "oranumber_util.h"

int oranumber_to_str(const OCINumber *on, char *buf, int buflen)
{
    signed char exponent;
    signed char mantissa[21]; /* terminated by a negative number */
    int datalen = on->OCINumberPart[0];
    int len = 0;
    int idx;
    int n;
#define PUTC(chr) do { \
    if (len < buflen) { \
        buf[len++] = (chr); \
    } else { \
        return ORANUMBER_TOO_SHORT_BUFFER; \
    } \
} while(0)
#define PUTEND() do { \
    if (len < buflen) { \
        buf[len] = '\0'; \
    } else { \
        return ORANUMBER_TOO_SHORT_BUFFER; \
    } \
} while(0)

    if (datalen == 0) {
        /* too short */
        return -1;
    }
    if (datalen == 1) {
        if (on->OCINumberPart[1] == 0x80) {
            /* zero */
            PUTC('0');
            PUTEND();
            return 1;
        }
        /* unexpected format */
        return -1;
    }
    if (datalen > 21) {
        /* too long */
        return -1;
    }
    /* normalize exponent and mantissa */
    if (on->OCINumberPart[1] >= 128) {
        /* positive number */
        exponent = on->OCINumberPart[1] - 193;
        for (idx = 0; idx < on->OCINumberPart[0] - 1; idx++) {
            mantissa[idx] = on->OCINumberPart[idx + 2] - 1;
        }
        mantissa[idx] = -1;
    } else {
        /* negative number */
        exponent = 62 - on->OCINumberPart[1];
        for (idx = 0; idx < on->OCINumberPart[0] - 1; idx++) {
            mantissa[idx] = 101 - on->OCINumberPart[idx + 2];
        }
        mantissa[idx] = -1;
        PUTC('-');
    }
    /* convert exponent and mantissa to human readable number */
    idx = 0;
    if (exponent-- >= 0) {
        /* integer part */
        n = mantissa[idx++];
        if (n / 10 != 0) {
            PUTC(n / 10 + '0');
        }
        PUTC(n % 10 + '0');
        while (exponent-- >= 0) {
            n = mantissa[idx++];
            if (n < 0) {
                do {
                    PUTC('0');
                    PUTC('0');
                } while (exponent-- >= 0);
                PUTEND();
                return len;
            }
            PUTC(n / 10 + '0');
            PUTC(n % 10 + '0');
        }
        if (mantissa[idx] < 0) {
            PUTEND();
            return len;
        }
    } else {
        PUTC('0');
    }
    PUTC('.');
    /* fractional number part */
    while (++exponent < -1) {
        PUTC('0');
        PUTC('0');
    }
    while ((n = mantissa[idx++]) >= 0) {
        PUTC(n / 10 + '0');
        PUTC(n % 10 + '0');
    }
    if (buf[len - 1] == '0') {
        len--;
    }
    PUTEND();
    return len;
}

int oranumber_from_str(OCINumber *on, const char *buf, int buflen)
{
    const char *end;
    int is_positive = 1;
    char mantissa[41];
    int dec_point;
    long exponent = 0;
    int idx = 0;
    int i;

    if (buflen < 0) {
        end = buf + strlen(buf);
    } else {
        end = buf + buflen;
    }

    /* skip leading spaces */
    while (buf < end && *buf == ' ') {
        buf++;
    }
    if (buf == end) {
        return ORANUMBER_INVALID_NUMBER;
    }
    /* read a sign mark */
    if (*buf == '+') {
        buf++;
    } else if (*buf == '-') {
        buf++;
        is_positive = 0;
    }
    /* next should be number or a dot */
    if ((*buf < '0' || '9' < *buf) && *buf != '.') {
        return ORANUMBER_INVALID_NUMBER;
    }
    /* skip leading zeros */
    while (buf < end) {
        if (*buf == '0') {
            buf++;
        } else {
            break;
        }
    }
    /* read integer part */
    while (buf < end) {
        if ('0' <= *buf && *buf <= '9') {
            if (idx < 41) {
                mantissa[idx] = *buf - '0';
            }
            idx++;
        } else if (*buf == '.' || *buf == 'E' || *buf == 'e' || *buf == ' ') {
            break;
        } else {
            return ORANUMBER_INVALID_NUMBER;
        }
        buf++;
    }
    dec_point = idx;
    /* read fractional part */
    if (buf < end && *buf == '.') {
        buf++;
        if (idx == 0) {
            /* skip leading zeros */
            while (buf < end) {
                if (*buf == '0') {
                    dec_point--;
                    buf++;
                } else {
                    break;
                }
            }
        }
        while (buf < end) {
            if ('0' <= *buf && *buf <= '9') {
                if (idx < 41) {
                    mantissa[idx++] = *buf - '0';
                }
            } else if (*buf == 'E' || *buf == 'e' || *buf == ' ') {
                break;
            } else {
                return ORANUMBER_INVALID_NUMBER;
            }
            buf++;
        }
    }
    /* read exponent part */
    if (buf < end && (*buf == 'E' || *buf == 'e')) {
        int negate = 0;
        buf++;
        if (buf < end) {
            if (*buf == '+') {
                buf++;
            } else if (*buf == '-') {
                buf++;
                negate = 1;
            }
        }
        while (buf < end) {
            if ('0' <= *buf && *buf <= '9') {
                exponent *= 10;
                exponent += *buf - '0';
            } else if (*buf == ' ') {
                break;
            } else {
                return ORANUMBER_INVALID_NUMBER;
            }
            buf++;
        }
        if (negate) {
            exponent = -exponent;
        }
    }
    /* skip trailing spaces */
    while (buf < end && *buf == ' ') {
        buf++;
    }
    /* final format check */
    if (buf != end) {
        return ORANUMBER_INVALID_NUMBER;
    }
    /* determine exponent */
    exponent += dec_point - 1;
    if (exponent % 2 == 0) {
        memmove(mantissa + 1, mantissa, 40);
        mantissa[0] = 0;
        idx++;
    }
    /* round if needed */
    if (idx > 40) {
        idx = 40;
        if (mantissa[40] >= 5) {
            /* round up */
            for (i = 39; i >= 0; i--) {
                mantissa[i]++;
                if (mantissa[i] == 10) {
                    mantissa[i] = 0;
                } else {
                    break;
                }
            }
            if (i == -1) {
                /* all figures are rounded up. */
                mantissa[0] = 0;
                mantissa[1] = 1;
                idx = 2;
                exponent++;
            }
        }
    }
    /* shrink mantissa scale */
    while (idx > 0 && mantissa[idx - 1] == 0) {
        idx--;
    }
    /* check zero or underflow */
    if (idx == 0 || exponent < -130) {
        on->OCINumberPart[0] = 1;
        on->OCINumberPart[1] = 0x80;
        return ORANUMBER_SUCCESS;
    }
    /* check overflow */
    if (exponent > 125) {
        return ORANUMBER_NUMERIC_OVERFLOW;
    }
    /* change the base number from 10 to 100 */
    if (idx % 2 == 1) {
        mantissa[idx++] = 0;
    }
    idx /= 2;
    for (i = 0; i < idx; i++) {
        mantissa[i] = mantissa[i * 2] * 10 + mantissa[i * 2 + 1];
    }
    /* add negative value's terminator */
    if (!is_positive && idx < 20) {
        mantissa[idx++] = -1;
    }
    /* construct OCINumber */
    on->OCINumberPart[0] = 1 + idx;
    if (is_positive) {
        on->OCINumberPart[1] = (exponent >> 1) + 193;
        for (i = 0; i < idx; i++) {
            on->OCINumberPart[i + 2] = mantissa[i] + 1;
        }
    } else {
        on->OCINumberPart[1] = 62 - (exponent >> 1);
        for (i = 0; i < idx; i++) {
            on->OCINumberPart[i + 2] = 101 - mantissa[i];
        }
    }
    return ORANUMBER_SUCCESS;
}

int oranumber_dump(const OCINumber *on, char *buf)
{
    int idx;
    int len = on->OCINumberPart[0];
    int offset;

    offset = sprintf(buf, "Typ=2 Len=%u: ", len);
    if (len > 21) {
        len = 21;
    }
    for (idx = 1; idx <= len; idx++) {
        offset += sprintf(buf + offset, "%hhu,", on->OCINumberPart[idx]);
    }
    buf[--offset] = '\0';
    return offset;
}


#include "utils.h"

int unicode_to_utf8(const unsigned short* uc, int len, char* out, int len_out)
{
    int row, cell, i, p = 0;

    if (len <= 0)
        len = strlen16bit(uc);

    for (i = 0; i < len; i++) {
        row = (uc[i] >> 8) & 0xff;
        cell = uc[i] & 0xff;

        if (!row && cell < 0x80) {
            if (p >= (len_out - 1))
                break;
            out[p++] = cell;
        } else {
            unsigned char b = (row << 2) | (cell >> 6);
            if (row < 0x08) {
                if (p >= (len_out - 2))
                    break;
                out[p++] = 0xc0 | b;
            } else {
                if (p >= (len_out - 3))
                    break;
                out[p++] = 0xe0 | (row >> 4);
                out[p++] = 0x80 | (b & 0x3f);
            }
            out[p++] = 0x80 | (cell & 0x3f);
        }
    }

    out[p] = 0;

    return p;
}

int utf8_to_unicode(const char* chars, int len, unsigned short* result, int len_out)
{
    unsigned short uc = 0;
    int need = 0, i, p = 0;

    for (i = 0; i < len; i++) {
        unsigned char ch = chars[i];
        if (need) {
            if ((ch & 0xc0) == 0x80) {
                uc = (uc << 6) | (ch & 0x3f);
                need--;
                if (!need) {
                    if (p >= (len_out - 1))
                        break;
                    result[p++] = uc;
                }
            } else {
                result[p++] = 0xfffd;
                need = 0;
            }
        } else {
            if (ch < 128) {
                result[p++] = ch;
            } else if ((ch & 0xe0) == 0xc0) {
                uc = ch & 0x1f;
                need = 1;
            } else if ((ch & 0xf0) == 0xe0) {
                uc = ch & 0x0f;
                need = 2;
            }
        }
    }

    return p;
}

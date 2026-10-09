#include <psyz.h>
#include <libapi.h>
#include <psyz/log.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>

// 1:valid, 0:invalid
static inline int validate_chan(long chan) {
    unsigned int cardNo = (unsigned int)(chan & 15);
    unsigned int portNo = (unsigned int)(chan >> 4);
    if (cardNo >= 4) { // validate multi-tap card support
        return 0;
    }
    if (portNo >= 2) { // PS1 has only two memcard ports
        return 0;
    }
    return 1;
}

void _bu_init(void) {
#ifdef _MSC_VER
    NOT_IMPLEMENTED;
#elif __MINGW32__
    mkdir("bu00");
    mkdir("bu10");
#else
    mkdir("bu00", 0755);
    mkdir("bu10", 0755);
#endif
}

long _card_auto(long val) {
    NOT_IMPLEMENTED;
    return val;
}

long _card_info(long chan) {
    if (!validate_chan(chan)) {
        return 0;
    }
    // file-backed card is always present and ready
    return 1;
}

long _card_load(long chan) {
    if (!validate_chan(chan)) {
        return 0;
    }
    return 1;
}

void _new_card(void) {}

long _card_status(long drv) {
    (void)drv;
    return 1; // synchronous file IO: never busy
}

void InitCARD2(long val) { (void)val; }

long StartCARD2(void) { return 0; }

long StopCARD2(void) { return 0; }

void _ExitCard(void) {}

static void _bzero(unsigned char* p, int n) { memset(p, 0, n); }

long _card_sector_write(long chan, long block, unsigned char* buf) {
    unsigned char sp10[0x80];
    u8 checksum;
    s32 retries;
    s32 i;
    u8* var_v1;
    u8* var_v1_2;

    retries = 0;
    var_v1 = buf;
    checksum = 0;
    for (i = 0; i < 0x7F; i++) {
        checksum ^= *var_v1++;
    }
    *var_v1 = checksum;
    while (1) {
        if (retries < 8) {
            _new_card();
            if (_card_write(chan, block, buf) == 1) {
                do {
                } while (!(_card_status(chan >> 4) & 1));
                var_v1_2 = sp10;
                _bzero(var_v1_2, sizeof(sp10));
                _new_card();
                if (_card_read(chan, block, var_v1_2) != 1) {
                    ERRORF("card read error\n");
                } else {
                    do {
                    } while (!(_card_status(chan >> 4) & 1));
                }
                var_v1 = sp10;
                checksum = 0;
                for (i = 0; i < 0x7F; i++) {
                    checksum ^= *var_v1++;
                }
                if (buf[0x7F] == checksum) {
                    return 1;
                }
                retries++;
            } else {
                return 0;
            }
        } else {
            break;
        }
    }
    return 0;
}

// File-backed memcard image: one 128KB file per channel, PSX sector
// geometry (128-byte frames). Created on demand; short reads zero-fill.
#define CARD_SECTOR 128
#define CARD_SECTORS 1024

static FILE* card_file(long chan, int for_write) {
    char path[32];
    snprintf(path, sizeof(path), "card%ld.mcd", (long)((chan >> 4) & 1));
    FILE* f = fopen(path, "r+b");
    if (f == NULL && for_write) {
        f = fopen(path, "w+b");
    }
    return f;
}

long _card_write(long chan, long block, unsigned char* buf) {
    if (!validate_chan(chan) || block < 0 || block >= CARD_SECTORS) {
        return 0;
    }
    FILE* f = card_file(chan, 1);
    if (f == NULL) {
        return 0;
    }
    fseek(f, block * CARD_SECTOR, SEEK_SET);
    size_t n = fwrite(buf, 1, CARD_SECTOR, f);
    fclose(f);
    return n == CARD_SECTOR ? 1 : 0;
}

long _card_read(long chan, long block, unsigned char* buf) {
    if (!validate_chan(chan) || block < 0 || block >= CARD_SECTORS) {
        return 0;
    }
    memset(buf, 0, CARD_SECTOR);
    FILE* f = card_file(chan, 0);
    if (f == NULL) {
        return 1; // no card image yet: reads as blank card
    }
    fseek(f, block * CARD_SECTOR, SEEK_SET);
    fread(buf, 1, CARD_SECTOR, f);
    fclose(f);
    return 1;
}

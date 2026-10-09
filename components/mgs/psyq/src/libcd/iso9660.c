#include <common.h>
#ifdef ESP_PLATFORM
#include "esp_attr.h"
#define PSYZ_ESP_XRAM EXT_RAM_BSS_ATTR
#else
#define PSYZ_ESP_XRAM
#endif
#include <libcd.h>

typedef union {
    int addr;
    CdlLOC i;
} LBA;

typedef struct {
    int num;       // 1-based directory number
    int parentNum; // parent directory number
    LBA lba;       // location of directory extent
    char name[0x20];
} CdlDIR;

// ISO 9660 Primary Volume Descriptor (at LBA 0x10) - partial layout
typedef struct {
    unsigned char type;          // volume descriptor type: 1 = PVD
    char id[5];                  // standard identifier: "CD001"
    unsigned char version;       // descriptor version
    unsigned char _unused[0x85]; // fields not needed
    LBA pathTableLBA;            // location of L-path table (little-endian)
} IsoPVD;

extern int CD_debug;
extern int CD_nopen;

static int cached_dir_num_ = 0; // index of the currently cached directory
static int cached_nopen_ = -1;  // CD_nopen value when the cache was last filled
static CdlFILE file_[CdlMAXFILE];      // store cached file entries
PSYZ_ESP_XRAM static CdlDIR dire_[CdlMAXDIR];        // store cached directory entries
PSYZ_ESP_XRAM static unsigned char load_buf_[0x800]; // CD sector buffer

s32 CD_cachefile(s32);
s32 CD_newmedia(void);
int CD_searchdir(int parentNum, char* name);
static int _cmp(const char* str1, const char* str2);
int cd_read(int n_sectors, int sector_no, unsigned char* ptr);

CdlFILE* CdSearchFile(CdlFILE* fp, char* name) {
    char buf[0x20];
    int parentDirNum; // 1-based directory number of the current path
    int i;
    unsigned char* pathPtr; // pointer into the input path string
    unsigned char* nameEnd; // pointer into buf, tracks end of current name
    unsigned char ch;

    if (cached_nopen_ != CD_nopen) {
        if (CD_newmedia() == 0) {
            return NULL;
        }
        cached_nopen_ = CD_nopen;
    }
    parentDirNum = 1;
    if (*name != '\\') {
        return NULL;
    }
    buf[0] = '\0';
    pathPtr = name;
    for (i = 0; i < CdlMAXLEVEL; i++) {
        fp++;
        fp--;
        ch = *pathPtr;
        nameEnd = buf;
        while (ch != '\\') {
            if (!ch) {
                goto out;
            }
            *nameEnd++ = ch;
            ch = *++pathPtr;
        }
        if (!*pathPtr) {
            break;
        }
        pathPtr++;
        *nameEnd = '\0';
        parentDirNum = CD_searchdir(parentDirNum, buf);
        if (parentDirNum == -1) {
            buf[0] = '\0';
            break;
        }
    }
out:
    if (i >= CdlMAXLEVEL) {
        if (CD_debug > 0) {
            printf("%s: path level (%d) error\n", name, i);
        }
        return NULL;
    }
    if (buf[0] == '\0') {
        if (CD_debug > 0) {
            printf("%s: dir was not found\n", name);
        }
        return NULL;
    }
    *nameEnd = '\0';
    if (CD_cachefile(parentDirNum) == 0) {
        if (CD_debug > 0) {
            printf("CdSearchFile: disc error\n");
        }
        return NULL;
    }
    if (CD_debug >= 2) {
        printf("CdSearchFile: searching %s...\n", buf);
    }
    for (i = 0; i < LEN(file_); i++) {
        if (file_[i].name[0] == '\0') {
            break;
        }
        if (_cmp(file_[i].name, buf)) {
            if (CD_debug >= 2) {
                printf("%s:  found\n", buf);
            }
            *fp = file_[i];
            return &file_[i];
        }
    }
    if (CD_debug > 0) {
        printf("%s: not found\n", buf);
    }
    return NULL;
}

static int _cmp(const char* str1, const char* str2) {
    return strncmp(str1, str2, 12) < 1U;
}

s32 CD_newmedia(void) {
    LBA pathTableLBA;
    unsigned char* pathTablePtr;
    int i;

    if (cd_read(1, 0x10, load_buf_) != 1) {
        if (CD_debug > 0) {
            printf("CD_newmedia: Read error in cd_read(PVD)\n");
        }
        return 0;
    }
    if (strncmp(((IsoPVD*)load_buf_)->id, "CD001", 5) != 0) {
        if (CD_debug > 0) {
            printf("CD_newmedia: Disc format error in cd_read(PVD)\n");
        }
        return 0;
    }
    (&pathTableLBA)->i = ((IsoPVD*)load_buf_)->pathTableLBA.i;
    if (cd_read(1, pathTableLBA.addr, load_buf_) != 1) {
        if (CD_debug > 0) {
            printf("CD_newmedia: Read error (PT:%08x)\n", pathTableLBA.addr);
        }
        return 0;
    }
    if (CD_debug > 1) {
        printf("CD_newmedia: sarching dir..\n");
    }
    do {
        i = 0;
        pathTablePtr = load_buf_;
        while (pathTablePtr < load_buf_ + sizeof(load_buf_)) {
            if (pathTablePtr[0] == 0) {
                break;
            }
            dire_[i].lba.i = ((LBA*)(&pathTablePtr[2]))->i;
            dire_[i].parentNum = pathTablePtr[6];
            dire_[i].num = i + 1;
            memcpy(dire_[i].name, &pathTablePtr[8], pathTablePtr[0]);
            dire_[i].name[pathTablePtr[0]] = '\0';
            pathTablePtr += 8 + pathTablePtr[0] + pathTablePtr[0] % 2;
            if (CD_debug > 1) {
                printf("\t%08x,%04x,%04x,%s\n", dire_[i].lba.addr, dire_[i].num,
                       dire_[i].parentNum, dire_[i].name);
            }
            if (++i >= CdlMAXDIR) {
                break;
            }
        }
        if (i < CdlMAXDIR) {
            dire_[i].parentNum = 0; // sentinel: no more entries
        }
    } while (0);

    cached_dir_num_ = 0;
    if (CD_debug > 1) {
        printf("CD_newmedia: %d dir entries found\n", i);
    }
    return 1;
}

int CD_searchdir(int parentNum, char* name) {
    int i;

    for (i = 0; i < CdlMAXDIR; i++) {
        if (dire_[i].parentNum == 0) {
            return -1;
        }
        if (dire_[i].parentNum != parentNum) {
            continue;
        }
        if (strcmp(name, dire_[i].name) == 0) {
            return i + 1;
        }
    }
    return -1;
}

s32 CD_cachefile(s32 dirNum) {
    LBA entryLba;
    u8* entry;
    s32 i;
    short* namePtr;

    if (dirNum == cached_dir_num_) {
        return 1;
    }

    if (cd_read(1, (dire_ - 1)[dirNum].lba.addr, load_buf_) != 1) {
        if (CD_debug > 0) {
            printf("CD_cachefile: dir not found\n");
        }
        return -1;
    }
    if (CD_debug > 1) {
        printf("CD_cachefile: searching...\n");
    }
    entry = load_buf_;
    i = 0;
    while (entry < load_buf_ + sizeof(load_buf_)) {
        if (entry[0] == 0) {
            break;
        }
        entryLba.i = ((LBA*)&entry[2])->i;
        CdIntToPos(entryLba.addr, &file_[i].pos);
        ((LBA*)&file_[i].size)->i = ((LBA*)&entry[0xA])->i;
        switch (i) {
        case 0:
            namePtr = (short*)file_[i].name;
            __builtin_memcpy(namePtr, ".", 2);
            break;
        case 1:
            namePtr = (short*)file_[i].name;
            __builtin_memcpy(namePtr, "..", 3);
            break;
        default:
            memcpy(file_[i].name, &entry[0x21], entry[0x20]);
            file_[i].name[entry[0x20]] = '\0';
            break;
        }
        if (CD_debug > 1) {
            printf("\t(%02x:%02x:%02x) %8d %s\n", file_[i].pos.minute,
                   file_[i].pos.second, file_[i].pos.sector, file_[i].size,
                   file_[i].name);
        }
        entry += entry[0];
        if (++i >= CdlMAXFILE) {
            break;
        }
    }
    cached_dir_num_ = dirNum;
    if (i < CdlMAXFILE) {
        file_[i].name[0] = '\0';
    }
    if (CD_debug > 1) {
        printf("CD_cachefile: %d files found\n", i);
    }
    return 1;
}

int cd_read(int n_sectors, int sector_no, unsigned char* ptr) {
    CdlLOC pos;

    CdIntToPos(sector_no, &pos);
    CdControl(CdlSetloc, (u_char*)&pos, 0);
    CdRead(n_sectors, (u_long*)ptr, CdlModeSpeed);
    return CdReadSync(0, 0) == 0;
}

#include "libcd_private.h"

int CdGetToc2(int n, CdlLOC* loc);
int CdGetToc(CdlLOC* loc) { return CdGetToc2(1, loc); }

int CdGetToc2(int n, CdlLOC* loc) {
    u_char param[4];
    u_char result[4];
    CdlCB old_cb;
    int track_first;
    int track_last;
    int i;
    int nTrack;

    param[0] = 1;
    old_cb = CdSyncCallback(NULL);
    if (!CdControlB(CdlGetTN, NULL, result)) {
        goto err;
    }
    track_first = btoi(result[1]);
    track_last = btoi(result[2]);
    if (CD_debug >= 2) {
        printf("track=%d,%d\n", track_first, track_last);
    }
    param[0] = 0;
    if (!CdControlB(CdlGetTD, param, result)) {
        goto err;
    }
    loc->minute = result[1];
    loc->second = result[2];
    loc->sector = 0;
    i = 1;
    while (track_first <= track_last) {
        param[0] = itob(track_first);
        if (!CdControlB(CdlGetTD, param, result)) {
            goto err;
        }
        loc[i].minute = result[1];
        loc[i].second = result[2];
        loc[i].sector = 0;
        i++;
        track_first++;
    }
    nTrack = i - 1;
    if (CD_debug >= 2) {
        for (i = 0; i <= nTrack; i++) {
            track_first = loc[i].second; // !FAKE
            printf("CdGetToc2: %02x:%02x:00\n", loc[i].minute, loc[i].second);
        }
    }
    CdSyncCallback(old_cb);
    return nTrack;
err:
    if (CD_debug) {
        printf("CdGetToc2: error\n");
    }
    CdSyncCallback(old_cb);
    return 0;
}

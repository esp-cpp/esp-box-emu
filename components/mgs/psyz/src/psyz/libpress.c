#include <psyz.h>
#include <libpress.h>
#include <psyz/log.h>

void DecDCTReset(int mode) { NOT_IMPLEMENTED; }

void DecDCTin(u_long* buf, int mode) { NOT_IMPLEMENTED; }

void DecDCTout(u_long* buf, int size) { NOT_IMPLEMENTED; }

DecDCCb DecDCTinCallback(DecDCCb func) {
    NOT_IMPLEMENTED;
    return NULL;
}

DecDCCb DecDCToutCallback(DecDCCb func) {
    NOT_IMPLEMENTED;
    return NULL;
}

int DecDCTvlc(u_long* bs, u_long* buf) {
    NOT_IMPLEMENTED;
    return 0;
}

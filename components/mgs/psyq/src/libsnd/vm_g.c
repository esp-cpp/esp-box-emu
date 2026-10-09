#include "libsnd_private.h"

short _svm_stereo_mono;
static short padding_D1502;
static int padding_D1504;
SpuReverbAttr _svm_rattr;
u8 _svm_vab_used[NUM_VAB];
char _SsVmMaxVoice;
static char padding_D1501;
u16 _svm_vab_count;
short kMaxPrograms;
static short padding_D1506;
struct struct_svm _svm_cur;
short _svm_damper;
char _svm_auto_kof_mode;
static char padding_D1506_1;
static int padding_D1506_2;
VabHdr* _svm_vab_vh[NUM_VAB];
ProgAtr* _svm_vab_pg[NUM_VAB];
VagAtr* _svm_vab_tn[NUM_VAB];
unsigned _svm_vab_start[NUM_VAB];
s32 _svm_vab_total[NUM_VAB];
VabHdr* _svm_vh;
ProgAtr* _svm_pg;
VagAtr* _svm_tn;
u_long* _svm_vg;

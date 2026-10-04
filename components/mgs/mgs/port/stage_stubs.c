/* Stubs for code that lives in stages we are not linking yet.
 *
 * Three kinds, all distinguishable at runtime:
 *   - actor constructors from overlays that still contain assembly. A boot
 *     reaching one is trying to spawn something from a stage we did not
 *     link; it says so and carries on rather than faulting.
 *   - SPU internals: audio, not wired up yet.
 *   - MDEC: the movie decoder, likewise.
 */

#include <stdio.h>

static void missing_actor(const char* name) {
    static int warned;
    if (warned < 20) {
        printf("[stub] actor %s is from an unlinked overlay\n", name);
        warned++;
    }
}

void* NewCamera_800CF388(void) { missing_actor("NewCamera_800CF388"); return 0; }
void* NewCape(void) { missing_actor("NewCape"); return 0; }
void* NewCountdownGcl(void) { missing_actor("NewCountdownGcl"); return 0; }
void* NewCrow(void) { missing_actor("NewCrow"); return 0; }
void* NewDog(void) { missing_actor("NewDog"); return 0; }
void* NewFog(void) { missing_actor("NewFog"); return 0; }
void* NewHind(void) { missing_actor("NewHind"); return 0; }
void* NewHindBoss(void) { missing_actor("NewHindBoss"); return 0; }
void* NewJeepDrum(void) { missing_actor("NewJeepDrum"); return 0; }
void* NewJeepEnemy(void) { missing_actor("NewJeepEnemy"); return 0; }
void* NewJeepScroll(void) { missing_actor("NewJeepScroll"); return 0; }
void* NewNinjaBoss(void) { missing_actor("NewNinjaBoss"); return 0; }
void* NewOcelotBoss(void) { missing_actor("NewOcelotBoss"); return 0; }
void* NewRope(void) { missing_actor("NewRope"); return 0; }
void* NewStage11GDemo(void) { missing_actor("NewStage11GDemo"); return 0; }
void* NewStage11Objects(void) { missing_actor("NewStage11Objects"); return 0; }
void* NewWire(void) { missing_actor("NewWire"); return 0; }

/* audio: the SPU emulation is not wired up yet */
int SpuGetKeyStatus(void) { return 0; }
int SpuReserveReverbWorkArea(void) { return 0; }
int SpuSetIRQ(void) { return 0; }
int SpuSetIRQAddr(void) { return 0; }
int SpuSetPitchLFOVoice(void) { return 0; }
int SpuSetReverbDepth(void) { return 0; }
int SpuSetReverbVoice(void) { return 0; }
int _SpuIsInAllocateArea_(void) { return 0; }
int _spu_FiDMA(void) { return 0; }
int _spu_FsetRXX(void) { return 0; }
int _spu_FsetRXXa(void) { return 0; }
int _spu_Fw(void) { return 0; }

int _spu_inTransfer;
int _spu_init(void) { return 0; }
int _spu_mem_mode_plus;
int _spu_mem_mode_unitM;
int _spu_transMode;
/* These two are NOT functions: psyz declares them as callback POINTERS
 * (libspu_private.h:190). Stubbing them as functions put them in read-only
 * flash, so the first `_spu_transferCallback = 0` inside SpuClearReverbWorkArea
 * stored into .text and faulted the cache. Kind matters as much as name. */
void (* volatile _spu_transferCallback)();
void (* volatile _spu_IRQCallback)();
int _spu_tsa;

/* MDEC: full-motion video */
int DecDCToutSync(void) { return 0; }
int DecDCTvlc2(void) { return 0; }
int DecDCTvlcBuild(void) { return 0; }

/* per-stage data referenced by tables in the main executable */
int AN_DamageSmoke1;
int d18a_snake18_800D4E94;
int s15c_dyncon_800D8C9C;

/* Sound-driver internals (libsnd). The SPU emulation exists but the sequenced
 * music driver is not wired up; these keep the link honest until it is. */
int VBLANK_MINUS;
int _snd_ev_flag;
int _snd_openflag;
int _spu_Fr;
int _svm_sreg;
void _SsMarkCallback(void) {}
void _SsVmInit(void) {}

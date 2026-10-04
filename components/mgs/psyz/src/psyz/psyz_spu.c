#include <psyz.h>
#ifdef ESP_PLATFORM
#include "esp_attr.h"
#define PSYZ_ESP_XRAM EXT_RAM_BSS_ATTR
#else
#define PSYZ_ESP_XRAM
#endif
#include <psyz/log.h>
#include <assert.h>
#include <string.h>
#include "libspu/libspu_private.h" /* esp-box-emu: psyq/src on the include path */
#include "spu_gauss.h"

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(SPU_RXX) == 0x200, "SPU_RXX must be 0x200 bytes");
_Static_assert(
    sizeof(union SpuUnion) == sizeof(SPU_RXX), "SpuUnion must alias SPU_RXX");
#endif

// One ADPCM block decodes to 28 samples
#define ADPCM_BLOCK_BYTES 16
#define ADPCM_BLOCK_SAMPLES 28

static inline short clamp16(int v) {
    if (v < -32768)
        return -32768;
    if (v > 32767)
        return 32767;
    return v;
}

static inline short clamp15(int v) {
    if (v < -16384)
        return -16384;
    if (v > 16383)
        return 16383;
    return v;
}

static inline int sign4(int n) { return (n & 0x8) ? (n - 0x10) : n; }

// Decode one 16-byte SPU ADPCM block into 28 short samples. PS1 SPU ADPCM:
//   block[0]    = shift_filter: low nibble = shift (12-shift_in, or 9 if >12)
//                               high nibble = filter index (clamped 0..4)
//   block[1]    = flags: bit0 = loop-end, bit1 = repeat, bit2 = loop-start
//   block[2..15]= 14 bytes of 4-bit nibbles (low nibble first)
static void spu_adpcm_decode_block(
    const unsigned char block[ADPCM_BLOCK_BYTES], short* hist1, short* hist2,
    short out[ADPCM_BLOCK_SAMPLES], u8* flags_out) {
    static const int pos[5] = {0, 60, 115, 98, 122};
    static const int neg[5] = {0, 0, -52, -55, -60};

    int shift_in = block[0] & 0x0F;
    int shift = (shift_in > 12) ? 9 : (12 - shift_in);
    int filter = (block[0] >> 4) & 0x07;
    if (filter > 4)
        filter = 4;
    int f0 = pos[filter];
    int f1 = neg[filter];
    if (flags_out)
        *flags_out = block[1];

    short prev = *hist1;
    short prev2 = *hist2;
    for (int i = 0; i < 14; i++) {
        unsigned short byte = block[2 + i];
        for (int n = 0; n < 2; n++) {
            int t = sign4((byte >> (n * 4)) & 0x0F);
            int s = (t << shift) + ((prev * f0) >> 6) + ((prev2 * f1) >> 6);
            short final = clamp16(s);
            out[i * 2 + n] = final;
            prev2 = prev;
            prev = final;
        }
    }
    *hist1 = prev;
    *hist2 = prev2;
}

#define N_CHANNELS 2                      // stereo interleaved
#define CD_RING_FRAMES 4096               // must be power of 2
#define CD_RING_MASK (CD_RING_FRAMES - 1) // ring wrapper
#define CD_RING_LOW_WATER 1024            // refill when short of N frames
#define ENV_KEYON_DELAY_TICKS 6 // latency to simulate PS1 SPU latching a key-on

typedef enum {
    ADSR_ATTACK = 0,
    ADSR_DECAY,
    ADSR_SUSTAIN,
    ADSR_RELEASE,
    ADSR_OFF,
} AdsrState;

typedef struct {
    unsigned cur_addr;    // current 16-byte block address in SPU RAM
    unsigned repeat_addr; // from voice loop_addr reg, or block flag bit 2
    short hist1, hist2;
    short samples[ADPCM_BLOCK_SAMPLES];
    u8 block_flags; // flags byte of the most-recently-decoded block
    u8 sample_idx;  // next sample to consume from samples[] (0..28)
    unsigned spos;  // 16.16 fixed-point counter (1.0 = 0x10000)
    unsigned sinc;  // pitch_reg << 4 is the per-output-tick increment
    short gwin[4];  // GAUSS interpolation window
    u8 gpos;        // index to next decoded sample in gwin
    u8 active;
    u8 needs_decode;
    int env_vol;          // mirrors SPU_VOICE_REG::volumex
    unsigned env_counter; // rate divider, increases by 1 for all voice duration
    AdsrState env_state;  // voice phase, works like a state machine
    u8 key_off;           // set by key-off; drives the release phase
    u8 delay_ticks;       // key-on -> envelope-start pipeline latency
    u16 adsr_lo;          // mirrors SPU_VOICE_REG::adsr[0]
    u16 adsr_hi;          // mirrors SPU_VOICE_REG::adsr[1]
} VoiceState;

// Full SPU state
PSYZ_ESP_XRAM static struct {
    u8 ram[PSYZ_SPU_RAM_SIZE];

    // Transfer address (byte address into SPU RAM)
    unsigned transfer_addr;

    // Capture buffer position
    u16 capture_pos;

    VoiceState voice[PSYZ_SPU_NUM_VOICES];

    // CD audio ring buffer, refilled by Psyz_CdPullSamples
    short cd_ring[CD_RING_FRAMES * N_CHANNELS];
    unsigned cd_ring_read;
    unsigned cd_ring_count; // number of valid frames in ring

    u8 initialized;
} spu;

u8* Psyz_SpuGetRam(void) { return spu.ram; }

void Psyz_SpuInit(void) {
    if (spu.initialized)
        return;
    memset(&spu, 0, sizeof(spu));
    spu.initialized = 1;
    INFOF("SPU emulation initialized");
}

static void spu_reset(void) {
    _spu_RXX->rxx.spustat = 0;
    for (int v = 0; v < PSYZ_SPU_NUM_VOICES; v++) {
        _spu_RXX->rxx.voice[v].volumex = 0;
    }
    memset(&spu, 0, sizeof(spu));
}

static void spu_reset_hot(void) {
    /* 512KB on the stack is instantly fatal on an embedded task (ESP32-S3
       stacks are tens of KB), and needlessly deep on desktop threads too.
       Static: this path is not reentrant - it swaps SPU RAM around a reset. */
    PSYZ_ESP_XRAM static u8 saved_ram[PSYZ_SPU_RAM_SIZE];
    memcpy(saved_ram, spu.ram, PSYZ_SPU_RAM_SIZE);
    spu_reset();
    memcpy(spu.ram, saved_ram, PSYZ_SPU_RAM_SIZE);
}

void Psyz_SpuReset(int hot) {
    if (hot) {
        spu_reset_hot();
    } else {
        spu_reset();
    }
    spu.initialized = 1;
}

void Psyz_SpuSetTransferAddr(unsigned int addr) {
    spu.transfer_addr = addr & (PSYZ_SPU_RAM_SIZE - 1);
}

unsigned int Psyz_SpuGetTransferAddr(void) { return spu.transfer_addr; }

void Psyz_SpuFifoWrite(unsigned short word) {
#ifdef PLATFORM_LE
    *(unsigned short*)&spu.ram[spu.transfer_addr] = word;
#else
    spu.ram[spu.transfer_addr] = (unsigned char)(word & 0xFF);
    spu.ram[spu.transfer_addr + 1] = (unsigned char)((word >> 8) & 0xFF);
#endif
    spu.transfer_addr = (spu.transfer_addr + 2) & (PSYZ_SPU_RAM_SIZE - 1);
}

void Psyz_SpuFifoWriteBulk(const unsigned char* src, unsigned int size) {
    Psyz_SpuMemWrite(spu.transfer_addr, src, size);
    spu.transfer_addr = (spu.transfer_addr + size) & (PSYZ_SPU_RAM_SIZE - 1);
}

void Psyz_SpuMemRead(unsigned int offset, void* dst, unsigned int size) {
    unsigned int start = offset & (PSYZ_SPU_RAM_SIZE - 1);
    unsigned int head = PSYZ_SPU_RAM_SIZE - start;
    if (size <= head) {
        memcpy(dst, &spu.ram[start], size);
    } else {
        memcpy(dst, &spu.ram[start], head);
        memcpy((unsigned char*)dst + head, &spu.ram[0], size - head);
    }
}

void Psyz_SpuMemWrite(unsigned int offset, const void* src, unsigned int size) {
    unsigned int start = offset & (PSYZ_SPU_RAM_SIZE - 1);
    unsigned int head = PSYZ_SPU_RAM_SIZE - start;
    if (size <= head) {
        memcpy(&spu.ram[start], src, size);
    } else {
        memcpy(&spu.ram[start], src, head);
        memcpy(&spu.ram[0], (const unsigned char*)src + head, size - head);
    }
}

static void spu_key_on_voice(int v) {
    volatile SPU_RXX* rxx = &_spu_RXX->rxx;
    VoiceState* vs = &spu.voice[v];
    vs->cur_addr =
        ((unsigned)rxx->voice[v].addr << 3) & (PSYZ_SPU_RAM_SIZE - 1);
    vs->repeat_addr =
        ((unsigned)rxx->voice[v].loop_addr << 3) & (PSYZ_SPU_RAM_SIZE - 1);
    vs->hist1 = vs->hist2 = 0;
    vs->sample_idx = ADPCM_BLOCK_SAMPLES; // force decode on first consume
    vs->block_flags = 0;
    vs->needs_decode = 1;
    // sinc = pitch << 4; pitch=0x1000 -> sinc=0x10000 (1.0 in 16.16)
    unsigned pitch = rxx->voice[v].pitch;
    vs->sinc = (pitch & 0x3FFF) << 4;
    if (vs->sinc == 0)
        vs->sinc = 1;

    // Start spos at 1.0 so the first voice_step consumes exactly one sample.
    // The gauss window stays mostly zero-padded for several ticks, producing
    // the silent warmup-then-ringing pattern observed in PS1 captures
    vs->spos = 0x10000;

    // We do not reset vs->gpos at key-on for accuracy. On real PS1 hardware,
    // a voice on will carry the residual state from prior voice activity
    vs->active = 1;

    // reset the envelope to a fresh attack from zero
    vs->adsr_lo = rxx->voice[v].adsr[0];
    vs->adsr_hi = rxx->voice[v].adsr[1];
    vs->env_vol = 0;
    vs->env_counter = 0;
    vs->env_state = ADSR_ATTACK;
    vs->key_off = 0;
    vs->delay_ticks = ENV_KEYON_DELAY_TICKS;
    rxx->voice[v].volumex = 0;
}

void Psyz_SpuWrite(unsigned int reg_offset, unsigned short value) {
    if (reg_offset >= sizeof(SPU_RXX) || (reg_offset & 1)) {
        WARNF("Psyz_SpuWrite: bad offset 0x%X", reg_offset);
        return;
    }
    if (reg_offset == offsetof(SPU_RXX, spustat)) { // read-only
        return;
    }
    _spu_RXX->raw[reg_offset >> 1] = value;
    // MSVC's C-mode offsetof is not an integer constant expression, so it
    // cannot appear in a case label. Use an if-chain instead.
    if (reg_offset == offsetof(SPU_RXX, key_on[0])) { // voices 0..15
        for (int v = 0; v < 16; v++) {
            if (value & (1u << v))
                spu_key_on_voice(v);
        }
    } else if (reg_offset == offsetof(SPU_RXX, key_on[1])) { // voices 16..23
        for (int v = 0; v < 8; v++) {
            if (value & (1u << v))
                spu_key_on_voice(16 + v);
        }
    } else if (reg_offset == offsetof(SPU_RXX, key_off[0])) { // voices 0..15
        for (int v = 0; v < 16; v++) {
            if (value & (1u << v))
                spu.voice[v].key_off = 1;
        }
    } else if (reg_offset == offsetof(SPU_RXX, key_off[1])) { // voices 16..23
        for (int v = 0; v < 8; v++) {
            if (value & (1u << v))
                spu.voice[16 + v].key_off = 1;
        }
    } else if (reg_offset == offsetof(SPU_RXX, trans_addr)) {
        Psyz_SpuSetTransferAddr((unsigned)value << 3);
    } else if (reg_offset == offsetof(SPU_RXX, trans_fifo)) {
        Psyz_SpuFifoWrite(value);
    }
}

unsigned short Psyz_SpuRead(unsigned int reg_offset) {
    if (reg_offset >= sizeof(SPU_RXX) || (reg_offset & 1)) {
        WARNF("Psyz_SpuRead: bad offset 0x%X", reg_offset);
        return 0;
    }
    // offsetof is not an ICE under MSVC C mode, so it cannot be a case label.
    if (reg_offset == offsetof(SPU_RXX, trans_addr)) {
        return (unsigned short)((spu.transfer_addr >> 3) & 0xFFFF);
    }
    if (reg_offset == offsetof(SPU_RXX, spustat)) {
        // lower 6 bits mirror SPUCNT's low bits
        // bit 11, capture-buffer half-pointer, flipped by spu_tick.
        return (_spu_RXX->rxx.spucnt & 0x3F) |
               (_spu_RXX->rxx.spustat & (1u << 11));
    }
    return _spu_RXX->raw[reg_offset >> 1];
}

static void write_capture(unsigned int idx, short val) {
    unsigned int addr = (idx * 0x400) | spu.capture_pos;
    if (addr < PSYZ_SPU_RAM_SIZE) {
        *(short*)&spu.ram[addr] = val;
    }
}

static int voice_decode_one_sample(VoiceState* vs) {
    if (vs->sample_idx >= ADPCM_BLOCK_SAMPLES) {
        // end of block, and handle loop/end flags
        if (!vs->needs_decode && (vs->block_flags & 0x01)) {
            if (!(vs->block_flags & 0x02)) {
                vs->active = 0;
                vs->gwin[vs->gpos] = 0;
                vs->gpos = (vs->gpos + 1) & 3;
                return 0;
            }
            vs->cur_addr = vs->repeat_addr;
        }
        vs->needs_decode = 1;
        vs->sample_idx = 0;
    }
    if (vs->needs_decode) {
        unsigned char block[ADPCM_BLOCK_BYTES];
        Psyz_SpuMemRead(vs->cur_addr, block, ADPCM_BLOCK_BYTES);
        spu_adpcm_decode_block(
            block, &vs->hist1, &vs->hist2, vs->samples, &vs->block_flags);
        if (vs->block_flags & 0x04) {
            vs->repeat_addr = vs->cur_addr;
        }
        vs->cur_addr =
            (vs->cur_addr + ADPCM_BLOCK_BYTES) & (PSYZ_SPU_RAM_SIZE - 1);
        vs->needs_decode = 0;
    }
    short s = vs->samples[vs->sample_idx++];
    vs->gwin[vs->gpos] = s;
    vs->gpos = (vs->gpos + 1) & 3;
    return 1;
}

static unsigned adsr_denominator(int rate) {
    return rate < 48 ? 1u : (1u << ((rate >> 2) - 11));
}

static int adsr_num_increase(int rate) {
    return rate < 48 ? (7 - (rate & 3)) << (11 - (rate >> 2))
                     : (7 - (rate & 3));
}

static int adsr_num_decrease(int rate) {
    return rate < 48 ? (-8 + (rate & 3)) << (11 - (rate >> 2))
                     : (-8 + (rate & 3));
}

// process voice ADSR envelope by one sample, calculate ADSR envelope volume
static void voice_envelope_step(VoiceState* vs) {
    if (vs->delay_ticks) { // simulate PS1 SPU key on latency
        vs->delay_ticks--;
        return;
    }
    if (vs->key_off && vs->env_state != ADSR_OFF) {
        vs->env_state = ADSR_RELEASE;
    }

    // the PS1 SPU has an internal counter, shared across all states, that
    // allows to trigger a change based on the selected ADSR rate
    unsigned ctr = ++vs->env_counter;
#define ADSR_FIRES(rate) ((ctr % adsr_denominator(rate)) == 0)

    switch (vs->env_state) {
    case ADSR_ATTACK: {
        int attack_rate = (vs->adsr_lo >> 8) & 0x7F;
        int attack_exp = (vs->adsr_lo >> 15) & 1;
        int rate = attack_rate;
        if (attack_exp && vs->env_vol >= 0x6000) {
            rate += 8;
        }
        if (ADSR_FIRES(rate)) {
            vs->env_vol += adsr_num_increase(rate);
            if (vs->env_vol >= 0x7FFF) {
                vs->env_vol = 0x7FFF;
                vs->env_state = ADSR_DECAY;
            }
        }
        break;
    }
    case ADSR_DECAY: {
        int decay_rate = (vs->adsr_lo >> 4) & 0x0F;
        int rate = decay_rate * 4; // decay is always exponential decrease
        if (ADSR_FIRES(rate)) {
            vs->env_vol += (adsr_num_decrease(rate) * vs->env_vol) >> 15;
            if (vs->env_vol < 0) {
                vs->env_vol = 0;
            }
            int sustain_level = vs->adsr_lo & 0x0F;
            if (((vs->env_vol >> 11) & 0xF) <= sustain_level) {
                vs->env_state = ADSR_SUSTAIN;
            }
        }
        break;
    }
    case ADSR_SUSTAIN: {
        int sustain_rate = (vs->adsr_hi >> 6) & 0x7F;
        int sustain_dec = (vs->adsr_hi >> 14) & 1;
        int sustain_exp = (vs->adsr_hi >> 15) & 1;
        int rate = sustain_rate;
        if (!sustain_dec) {
            if (sustain_exp && vs->env_vol >= 0x6000) {
                rate += 8;
            }
            if (ADSR_FIRES(rate)) {
                vs->env_vol += adsr_num_increase(rate);
                if (vs->env_vol > 0x7FFF) {
                    vs->env_vol = 0x7FFF;
                }
            }
        } else {
            if (ADSR_FIRES(rate)) {
                if (sustain_exp) {
                    vs->env_vol +=
                        (adsr_num_decrease(rate) * vs->env_vol) >> 15;
                } else {
                    vs->env_vol += adsr_num_decrease(rate);
                }
                if (vs->env_vol < 0) {
                    vs->env_vol = 0;
                }
            }
        }
        break;
    }
    case ADSR_RELEASE: {
        int release_rate = vs->adsr_hi & 0x1F;
        int rate = release_rate * 4;
        if (ADSR_FIRES(rate)) {
            int release_exp = (vs->adsr_hi >> 5) & 1;
            if (release_exp) {
                vs->env_vol += (adsr_num_decrease(rate) * vs->env_vol) >> 15;
            } else {
                vs->env_vol += adsr_num_decrease(rate);
            }
            if (vs->env_vol <= 0) {
                vs->env_vol = 0;
                vs->env_state = ADSR_OFF;
                vs->active = 0;
            }
        }
        break;
    }
    default:
        break;
    }
#undef ADSR_FIRES
}

// Advance voice `v` by one output-rate tick (44.1 kHz) and return its
// pitch-resampled, gauss-interpolated short sample.
static short voice_step(int v) {
    VoiceState* vs = &spu.voice[v];

    // for pitch changes during voice on, enable vibrato or bends
    unsigned pitch = _spu_RXX->rxx.voice[v].pitch & 0x3FFF;
    vs->sinc = pitch ? pitch << 4 : 1;

    // consume decoded samples until the pitch counter is below 1.0
    while (vs->spos >= 0x10000) {
        if (!voice_decode_one_sample(vs)) {
            // Voice stopped mid-decode; flush remaining ticks as zero.
            vs->spos = 0;
            break;
        }
        vs->spos -= 0x10000;
    }

    // GAUSS interpolation
    int vl = (vs->spos >> 6) & ~3;
    int g0 = vs->gwin[vs->gpos & 3];
    int g1 = vs->gwin[(vs->gpos + 1) & 3];
    int g2 = vs->gwin[(vs->gpos + 2) & 3];
    int g3 = vs->gwin[(vs->gpos + 3) & 3];
    int acc = (spu_gauss_tbl[vl + 0] * g0) & ~2047;
    acc += (spu_gauss_tbl[vl + 1] * g1) & ~2047;
    acc += (spu_gauss_tbl[vl + 2] * g2) & ~2047;
    acc += (spu_gauss_tbl[vl + 3] * g3) & ~2047;
    vs->spos += vs->sinc;
    return clamp16(acc >> 12);
}

static inline int voice_vol(unsigned short reg) {
    if (reg & 0x8000) {
        // https://problemkaputt.de/psxspx-spu-volume-and-adsr-generator.htm
        LOG_ONCE("voice volume bit15 not implemented");
        return 0;
    }
    int v = reg & 0x7FFF;
    if (v & 0x4000) { // bit14 is sign for values -0x4000 to 0x3FFF
        v -= 0x8000;
    }
    return v;
}

// generate one frame at 44100hz with voices mix, cd playback and volume control
static void spu_tick(short* out) {
    SPU_RXX* rxx = (SPU_RXX*)&_spu_RXX->rxx;
    unsigned short spucnt = rxx->spucnt;

    // accumulate frame to each separate channels
    int left_sum = 0, right_sum = 0;

    // Pre-fill CD ring buffer if running low
    if (spu.cd_ring_count < CD_RING_LOW_WATER) {
        size_t space = CD_RING_FRAMES - spu.cd_ring_count;
        size_t write_pos =
            (spu.cd_ring_read + spu.cd_ring_count) & CD_RING_MASK;
        // Fill into contiguous chunk up to end of array
        size_t chunk = CD_RING_FRAMES - write_pos;
        if (chunk > space)
            chunk = space;
        size_t got = Psyz_CdPullSamples(&spu.cd_ring[write_pos * 2], chunk);
        spu.cd_ring_count += got;
    }

    // Read one stereo frame from CD ring buffer
    int cd_left = 0, cd_right = 0;
    if (spu.cd_ring_count > 0) {
        cd_left = spu.cd_ring[spu.cd_ring_read * 2];
        cd_right = spu.cd_ring[spu.cd_ring_read * 2 + 1];
        spu.cd_ring_read = (spu.cd_ring_read + 1) & CD_RING_MASK;
        spu.cd_ring_count--;
    }

    // decode+resample, scale volume by ADSR envelope, then mix voices
    short v1_sample = 0, v3_sample = 0;
    for (int v = 0; v < PSYZ_SPU_NUM_VOICES; v++) {
        if (!spu.voice[v].active)
            continue;
        short s = voice_step(v);
        voice_envelope_step(&spu.voice[v]);
        if (spu.voice[v].env_state == ADSR_OFF) {
            s = 0;
        }
        // capture buffer stores sample pre-envelope, weirdly only after
        // processing the ADSR envelope -- this might need a re-test on real HW
        if (v == 1) {
            v1_sample = s;
        } else if (v == 3) {
            v3_sample = s;
        }
        rxx->voice[v].volumex = (unsigned short)spu.voice[v].env_vol;
        s = (short)(((int)s * spu.voice[v].env_vol) >> 15); // apply ADSR vol
        left_sum += (s * voice_vol(rxx->voice[v].volume.left)) >> 15;
        right_sum += (s * voice_vol(rxx->voice[v].volume.right)) >> 15;
    }

    // Mute all voices. CD audio is mixed after, and not affected by mute.
    if (!(spucnt & SPU_CTRL_MASK_MUTE_SPU)) {
        left_sum = right_sum = 0;
    }

    // Mix CD audio per SPUCNT and cd_vol registers.
    if (spucnt & SPU_CTRL_MASK_CD_AUDIO_ENABLE) {
        left_sum += (cd_left * rxx->cd_vol.left) >> 15;
        right_sum += (cd_right * rxx->cd_vol.right) >> 15;
    }

    out[0] = clamp16((left_sum * clamp15(rxx->main_vol.left)) >> 14);
    out[1] = clamp16((right_sum * clamp15(rxx->main_vol.right)) >> 14);

    // Capture buffers back to SPU RAM, as per real hardware
    write_capture(0, (short)cd_left);
    write_capture(1, (short)cd_right);
    write_capture(2, v1_sample);
    write_capture(3, v3_sample);

    // SPUSTAT bit 11 flips when capture_pos crosses 0x200
    unsigned prev_pos = spu.capture_pos;
    spu.capture_pos = (prev_pos + 2) & 0x3FF;
    if ((prev_pos ^ spu.capture_pos) & 0x200) {
        rxx->spustat ^= 1u << 11;
    }
}

#ifdef ESP_PLATFORM
// On PSX the RCnt handlers were IRQs on the ONE cpu; on the ESP32 the audio
// pull runs on core 1 and calling the handlers from there races the game
// (VSyncHandler/seq tickers touch GPU queues and prim chains). Samples are
// accumulated here and ticked from the game thread (Psyz_VideoVSync).
volatile int psyz_pending_rcnt;
#endif

void Psyz_SpuPullSamples(short* out, int num_frames) {
    if (!spu.initialized) {
        memset(out, 0, num_frames * 2 * sizeof(short));
        return;
    }
#ifdef ESP_PLATFORM
    psyz_pending_rcnt += num_frames;
#else
    Psyz_RcntAdd(num_frames);
#endif
    for (int i = 0; i < num_frames; i++) {
        spu_tick(&out[i * 2]);
    }
}

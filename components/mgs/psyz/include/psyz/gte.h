#ifndef PSYZ_GTE_H
#define PSYZ_GTE_H
#include <libgte.h>

/**
 * @file gte.h
 * @brief Geometry Transformation Engine (COP2) endpoints.
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Read a GTE data register (COP2 data)
 *
 * The PS1 GTE (COP2) exposes 32 data registers holding input vectors,
 * accumulators, screen XY/Z FIFOs, and MAC values. Register indices follow
 * https://psx-spx.consoledev.net/geometrytransformationenginegte/
 *
 * @param reg Register index (0-31)
 * @return Register value packed as a 32-bit word
 */
unsigned int Psyz_GteDataRead(unsigned reg);

/**
 * @brief Write a GTE data register (COP2 data)
 *
 * Writes a 32-bit word into the GTE data register file. Used by an external
 * emulator to forward MTC2/LWC2 instructions into PsyZ's GTE state.
 * Register indices follow
 * https://psx-spx.consoledev.net/geometrytransformationenginegte/
 *
 * @param reg Register index (0-31)
 * @param value Value to write
 */
void Psyz_GteDataWrite(unsigned reg, unsigned int value);

/**
 * @brief Read a GTE control register (COP2 control)
 *
 * The GTE control registers hold transformation matrices, translation vectors,
 * projection parameters, and the FLAG register. Register indices follow
 * https://psx-spx.consoledev.net/geometrytransformationenginegte/
 *
 * @param reg Register index (0-31)
 * @return Register value packed as a 32-bit word
 */
unsigned int Psyz_GteCtrlRead(unsigned reg);

/**
 * @brief Write a GTE control register (COP2 control)
 *
 * Writes a 32-bit word into the GTE control register file. Used by an external
 * emulator to forward CTC2 instructions into PsyZ's GTE state.
 * Register indices follow
 * https://psx-spx.consoledev.net/geometrytransformationenginegte/
 *
 * @param reg Register index (0-31)
 * @param value Value to write
 */
void Psyz_GteCtrlWrite(unsigned reg, unsigned int value);

/**
 * @brief Execute a GTE command (COP2 instruction)
 *
 * Dispatches a 25-bit GTE command to the corresponding operation. Bits 0-5
 * select the operation (e.g. RTPS, RTPT, NCLIP, AVSZ3, AVSZ4). Used by an
 * external emulator to forward COP2 instructions into PsyZ's GTE state.
 * https://psx-spx.consoledev.net/geometrytransformationenginegte/
 *
 * @param cmd 25-bit GTE command word
 */
void Psyz_GteCommand(unsigned int cmd);

void Psyz_GteLdRgb(CVECTOR* v);
void Psyz_GteStRgb(CVECTOR* v);
void Psyz_GteLdClmv(void* p);
void Psyz_GteStClmv(void* p);
void Psyz_GteLdTr(long tx, long ty, long tz);
void Psyz_GteLdTx(long v);
void Psyz_GteLdTy(long v);
void Psyz_GteLdTz(long v);
void Psyz_GteAvsz3(void);
void Psyz_GteDpcs(void);
void Psyz_GteLcir(void);
void Psyz_GteRtps(void);
void Psyz_GteRtpt(void);
void Psyz_GteNclip(void);
void Psyz_GteLdv0(SVECTOR* v);
void Psyz_GteLdv3(SVECTOR* v0, SVECTOR* v1, SVECTOR* v2);
void Psyz_GteLdv01c(SVECTOR* v);
void Psyz_GteLdv3c(SVECTOR* v);
void Psyz_GteStsxy(unsigned int* out);
void Psyz_GteStsxy3(unsigned int* out0, unsigned int* out1, unsigned int* out2);
void Psyz_GteStsxy01c(unsigned int* out);
void Psyz_GteStsxy3Gt3(void* polyGt3);
void Psyz_GteStszotz(unsigned int* out);
void Psyz_GteStopz(int* out);

#ifdef __cplusplus
}
#endif

#endif

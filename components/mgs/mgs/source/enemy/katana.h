#ifndef __MGS_ENEMY_KATANA_H__
#define __MGS_ENEMY_KATANA_H__

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>

void *NewKatana(MATRIX *pos, SVECTOR *svec1, SVECTOR *svec2, int *field_44);
void *NewDemoKatana(MATRIX *pos);

#endif // __MGS_ENEMY_KATANA_H__

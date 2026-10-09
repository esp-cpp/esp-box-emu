#ifndef __MGS_OKAJIMA_BULLET_H__
#define __MGS_OKAJIMA_BULLET_H__

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>

enum {
    BULLET_FLASH            = 0x00000001,
    BULLET_SMOKE0           = 0x00000002,
    BULLET_SMOKE1           = 0x00000004,
    BULLET_FLASHSOCOM0      = 0x00000008,
    BULLET_FLASHSOCOM1      = 0x00000010,
    BULLET_FLASHHEAVY       = 0x00000020,
    BULLET_RECOILSPARK      = 0x00000100,
    BULLET_RECOILHEAVYSPARK = 0x00000200,
    BULLET_RECOILSMOKE      = 0x00000400,
    BULLET_BLAST            = 0x00000800,
    BULLET_NO_HZD           = 0x00001000,
};

void *NewBulletEnemy(MATRIX *world, int side, int color, int type, int test);
void *NewBullet(MATRIX *world, int side, int color, int type);
void *NewBulletEx(int flag, MATRIX *world, int side, int color, int type, int size, int damage, int length, int speed);
void *NewBullet2(MATRIX *world, int side, int color, int type, int size, int damage, int length, int speed);

#endif // __MGS_OKAJIMA_BULLET_H__

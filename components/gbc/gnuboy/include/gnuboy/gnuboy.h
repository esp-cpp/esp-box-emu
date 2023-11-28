#pragma once

void ev_poll();
void vid_close();
void vid_preinit();
// void vid_init(); // NOT NEEDED
void vid_begin();
void vid_end();
void vid_setpal(int i, int r, int g, int b);
void vid_settitle(char *title);

void sys_sleep(int us);
void *sys_timer();
int  sys_elapsed(void *in_ptr);

/* Sound */
void pcm_init();
int  pcm_submit();
void pcm_close();

void sys_checkdir(char *path, int wr);
void sys_sanitize(char *s);
void sys_initpath(char *exe);
void doevents();
void die(char *fmt, ...);

/* emu.c */
void emu_reset();
void emu_run();
void emu_step();

/* hw.c */
#include "gnuboy/defs.h" /* need byte for below */
void hw_interrupt(byte i, byte mask);

/* save.c */
#include <stdio.h> /* need FILE for below */
void savestate(FILE *f);
void loadstate(FILE *f);

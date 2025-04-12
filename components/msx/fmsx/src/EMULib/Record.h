/** EMULib Emulation Library *********************************/
/**                                                         **/
/**                        Record.h                         **/
/**                                                         **/
/** This file contains routines for gameplay recording and  **/
/** replay.                                                 **/
/**                                                         **/
/** Copyright (C) Marat Fayzullin 2013-2021                 **/
/**     The contents of this file are property of Marat     **/
/**     Fayzullin and should only be used as agreed with    **/
/**     him. The file is confidential. Absolutely no        **/
/**     distribution allowed.                               **/
/*************************************************************/
#ifndef RECORD_H
#define RECORD_H

#include "EMULib.h"

#ifdef __cplusplus
extern "C" {
#endif

/** RPLPlay()/RPLRecord() arguments **************************/
#define RPL_OFF     0xFFFFFFFF       /* Disable              */
#define RPL_ON      0xFFFFFFFE       /* Enable               */
#define RPL_TOGGLE  0xFFFFFFFD       /* Toggle               */
#define RPL_RESET   0xFFFFFFFC       /* Reset and enable     */
#define RPL_NEXT    0xFFFFFFFB       /* Play the next record */
#define RPL_QUERY   0xFFFFFFFA       /* Query state          */

/** RPLPlay(RPL_NEXT) results ********************************/
#define RPL_ENDED   0xFFFFFFFF       /* Finished or stopped  */

#define RPL_RECSIZE  (RPL_STEP+1)
#define RPL_BUFSIZE  64
#define RPL_STEP     10
#define RPL_SIGNSIZE 12

typedef struct
{
  unsigned char *State;
  unsigned int StateSize;
  unsigned int JoyState[RPL_RECSIZE];
  unsigned int Count[RPL_RECSIZE];
  unsigned char KeyState[RPL_RECSIZE][16];
} RPLState;

/** RPLInit() ************************************************/
/** Initialize record/relay subsystem.                      **/
/*************************************************************/
void RPLInit(unsigned int (*SaveHandler)(unsigned char *,unsigned int),unsigned int (*LoadHandler)(unsigned char *,unsigned int),unsigned int MaxSize);

/** RPLTrash() ***********************************************/
/** Free all record/replay resources.                       **/
/*************************************************************/
void RPLTrash(void);

/** RPLRecord() **********************************************/
/** Record emulation state and joystick input for replaying **/
/** it back later.                                          **/
/*************************************************************/
int RPLRecord(unsigned int JoyState);

/** RPLRecordKeys() ******************************************/
/** Record emulation state, keys, and joystick input for    **/
/** replaying them back later.                              **/
/*************************************************************/
int RPLRecordKeys(unsigned int JoyState,const unsigned char *Keys,unsigned int KeySize);

/** RPLPlay() ************************************************/
/** Replay gameplay saved with RPLRecord().                 **/
/*************************************************************/
unsigned int RPLPlay(int Cmd);

/** RPLPlayKeys() ********************************************/
/** Replay gameplay saved with RPLRecordKeys().             **/
/*************************************************************/
unsigned int RPLPlayKeys(int Cmd,unsigned char *Keys,unsigned int KeySize);

/** RPLCount() ***********************************************/
/** Compute the number of remaining replay records.         **/
/*************************************************************/
unsigned int RPLCount(void);

/** RPLShow() ************************************************/
/** Draw replay icon when active.                           **/
/*************************************************************/
void RPLShow(Image *Img,int X,int Y);

/** SaveRPL() ************************************************/
/** Save gameplay recording into given file.                **/
/*************************************************************/
int SaveRPL(const char *FileName);

/** LoadRPL() ************************************************/
/** Load gameplay recording from given file.                **/
/*************************************************************/
int LoadRPL(const char *FileName);

/** RPLControls() ********************************************/
/** Let user browse through replay states with directional  **/
/** buttons: LEFT:REW, RIGHT:FWD, DOWN:STOP, UP:CONTINUE.   **/
/*************************************************************/
unsigned int RPLControls(unsigned int Buttons);

#ifdef __cplusplus
}
#endif
#endif /* RECORD_H */

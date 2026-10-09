#ifndef KERNEL_H
#define KERNEL_H

#define DescMask 0xff000000
#define DescTH DescMask
#define DescHW 0xf0000000
#define DescEV 0xf1000000
#define DescRC 0xf2000000
#define DescUEV 0xf3000000 /* User event */
#define DescSW 0xf4000000  /* BIOS */

#define HwVBLANK (DescHW | 0x01) /* VBLANK */
#define HwGPU (DescHW | 0x02)    /* GPU */
#define HwCdRom (DescHW | 0x03)  /* CDROM Decorder */
#define HwDMAC (DescHW | 0x04)   /* DMA controller */
#define HwRTC0 (DescHW | 0x05)   /* RTC0 */
#define HwRTC1 (DescHW | 0x06)   /* RTC1 */
#define HwRTC2 (DescHW | 0x07)   /* RTC2 */
#define HwCNTL (DescHW | 0x08)   /* Controller */
#define HwSPU (DescHW | 0x09)    /* SPU */
#define HwPIO (DescHW | 0x0a)    /* PIO */
#define HwSIO (DescHW | 0x0b)    /* SIO */

#define HwCPU (DescHW | 0x10)    /* Exception */
#define HwCARD (DescHW | 0x11)   /* memory card */
#define HwCARD_0 (DescHW | 0x12) /* memory card */
#define HwCARD_1 (DescHW | 0x13) /* memory card */
#define SwCARD (DescSW | 0x01)   /* memory card */
#define SwMATH (DescSW | 0x02)   /* libmath */

#define RCntCNT0 (DescRC | 0x00) /* display pixel */
#define RCntCNT1 (DescRC | 0x01) /* horizontal sync */
#define RCntCNT2 (DescRC | 0x02) /* system clock divided by 8 */
#define RCntCNT3 (DescRC | 0x03) /* vertical sync, target value fixed at 1 */

#define RCntMdINTR 0x1000
#define RCntMdNOINTR 0x2000
#define RCntMdSC 0x0001
#define RCntMdSP 0x0000
#define RCntMdFR 0x0000
#define RCntMdGATE 0x0010

#define EvSpCZ 0x0001      /* counter becomes zero */
#define EvSpINT 0x0002     /* interrupted */
#define EvSpIOE 0x0004     /* end of i/o */
#define EvSpCLOSE 0x0008   /* file was closed */
#define EvSpACK 0x0010     /* command acknowledged */
#define EvSpCOMP 0x0020    /* command completed */
#define EvSpDR 0x0040      /* data ready */
#define EvSpDE 0x0080      /* data end */
#define EvSpTIMOUT 0x0100  /* time out */
#define EvSpUNKNOWN 0x0200 /* unknown command */
#define EvSpIOER 0x0400    /* end of read buffer */
#define EvSpIOEW 0x0800    /* end of write buffer */
#define EvSpTRAP 0x1000    /* general interrupt */
#define EvSpNEW 0x2000     /* new device */
#define EvSpSYSCALL 0x4000 /* system call instruction */
#define EvSpERROR 0x8000   /* error happned */
#define EvSpPERROR 0x8001  /* previous write error happned */
#define EvSpEDOM 0x0301    /* domain error in libmath */
#define EvSpERANGE 0x0302  /* range error in libmath */

#define EvMdINTR 0x1000
#define EvMdNOINTR 0x2000

#define EvStUNUSED 0x0000
#define EvStWAIT 0x1000
#define EvStACTIVE 0x2000
#define EvStALREADY 0x4000

#define TcbMdRT 0x1000  /* reserved by system */
#define TcbMdPRI 0x2000 /* reserved by system */

#define TcbStUNUSED 0x1000
#define TcbStACTIVE 0x4000

/**
 * @brief Event Control Block
 *
 * Stores information for each event.
 */
struct EvCB {
    unsigned int desc;  /**< Cause descriptor */
    int status;         /**< Status */
    int spec;           /**< Event type */
    int mode;           /**< Mode */
    long (*FHandler)(); /**< Pointer to a function type handler */
    int system[2];      /**< Reserved by system */
};

/**
 * @brief Task Control Block
 *
 * Stores a context (including contents of the registers) for thread management.
 */
#define NREGS 32
struct TCB {
    int status;               /**< Status */
    int mode;                 /**< Mode */
    unsigned long reg[NREGS]; /**< Register saving area (specified by register
                                 designation macro) */
    int system[6];            /**< Reserved by system */
};

/**
 * @brief Task Control Block Header
 *
 * Used for thread management. entry is a pointer to the currently executing
 * TCB.
 */
struct TCBH {
    struct TCB* entry; /**< Pointer to execution TCB */
    int flag;          /**< System reserved */
};

/**
 * @brief System Table Information
 *
 * Information about various system tables used by the kernel. The tables begin
 * at address 0x00000100.
 */
struct ToT {
    unsigned long* head; /**< Pointer to a system table start address */
    int size;            /**< System table size (in bytes) */
};

/**
 * @brief Execution file data structure
 *
 * Stores information for loading and executing a program. The data is stored in
 * the first 2K bytes of the execution file (PS-X EXE format). By adding stack
 * information and transfering it to Exec(), the program is activated.
 */
struct EXEC {
    unsigned long pc0;    /**< Execution start address */
    unsigned long gp0;    /**< gp register initial value */
    unsigned long t_addr; /**< Starting address of initialized text section */
    unsigned long t_size; /**< Size of text section */
    unsigned long d_addr; /**< Starting address of initialized data section */
    unsigned long d_size; /**< Size of initialized data section */
    unsigned long b_addr; /**< Uninitialized data section start address */
    unsigned long b_size; /**< Uninitialized data section size */
    unsigned long s_addr; /**< Stack start address (specified by the user) */
    unsigned long s_size; /**< Stack size (specified by the user) */
    unsigned long sp;     /**< Register shunt variable */
    unsigned long fp;     /**< Register shunt variable */
    unsigned long gp;     /**< Register shunt variable */
    unsigned long ret;    /**< Register shunt variable */
    unsigned long base;   /**< Register shunt variable */
};

/**
 * @brief Directory entries
 *
 * Stores information relating to files registered in the file system.
 */
struct DIRENTRY {
    char name[20];         /**< Filename */
    int attr;              /**< Attributes (dependent on file system) */
    int size;              /**< File size (in bytes) */
    struct DIRENTRY* next; /**< Pointer to next file entry (for user) */
    char system[8];        /**< Reserved by system */
};

// EnterCriticalSection/ExitCriticalSection collide with the Win32 API.
// To avoid collision and inconsistency, the PS1_ prefix is added.
void PS1_EnterCriticalSection(void);
void PS1_ExitCriticalSection(void);
#undef EnterCriticalSection
#undef ExitCriticalSection
#define EnterCriticalSection PS1_EnterCriticalSection
#define ExitCriticalSection PS1_ExitCriticalSection

#endif

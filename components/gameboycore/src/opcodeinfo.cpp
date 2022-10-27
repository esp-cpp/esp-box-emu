
#include "gameboycore/opcodeinfo.h"

namespace gb
{
    static const OpcodeInfo opcodeinfo1[] = {
        { 4, "nop"       }, // 0x00
        { 12, "ld BC,%04X", OperandType::IMM16      }, // 0x01
        { 8, "ld (BC),A" }, // 0x02
        { 8, "inc BC"    }, // 0x03
        { 4, "inc B"    }, // 0x04
        { 4, "dec B"      }, // 0x05
        { 8, "ld B,%02X",  OperandType::IMM8     }, // 0x06
        { 4, "rlca",      }, // 0x07
        { 20, "ld (%04X)SP", OperandType::IMM16 }, // 0x08
        { 8, "add HL,BC",  }, // 0x09
        { 8, "ld A,(BC)", }, // 0x0A
        { 4, "dec BC"    }, // 0x0B
        { 4, "inc C"      }, // 0x0C
        { 4, "dec C"     }, // 0x0D
        { 8, "ld C,%02X",  OperandType::IMM8 }, // 0x0E
        { 4, "rrca",       }, // 0x0F

        { 4, "stop %02X", OperandType::IMM8       }, // 0x10
        { 12, "ld DE,%04X",   OperandType::IMM16 }, // 0x11
        { 8, "ld (DE),A",  }, // 0x12
        { 8, "inc DE",     }, // 0x13
        { 4, "inc D",      }, // 0x14
        { 4, "dec D",      }, // 0x15
        { 8, "ld D,%02X", OperandType::IMM8 }, // 0x16
        { 4, "rla",        }, // 0x17
        { 12, "jr %02X", OperandType::IMM8 }, // 0x18
        { 8, "add HL,DE",  }, // 0x19
        { 8, "ld A,(DE)",  }, // 0x1A
        { 8, "dec DE",     }, // 0x1B
        { 4, "inc E",      }, // 0x1C
        { 4, "dec E",      }, // 0x1D
        { 8, "ld E,%02X",  OperandType::IMM8 }, // 0x1E
        { 4, "rra",        }, // 0x1F

        { 8, "jr NZ,%02X", OperandType::IMM8      }, // 0x20
        { 12, "ld HL,%04X",  OperandType::IMM16 }, // 0x21
        { 8, "ld (HL+),A", }, // 0x22
        { 8, "inc HL",     }, // 0x23
        { 4, "inc H",      }, // 0x24
        { 4, "dec H",      }, // 0x25
        { 8, "ld H,%02X", OperandType::IMM8 }, // 0x26
        { 4, "daa",        }, // 0x27
        { 8, "jr Z,%02X",  OperandType::IMM8 }, // 0x28
        { 8, "add HL,HL",  }, // 0x29
        { 8, "ld A,(HL+)", }, // 0x2A
        { 8, "dec HL",     }, // 0x2B
        { 4, "inc L",      }, // 0x2C
        { 4, "dec L",      }, // 0x2D
        { 8, "ld L,%02X",  OperandType::IMM8 }, // 0x2E
        { 4, "cpl",        }, // 0x2F

        { 8, "jr NC,%02X",  OperandType::IMM8 }, // 0x30
        { 12, "ld SP,%04X", OperandType::IMM16 }, // 0x31
        { 8, "ld (HL-),A", }, // 0x32
        { 8, "inc SP",     }, // 0x33
        { 12, "inc (HL)",    }, // 0x34
        { 12, "dec (HL)",   }, // 0x35
        { 12, "ld (HL),%02X", OperandType::IMM8   }, // 0x36
        { 4, "scf",        }, // 0x37
        { 8, "jr C,%02X",  OperandType::IMM8 }, // 0x38
        { 8, "add HL,SP", }, // 0x39
        { 8, "ld A,(HL-)", }, // 0x3A
        { 8, "dec SP",     }, // 0x3B
        { 4, "inc A",      }, // 0x3C
        { 4, "dec A",      }, // 0x3D
        { 8, "ld A,%02X", OperandType::IMM8 }, // 0x3E
        { 4, "ccf",        }, // 0x3F

        { 4, "ld B,B",     }, // 0x40
        { 4, "ld B,C",     }, // 0x41
        { 4, "ld B,D",     }, // 0x42
        { 4, "ld B,E",     }, // 0x43
        { 4, "ld B,H",     }, // 0x44
        { 4, "ld B,L",     }, // 0x45
        { 8, "ld B,(HL)",  }, // 0x46
        { 4, "ld B,A",     }, // 0x47
        { 4, "ld C,B",     }, // 0x48
        { 4, "ld C,C",     }, // 0x49
        { 4, "ld C,D",     }, // 0x4A
        { 4, "ld C,E",     }, // 0x4B
        { 4, "ld C,H",     }, // 0x4C
        { 4, "ld C,L",     }, // 0x4D
        { 8, "ld C,(HL)",  }, // 0x4E
        { 4, "ld C,A",     }, // 0x4F

        { 4, "ld D,B",     }, // 0x50
        { 4, "ld D,C",     }, // 0x51
        { 4, "ld D,D",     }, // 0x52
        { 4, "ld D,E",     }, // 0x53
        { 4, "ld D,H",     }, // 0x54
        { 4, "ld D,L",     }, // 0x55
        { 8, "ld D,(HL)",  }, // 0x56
        { 4, "ld D,A",     }, // 0x57
        { 4, "ld E,B",     }, // 0x58
        { 4, "ld E,C",     }, // 0x59
        { 4, "ld E,D",     }, // 0x5A
        { 4, "ld E,E",     }, // 0x5B
        { 4, "ld E,H",     }, // 0x5C
        { 4, "ld E,L",     }, // 0x5D
        { 8, "ld E,(HL)",  }, // 0x5E
        { 4, "ld E,A",     }, // 0x5F

        { 4, "ld H,B",     }, // 0x60
        { 4, "ld H,C",     }, // 0x61
        { 4, "ld H,D",     }, // 0x62
        { 4, "ld H,E",     }, // 0x63
        { 4, "ld H,H",     }, // 0x64
        { 4, "ld H,L",     }, // 0x65
        { 8, "ld H,(HL)",  }, // 0x66
        { 4, "ld H,A",     }, // 0x67
        { 4, "ld L,B",     }, // 0x68
        { 4, "ld L,C",     }, // 0x69
        { 4, "ld L,D",     }, // 0x6A
        { 4, "ld L,E",     }, // 0x6B
        { 4, "ld L,H",     }, // 0x6C
        { 4, "ld L,L",     }, // 0x6D
        { 8, "ld L,(HL)",  }, // 0x6E
        { 4, "ld L,A",     }, // 0x6F

        { 8, "ld (HL),B",  }, // 0x70
        { 8, "ld (HL),C",  }, // 0x71
        { 8, "ld (HL),D",  }, // 0x72
        { 8, "ld (HL),E",  }, // 0x73
        { 8, "ld (HL),H",  }, // 0x74
        { 8, "ld (HL),L",  }, // 0x75
        { 4, "halt",       }, // 0x76
        { 8, "ld (HL),A",  }, // 0x77
        { 4, "ld A,B",     }, // 0x78
        { 4, "ld A,C",     }, // 0x79
        { 4, "ld A,D",     }, // 0x7A
        { 4, "ld A,E",     }, // 0x7B
        { 4, "ld A,H",     }, // 0x7C
        { 4, "ld A,L",     }, // 0x7D
        { 4, "ld A,(HL)",  }, // 0x7E
        { 4, "ld A,A",     }, // 0x7F

        { 4, "add A,B",    }, // 0x80
        { 4, "add A,C",    }, // 0x81
        { 4, "add A,D",    }, // 0x82
        { 4, "add A,E",    }, // 0x83
        { 4, "add A,H",    }, // 0x84
        { 4, "add A,L",    }, // 0x85
        { 8, "add A,(HL)", }, // 0x86
        { 4, "add A,A",    }, // 0x87
        { 4, "adc A,B",    }, // 0x88
        { 4, "adc A,C",    }, // 0x89
        { 4, "adc A,D",    }, // 0x8A
        { 4, "adc A,E",    }, // 0x8B
        { 4, "adc A,H",    }, // 0x8C
        { 4, "adc A,L",    }, // 0x8D
        { 8, "adc A,(HL)", }, // 0x8E
        { 4, "adc A,A",    }, // 0x8F

        { 4, "sub B",      }, // 0x90
        { 4, "sub C",      }, // 0x91
        { 4, "sub D",      }, // 0x92
        { 4, "sub E",      }, // 0x93
        { 4, "sub H",      }, // 0x94
        { 4, "sub L",      }, // 0x95
        { 8, "sub (HL)",   }, // 0x96
        { 4, "sub A",      }, // 0x97
        { 4, "sbc A,B",   }, // 0x98
        { 4, "sbc A,C",   }, // 0x99
        { 4, "sbc A,D",   }, // 0x9A
        { 4, "sbc A,E",   }, // 0x9B
        { 4, "sbc A,H",   }, // 0x9C
        { 4, "sbc A,L",   }, // 0x9D
        { 8, "sbc A,(HL)",}, // 0x9E
        { 4, "sbc A,A",   }, // 0x9F

        { 4, "and B",      }, // 0xA0
        { 4, "and C",      }, // 0xA1
        { 4, "and D",      }, // 0xA2
        { 4, "and E",      }, // 0xA3
        { 4, "and H",      }, // 0xA4
        { 4, "and L",      }, // 0xA5
        { 8, "and (HL)",   }, // 0xA6
        { 4, "and A",      }, // 0xA7
        { 4, "xor B",      }, // 0xA8
        { 4, "xor C",      }, // 0xA9
        { 4, "xor D",      }, // 0xAA
        { 4, "xor E",      }, // 0xAB
        { 4, "xor H",      }, // 0xAC
        { 4, "xor L",      }, // 0xAD
        { 8, "xor (HL)",   }, // 0xAE
        { 4, "xor A",      }, // 0xAF

        { 4, "or B",       }, // 0xB0
        { 4, "or C",       }, // 0xB1
        { 4, "or D",       }, // 0xB2
        { 4, "or E",       }, // 0xB3
        { 4, "or H",       }, // 0xB4
        { 4, "or L",       }, // 0xB5
        { 8, "or (HL)",    }, // 0xB6
        { 4, "or A",       }, // 0xB7
        { 4, "cp B",       }, // 0xB8
        { 4, "cp C",       }, // 0xB9
        { 4, "cp D",       }, // 0xBA
        { 4, "cp E",       }, // 0xBB
        { 4, "cp H",       }, // 0xBC
        { 4, "cp L",       }, // 0xBD
        { 4, "cp (HL)",    }, // 0xBE
        { 4, "cp A",       }, // 0xBF

        { 8, "ret NZ",     }, // 0xC0
        { 12, "pop BC",     }, // 0xC1
        { 12, "jp NZ,%04X", OperandType::IMM16 }, // 0xC2
        { 16, "jp %04X",      OperandType::IMM16 }, // 0xC3
        { 12, "call NZ,%04X",  OperandType::IMM16 }, // 0xC4
        { 16, "push BC",    }, // 0xC5
        { 8, "add A,%02X",   OperandType::IMM8 }, // 0xC6
        { 16, "rst 00",     }, // 0xC7
        { 8, "ret Z",      }, // 0xC8
        { 8, "ret",        }, // 0xC9
        { 12, "jp Z,%04X", OperandType::IMM16 }, // 0xCA
        { 0, "prefix"       }, // 0xCB
        { 12, "call Z,%04X", OperandType::IMM16 }, // 0xCC
        { 24, "call %04X",   OperandType::IMM16 }, // 0xCD
        { 8, "adc A,%02X",  OperandType::IMM8 }, // 0xCE
        { 16, "rst 08h",    }, // 0xCF

        { 8, "ret NC",     }, // 0xD0
        { 12, "pop DE",     }, // 0xD1
        { 12, "jp NC,%04X", OperandType::IMM16 }, // 0xD2
        { 0, "invalid"     }, // 0xD3
        { 12, "call NC,%04X",   OperandType::IMM16 }, // 0xD4
        { 16, "push DE",    }, // 0xD5
        { 8, "sub %02X", OperandType::IMM8       }, // 0xD6
        { 16, "rst 10",      }, // 0xD7
        { 8, "ret C",      }, // 0xD8
        { 16, "reti",       }, // 0xD9
        { 12, "jp C,%04X", OperandType::IMM16 }, // 0xDA
        { 0, "invalid"     }, // 0xDB
        { 12, "call C,%04X", OperandType::IMM16 }, // 0xDC
        { 0, "invalid"     }, // 0xDD
        { 8, "sbc A,%02X", OperandType::IMM8 }, // 0xDE
        { 16, "rst 18",     }, // 0xDF

        { 12, "ldh (%02X),A", OperandType:: IMM8}, // 0xE0
        { 12, "pop HL",     }, // 0xE1
        { 8, "ld (C),A",   }, // 0xE2
        { 0, "invalid"     }, // 0xE3
        { 0, "invalid"     }, // 0xE4
        { 16, "push HL",    }, // 0xE5
        { 8, "and %02X", OperandType::IMM8 }, // 0xE6
        { 16, "rst 20",     }, // 0xE7
        { 8, "add SP,%02X",  OperandType::IMM8 }, // 0xE8
        { 4, "jp (HL)",    }, // 0xE9
        { 16, "ld (%04X),A", OperandType:: IMM16}, // 0xEA
        { 0, "invalid"     }, // 0xEB
        { 0, "invalid"     }, // 0xEC
        { 0, "invalid"     }, // 0xED
        { 8, "xor %02X",  OperandType::IMM8 }, // 0xEE
        { 16, "rst 28",     }, // 0xEF

        { 12, "ldh A,(%02X)", OperandType::IMM8 }, // 0xF0
        { 12, "pop AF",      }, // 0xF1
        { 8, "ld (A),C",    }, // 0xF2
        { 4, "di",          }, // 0xF3
        { 0, "invalid"      }, // 0xF4
        { 16, "push AF",     }, // 0xF5
        { 8, "or %02X",  OperandType::IMM8 }, // 0xF6
        { 16, "rst 30",      }, // 0xF7
        { 12, "ld HL,SP+r8", }, // 0xF8
        { 8, "ld SP,HL",    }, // 0xF9
        { 16, "ld A,(%04X)", OperandType::IMM16 }, // 0xFA
        { 4, "ei",          }, // 0xFB
        { 0, "invalid"      }, // 0xFC
        { 0, "invalid"      }, // 0xFD
        { 8, "cp %02X",  OperandType::IMM8 }, // 0xFE
        { 16, "rst 38"       }  // 0xFF
    };

    static const OpcodeInfo opcodeinfo2[] = {
        { 8, "RLC B" }, // 00
        { 8, "RLC C" }, // 01
        { 8, "RLC D" }, // 02
        { 8, "RLC E" }, // 03
        { 8, "RLC H" }, // 04
        { 8, "RLC L" },     // 05
        { 16, "RLC (HL)" }, // 06
        { 8, "RLC A" },     // 07

        { 8, "RRC B" }, // 08
        { 8, "RRC C" }, // 09
        { 8, "RRC D" }, // 0A
        { 8, "RRC E" }, // 0B
        { 8, "RRC H" }, // 0C
        { 8, "RRC L" }, // 0D
        { 16, "RRC (HL)" }, // 0E
        { 8, "RRC A" }, // 0F

        { 8, "RL B" }, // 10
        { 8, "RL C" }, // 11
        { 8, "RL D" }, // 12
        { 8, "RL E" },
        { 8, "RL H" },
        { 8, "RL L" },
        { 16, "RL (HL)" },
        { 8, "RL A" },

        { 8, "RR B" },
        { 8, "RR C" },
        { 8, "RR D" },
        { 8, "RR E" },
        { 8, "RR H" },
        { 8, "RR L" },
        { 16, "RR (HL)" },
        { 8, "RR A" },

        { 8, "SLA B" },
        { 8, "SLA C" },
        { 8, "SLA D" },
        { 8, "SLA E" },
        { 8, "SLA H" },
        { 8, "SLA L" },
        { 16, "SLA (HL)" },
        { 8, "SLA A" },

        { 8, "SRA B" },
        { 8, "SRA C" },
        { 8, "SRA D" },
        { 8, "SRA E" },
        { 8, "SRA H" },
        { 8, "SRA L" },
        { 16, "SRA (HL)" },
        { 8, "SRA A" },

        { 8, "SWAP B" },
        { 8, "SWAP C" },
        { 8, "SWAP D" },
        { 8, "SWAP E" },
        { 8, "SWAP H" },
        { 8, "SWAP L" },
        { 16, "SWAP (HL)" },
        { 8, "SWAP A" },

        { 8, "SRL B" },
        { 8, "SRL C" },
        { 8, "SRL D" },
        { 8, "SRL E" },
        { 8, "SRL H" },
        { 8, "SRL L" },
        { 16, "SRL (HL)" },
        { 8, "SRL A" },

        { 8, "BIT 0,B" },
        { 8, "BIT 0,C" },
        { 8, "BIT 0,D" },
        { 8, "BIT 0,E" },
        { 8, "BIT 0,H" },
        { 8, "BIT 0,L" },
        { 16, "BIT 0,(HL)" },
        { 8, "BIT 0,A" },

        { 8, "BIT 1,B" },
        { 8, "BIT 1,C" },
        { 8, "BIT 1,D" },
        { 8, "BIT 1,E" },
        { 8, "BIT 1,H" },
        { 8, "BIT 1,L" },
        { 16, "BIT 1,(HL)" },
        { 8, "BIT 1,A" },

        { 8, "BIT 2,B" },
        { 8, "BIT 2,C" },
        { 8, "BIT 2,D" },
        { 8, "BIT 2,E" },
        { 8, "BIT 2,H" },
        { 8, "BIT 2,L" },
        { 16, "BIT 2,(HL)" },
        { 8, "BIT 2,A" },

        { 8, "BIT 3,B" },
        { 8, "BIT 3,C" },
        { 8, "BIT 3,D" },
        { 8, "BIT 3,E" },
        { 8, "BIT 3,H" },
        { 8, "BIT 3,L" },
        { 16, "BIT 3,(HL)" },
        { 8, "BIT 3,A" },

        { 8, "BIT 4,B" },
        { 8, "BIT 4,C" },
        { 8, "BIT 4,D" },
        { 8, "BIT 4,E" },
        { 8, "BIT 4,H" },
        { 8, "BIT 4,L" },
        { 16, "BIT 4,(HL)" },
        { 8, "BIT 4,A" },

        { 8, "BIT 5,B" },
        { 8, "BIT 5,C" },
        { 8, "BIT 5,D" },
        { 8, "BIT 5,E" },
        { 8, "BIT 5,H" },
        { 8, "BIT 5,L" },
        { 16, "BIT 5,(HL)" },
        { 8, "BIT 5,A" },

        { 8, "BIT 6,B" },
        { 8, "BIT 6,C" },
        { 8, "BIT 6,D" },
        { 8, "BIT 6,E" },
        { 8, "BIT 6,H" },
        { 8, "BIT 6,L" },
        { 16, "BIT 6,(HL)" },
        { 8, "BIT 6,A" },

        { 8, "BIT 7,B" },
        { 8, "BIT 7,C" },
        { 8, "BIT 7,D" },
        { 8, "BIT 7,E" },
        { 8, "BIT 7,H" },
        { 8, "BIT 7,L" },
        { 16, "BIT 7,(HL)" },
        { 8, "BIT 7,A" },

            //

        { 8, "RES 0,B" },
        { 8, "RES 0,C" },
        { 8, "RES 0,D" },
        { 8, "RES 0,E" },
        { 8, "RES 0,H" },
        { 8, "RES 0,L" },
        { 16, "RES 0,(HL)" },
        { 8, "RES 0,A" },

        { 8, "RES 1,B" },
        { 8, "RES 1,C" },
        { 8, "RES 1,D" },
        { 8, "RES 1,E" },
        { 8, "RES 1,H" },
        { 8, "RES 1,L" },
        { 16, "RES 1,(HL)" },
        { 8, "RES 1,A" },

        { 8, "RES 2,B" },
        { 8, "RES 2,C" },
        { 8, "RES 2,D" },
        { 8, "RES 2,E" },
        { 8, "RES 2,H" },
        { 8, "RES 2,L" },
        { 16, "RES 2,(HL)" },
        { 8, "RES 2,A" },

        { 8, "RES 3,B" },
        { 8, "RES 3,C" },
        { 8, "RES 3,D" },
        { 8, "RES 3,E" },
        { 8, "RES 3,H" },
        { 8, "RES 3,L" },
        { 16, "RES 3,(HL)" },
        { 8, "RES 3,A" },

        { 8, "RES 4,B" },
        { 8, "RES 4,C" },
        { 8, "RES 4,D" },
        { 8, "RES 4,E" },
        { 8, "RES 4,H" },
        { 8, "RES 4,L" },
        { 16, "RES 4,(HL)" },
        { 8, "RES 4,A" },

        { 8, "RES 5,B" },
        { 8, "RES 5,C" },
        { 8, "RES 5,D" },
        { 8, "RES 5,E" },
        { 8, "RES 5,H" },
        { 8, "RES 5,L" },
        { 16, "RES 5,(HL)" },
        { 8, "RES 5,A" },

        { 8, "RES 6,B" },
        { 8, "RES 6,C" },
        { 8, "RES 6,D" },
        { 8, "RES 6,E" },
        { 8, "RES 6,H" },
        { 8, "RES 6,L" },
        { 16, "RES 6,(HL)" },
        { 8, "RES 6,A" },

        { 8, "RES 7,B" },
        { 8, "RES 7,C" },
        { 8, "RES 7,D" },
        { 8, "RES 7,E" },
        { 8, "RES 7,H" },
        { 8, "RES 7,L" },
        { 16, "RES 7,(HL)" },
        { 8, "RES 7,A" },
            //

        { 8, "SET 0,B" },
        { 8, "SET 0,C" },
        { 8, "SET 0,D" },
        { 8, "SET 0,E" },
        { 8, "SET 0,H" },
        { 8, "SET 0,L" },
        { 16, "SET 0,(HL)" },
        { 8, "SET 0,A" },

        { 8, "SET 1,B" },
        { 8, "SET 1,C" },
        { 8, "SET 1,D" },
        { 8, "SET 1,E" },
        { 8, "SET 1,H" },
        { 8, "SET 1,L" },
        { 16, "SET 1,(HL)" },
        { 8, "SET 1,A" },

        { 8, "SET 2,B" },
        { 8, "SET 2,C" },
        { 8, "SET 2,D" },
        { 8, "SET 2,E" },
        { 8, "SET 2,H" },
        { 8, "SET 2,L" },
        { 16, "SET 2,(HL)" },
        { 8, "SET 2,A" },

        { 8, "SET 3,B" },
        { 8, "SET 3,C" },
        { 8, "SET 3,D" },
        { 8, "SET 3,E" },
        { 8, "SET 3,H" },
        { 8, "SET 3,L" },
        { 16, "SET 3,(HL)" },
        { 8, "SET 3,A" },

        { 8, "SET 4,B" },
        { 8, "SET 4,C" },
        { 8, "SET 4,D" },
        { 8, "SET 4,E" },
        { 8, "SET 4,H" },
        { 8, "SET 4,L" },
        { 16, "SET 4,(HL)" },
        { 8, "SET 4,A" },

        { 8, "SET 5,B" },
        { 8, "SET 5,C" },
        { 8, "SET 5,D" },
        { 8, "SET 5,E" },
        { 8, "SET 5,H" },
        { 8, "SET 5,L" },
        { 16, "SET 5,(HL)" },
        { 8, "SET 5,A" },

        { 8, "SET 6,B" },
        { 8, "SET 6,C" },
        { 8, "SET 6,D" },
        { 8, "SET 6,E" },
        { 8, "SET 6,H" },
        { 8, "SET 6,L" },
        { 16, "SET 6,(HL)" },
        { 8, "SET 6,A" },

        { 8, "SET 7,B" },
        { 8, "SET 7,C" },
        { 8, "SET 7,D" },
        { 8, "SET 7,E" },
        { 8, "SET 7,H" },
        { 8, "SET 7,L" },
        { 16, "SET 7,(HL)" },
        { 8, "SET 7,A" },

    };

    OpcodeInfo getOpcodeInfo(uint8_t opcode, OpcodePage page)
    {
        if (page == OpcodePage::PAGE1)
        {
            return opcodeinfo1[opcode];
        }
        else // page == OpcodePage::PAGE2
        {
            return opcodeinfo2[opcode];
        }
    }
}

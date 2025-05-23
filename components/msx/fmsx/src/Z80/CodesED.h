/** Z80: portable Z80 emulator *******************************/
/**                                                         **/
/**                         CodesED.h                       **/
/**                                                         **/
/** This file contains implementation for the ED table of   **/
/** Z80 commands. It is included from Z80.c.                **/
/**                                                         **/
/** Copyright (C) Marat Fayzullin 1994-2021                 **/
/**     You are not allowed to distribute this software     **/
/**     commercially. Please, notify me, if you make any    **/
/**     changes to this file.                               **/
/*************************************************************/

/** This is a special patch for emulating BIOS calls: ********/
case DB_FE:     PatchZ80(R);break;
/*************************************************************/

case ADC_HL_BC: M_ADCW(BC);break;
case ADC_HL_DE: M_ADCW(DE);break;
case ADC_HL_HL: M_ADCW(HL);break;
case ADC_HL_SP: M_ADCW(SP);break;

case SBC_HL_BC: M_SBCW(BC);break;
case SBC_HL_DE: M_SBCW(DE);break;
case SBC_HL_HL: M_SBCW(HL);break;
case SBC_HL_SP: M_SBCW(SP);break;

case LD_xWORDe_HL:
  J.B.l=OpZ80(R->PC.W++);
  J.B.h=OpZ80(R->PC.W++);
  msx_WrZ80(J.W++,R->HL.B.l);
  msx_WrZ80(J.W,R->HL.B.h);
  break;
case LD_xWORDe_DE:
  J.B.l=OpZ80(R->PC.W++);
  J.B.h=OpZ80(R->PC.W++);
  msx_WrZ80(J.W++,R->DE.B.l);
  msx_WrZ80(J.W,R->DE.B.h);
  break;
case LD_xWORDe_BC:
  J.B.l=OpZ80(R->PC.W++);
  J.B.h=OpZ80(R->PC.W++);
  msx_WrZ80(J.W++,R->BC.B.l);
  msx_WrZ80(J.W,R->BC.B.h);
  break;
case LD_xWORDe_SP:
  J.B.l=OpZ80(R->PC.W++);
  J.B.h=OpZ80(R->PC.W++);
  msx_WrZ80(J.W++,R->SP.B.l);
  msx_WrZ80(J.W,R->SP.B.h);
  break;

case LD_HL_xWORDe:
  J.B.l=OpZ80(R->PC.W++);
  J.B.h=OpZ80(R->PC.W++);
  R->HL.B.l=msx_RdZ80(J.W++);
  R->HL.B.h=msx_RdZ80(J.W);
  break;
case LD_DE_xWORDe:
  J.B.l=OpZ80(R->PC.W++);
  J.B.h=OpZ80(R->PC.W++);
  R->DE.B.l=msx_RdZ80(J.W++);
  R->DE.B.h=msx_RdZ80(J.W);
  break;
case LD_BC_xWORDe:
  J.B.l=OpZ80(R->PC.W++);
  J.B.h=OpZ80(R->PC.W++);
  R->BC.B.l=msx_RdZ80(J.W++);
  R->BC.B.h=msx_RdZ80(J.W);
  break;
case LD_SP_xWORDe:
  J.B.l=OpZ80(R->PC.W++);
  J.B.h=OpZ80(R->PC.W++);
  R->SP.B.l=msx_RdZ80(J.W++);
  R->SP.B.h=msx_RdZ80(J.W);
  break;

case RRD:
  I=msx_RdZ80(R->HL.W);
  J.B.l=(I>>4)|(R->AF.B.h<<4);
  msx_WrZ80(R->HL.W,J.B.l);
  R->AF.B.h=(I&0x0F)|(R->AF.B.h&0xF0);
  R->AF.B.l=PZSTable[R->AF.B.h]|(R->AF.B.l&C_FLAG);
  break;
case RLD:
  I=msx_RdZ80(R->HL.W);
  J.B.l=(I<<4)|(R->AF.B.h&0x0F);
  msx_WrZ80(R->HL.W,J.B.l);
  R->AF.B.h=(I>>4)|(R->AF.B.h&0xF0);
  R->AF.B.l=PZSTable[R->AF.B.h]|(R->AF.B.l&C_FLAG);
  break;

case LD_A_I:
  R->AF.B.h=R->I;
  R->AF.B.l=(R->AF.B.l&C_FLAG)|(R->IFF&IFF_2? P_FLAG:0)|ZSTable[R->AF.B.h];
  break;

case LD_A_R:
  R->AF.B.h=R->R;
  R->AF.B.l=(R->AF.B.l&C_FLAG)|(R->IFF&IFF_2? P_FLAG:0)|ZSTable[R->AF.B.h];
  break;

case LD_I_A:   R->I=R->AF.B.h;break;
case LD_R_A:   R->R=R->AF.B.h;break;

case IM_0:     R->IFF&=~(IFF_IM1|IFF_IM2);break;
case IM_1:     R->IFF=(R->IFF&~IFF_IM2)|IFF_IM1;break;
case IM_2:     R->IFF=(R->IFF&~IFF_IM1)|IFF_IM2;break;

case RETI:
case RETN:     if(R->IFF&IFF_2) R->IFF|=IFF_1; else R->IFF&=~IFF_1;
               M_RET;break;

case NEG:      I=R->AF.B.h;R->AF.B.h=0;M_SUB(I);break;

case IN_B_xC:  M_IN(R->BC.B.h);break;
case IN_C_xC:  M_IN(R->BC.B.l);break;
case IN_D_xC:  M_IN(R->DE.B.h);break;
case IN_E_xC:  M_IN(R->DE.B.l);break;
case IN_H_xC:  M_IN(R->HL.B.h);break;
case IN_L_xC:  M_IN(R->HL.B.l);break;
case IN_A_xC:  M_IN(R->AF.B.h);break;
case IN_F_xC:  M_IN(J.B.l);break;

case OUT_xC_B: msx_OutZ80(R->BC.W,R->BC.B.h);break;
case OUT_xC_C: msx_OutZ80(R->BC.W,R->BC.B.l);break;
case OUT_xC_D: msx_OutZ80(R->BC.W,R->DE.B.h);break;
case OUT_xC_E: msx_OutZ80(R->BC.W,R->DE.B.l);break;
case OUT_xC_H: msx_OutZ80(R->BC.W,R->HL.B.h);break;
case OUT_xC_L: msx_OutZ80(R->BC.W,R->HL.B.l);break;
case OUT_xC_A: msx_OutZ80(R->BC.W,R->AF.B.h);break;
case OUT_xC_F: msx_OutZ80(R->BC.W,0);break;

case INI:
  msx_WrZ80(R->HL.W++,msx_InZ80(R->BC.W));
  --R->BC.B.h;
  R->AF.B.l=N_FLAG|(R->BC.B.h? 0:Z_FLAG);
  break;

case INIR:
  msx_WrZ80(R->HL.W++,msx_InZ80(R->BC.W));
  if(--R->BC.B.h) { R->AF.B.l=N_FLAG;R->ICount-=21;R->PC.W-=2; }
  else            { R->AF.B.l=Z_FLAG|N_FLAG;R->ICount-=16; }
  break;

case IND:
  msx_WrZ80(R->HL.W--,msx_InZ80(R->BC.W));
  --R->BC.B.h;
  R->AF.B.l=N_FLAG|(R->BC.B.h? 0:Z_FLAG);
  break;

case INDR:
  msx_WrZ80(R->HL.W--,msx_InZ80(R->BC.W));
  if(!--R->BC.B.h) { R->AF.B.l=N_FLAG;R->ICount-=21;R->PC.W-=2; }
  else             { R->AF.B.l=Z_FLAG|N_FLAG;R->ICount-=16; }
  break;

case OUTI:
  --R->BC.B.h;
  I=msx_RdZ80(R->HL.W++);
  msx_OutZ80(R->BC.W,I);
  R->AF.B.l=N_FLAG|(R->BC.B.h? 0:Z_FLAG)|(R->HL.B.l+I>255? (C_FLAG|H_FLAG):0);
  break;

case OTIR:
  --R->BC.B.h;
  I=msx_RdZ80(R->HL.W++);
  msx_OutZ80(R->BC.W,I);
  if(R->BC.B.h)
  {
    R->AF.B.l=N_FLAG|(R->HL.B.l+I>255? (C_FLAG|H_FLAG):0);
    R->ICount-=21;
    R->PC.W-=2;
  }
  else
  {
    R->AF.B.l=Z_FLAG|N_FLAG|(R->HL.B.l+I>255? (C_FLAG|H_FLAG):0);
    R->ICount-=16;
  }
  break;

case OUTD:
  --R->BC.B.h;
  I=msx_RdZ80(R->HL.W--);
  msx_OutZ80(R->BC.W,I);
  R->AF.B.l=N_FLAG|(R->BC.B.h? 0:Z_FLAG)|(R->HL.B.l+I>255? (C_FLAG|H_FLAG):0);
  break;

case OTDR:
  --R->BC.B.h;
  I=msx_RdZ80(R->HL.W--);
  msx_OutZ80(R->BC.W,I);
  if(R->BC.B.h)
  {
    R->AF.B.l=N_FLAG|(R->HL.B.l+I>255? (C_FLAG|H_FLAG):0);
    R->ICount-=21;
    R->PC.W-=2;
  }
  else
  {
    R->AF.B.l=Z_FLAG|N_FLAG|(R->HL.B.l+I>255? (C_FLAG|H_FLAG):0);
    R->ICount-=16;
  }
  break;

case LDI:
  msx_WrZ80(R->DE.W++,msx_RdZ80(R->HL.W++));
  --R->BC.W;
  R->AF.B.l=(R->AF.B.l&~(N_FLAG|H_FLAG|P_FLAG))|(R->BC.W? P_FLAG:0);
  break;

case LDIR:
  msx_WrZ80(R->DE.W++,msx_RdZ80(R->HL.W++));
  if(--R->BC.W)
  {
    R->AF.B.l=(R->AF.B.l&~(H_FLAG|P_FLAG))|N_FLAG;
    R->ICount-=21;
    R->PC.W-=2;
  }
  else
  {
    R->AF.B.l&=~(N_FLAG|H_FLAG|P_FLAG);
    R->ICount-=16;
  }
  break;

case LDD:
  msx_WrZ80(R->DE.W--,msx_RdZ80(R->HL.W--));
  --R->BC.W;
  R->AF.B.l=(R->AF.B.l&~(N_FLAG|H_FLAG|P_FLAG))|(R->BC.W? P_FLAG:0);
  break;

case LDDR:
  msx_WrZ80(R->DE.W--,msx_RdZ80(R->HL.W--));
  R->AF.B.l&=~(N_FLAG|H_FLAG|P_FLAG);
  if(--R->BC.W)
  {
    R->AF.B.l=(R->AF.B.l&~(H_FLAG|P_FLAG))|N_FLAG;
    R->ICount-=21;
    R->PC.W-=2;
  }
  else
  {
    R->AF.B.l&=~(N_FLAG|H_FLAG|P_FLAG);
    R->ICount-=16;
  }
  break;

case CPI:
  I=msx_RdZ80(R->HL.W++);
  J.B.l=R->AF.B.h-I;
  --R->BC.W;
  R->AF.B.l =
    N_FLAG|(R->AF.B.l&C_FLAG)|ZSTable[J.B.l]|
    ((R->AF.B.h^I^J.B.l)&H_FLAG)|(R->BC.W? P_FLAG:0);
  break;

case CPIR:
  I=msx_RdZ80(R->HL.W++);
  J.B.l=R->AF.B.h-I;
  if(--R->BC.W&&J.B.l) { R->ICount-=21;R->PC.W-=2; } else R->ICount-=16;
  R->AF.B.l =
    N_FLAG|(R->AF.B.l&C_FLAG)|ZSTable[J.B.l]|
    ((R->AF.B.h^I^J.B.l)&H_FLAG)|(R->BC.W? P_FLAG:0);
  break;  

case CPD:
  I=msx_RdZ80(R->HL.W--);
  J.B.l=R->AF.B.h-I;
  --R->BC.W;
  R->AF.B.l =
    N_FLAG|(R->AF.B.l&C_FLAG)|ZSTable[J.B.l]|
    ((R->AF.B.h^I^J.B.l)&H_FLAG)|(R->BC.W? P_FLAG:0);
  break;

case CPDR:
  I=msx_RdZ80(R->HL.W--);
  J.B.l=R->AF.B.h-I;
  if(--R->BC.W&&J.B.l) { R->ICount-=21;R->PC.W-=2; } else R->ICount-=16;
  R->AF.B.l =
    N_FLAG|(R->AF.B.l&C_FLAG)|ZSTable[J.B.l]|
    ((R->AF.B.h^I^J.B.l)&H_FLAG)|(R->BC.W? P_FLAG:0);
  break;

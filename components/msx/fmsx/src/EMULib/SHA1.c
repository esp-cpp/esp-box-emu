#include "SHA1.h"

#define ROTATE(Data,Shift) \
  ((((Data)<<(Shift))|((Data)>>(32-(Shift))))&0xFFFFFFFF)

static void ProcessSHA1(SHA1 *State)
{
  static const unsigned int K[] = { 0x5A827999,0x6ED9EBA1,0x8F1BBCDC,0xCA62C1D6 };
  unsigned int A,B,C,D,E,T,W[80];
  int J;

  if(State->Ptr<sizeof(State->Buf)) return;

  for(J=0;J<64;J+=4)
    W[J>>2] =
      (((unsigned int)State->Buf[J])<<24)
    | (((unsigned int)State->Buf[J+1])<<16)
    | (((unsigned int)State->Buf[J+2])<<8)
    | State->Buf[J+3];

  for(J=16;J<80;++J)
  {
    T    = W[J-3]^W[J-8]^W[J-14]^W[J-16];
    W[J] = ROTATE(T,1);
  }

  A = State->Msg[0];
  B = State->Msg[1];
  C = State->Msg[2];
  D = State->Msg[3];
  E = State->Msg[4];

  for(J=0;J<20;++J)
  {
    T = (ROTATE(A,5) + ((B&C)|(~B&D)) + E + W[J] + K[0]) & 0xFFFFFFFF;
    E = D;
    D = C;
    C = ROTATE(B,30);
    B = A;
    A = T;
  }

  for(J=20;J<40;++J)
  {
    T = (ROTATE(A,5) + (B^C^D) + E + W[J] + K[1]) & 0xFFFFFFFF;
    E = D;
    D = C;
    C = ROTATE(B,30);
    B = A;
    A = T;
  }

  for(J=40;J<60;++J)
  {
    T = (ROTATE(A,5) + ((B&C)|(B&D)|(C&D)) + E + W[J] + K[2]) & 0xFFFFFFFF;
    E = D;
    D = C;
    C = ROTATE(B,30);
    B = A;
    A = T;
  }

  for(J=60;J<80;++J)
  {
    T = (ROTATE(A,5) + (B^C^D) + E + W[J] + K[3]) & 0xFFFFFFFF;
    E = D;
    D = C;
    C = ROTATE(B,30);
    B = A;
    A = T;
  }


  State->Msg[0] = (State->Msg[0] + A) & 0xFFFFFFFF;
  State->Msg[1] = (State->Msg[1] + B) & 0xFFFFFFFF;
  State->Msg[2] = (State->Msg[2] + C) & 0xFFFFFFFF;
  State->Msg[3] = (State->Msg[3] + D) & 0xFFFFFFFF;
  State->Msg[4] = (State->Msg[4] + E) & 0xFFFFFFFF;

  State->Ptr  = 0;
}

void ResetSHA1(SHA1 *State)
{
  State->LenL  = 0;
  State->LenH  = 0;
  State->Ptr   = 0;
  State->Done  = 0;
  State->Error = 0;

  State->Msg[0] = 0x67452301;
  State->Msg[1] = 0xEFCDAB89;
  State->Msg[2] = 0x98BADCFE;
  State->Msg[3] = 0x10325476;
  State->Msg[4] = 0xC3D2E1F0;
}

int ComputeSHA1(SHA1 *State)
{
  if(State->Error) return(0);
  if(State->Done)  return(1);

  //
  // Padding digest to required number of bits
  //

  // Make sure buffer is not full
  ProcessSHA1(State);

  // Add 0x80
  State->Buf[State->Ptr++] = 0x80;

  // If not enough space for message length...
  if(State->Ptr>sizeof(State->Buf)-8)
  {
    // Pad with zeros
    while(State->Ptr<sizeof(State->Buf))
      State->Buf[State->Ptr++] = 0;
    ProcessSHA1(State);
  }

  // Pad with more zeros, leaving 8 octets at the end
  while(State->Ptr<sizeof(State->Buf)-8)
    State->Buf[State->Ptr++] = 0;

  // Store the message length as the last 8 octets
  State->Buf[State->Ptr++] = (State->LenH>>24) & 0xFF;
  State->Buf[State->Ptr++] = (State->LenH>>16) & 0xFF;
  State->Buf[State->Ptr++] = (State->LenH>>8)  & 0xFF;
  State->Buf[State->Ptr++] = (State->LenH)     & 0xFF;
  State->Buf[State->Ptr++] = (State->LenL>>24) & 0xFF;
  State->Buf[State->Ptr++] = (State->LenL>>16) & 0xFF;
  State->Buf[State->Ptr++] = (State->LenL>>8)  & 0xFF;
  State->Buf[State->Ptr++] = (State->LenL)     & 0xFF;

  ProcessSHA1(State);

  // Done
  State->Done = 1;
  return(1);
}

int InputSHA1(SHA1 *State,const unsigned char *Data,unsigned int Size)
{
  unsigned int J;

  if(State->Done || State->Error)
  {
    State->Error = 1;
    return(0);
  }

  // Make sure buffer is not full
  ProcessSHA1(State);

  // No data, nothing to process
  if(!Size) return(1);

  // Push input data into the buffer
  for(J=0 ; !State->Error && (J<Size) ; ++J)
  {
    State->Buf[State->Ptr++] = Data[J];
    State->LenL = (State->LenL+8) & 0xFFFFFFFF;

    if(!State->LenL)
    {
      State->LenH = (State->LenH+1)&0xFFFFFFFF;
      if(!State->LenH)
      {
        State->Error = 1;
        return(0);
      }
    }

    if(State->Ptr>=sizeof(State->Buf)) ProcessSHA1(State);
  }

  return(1);
}

const char *OutputSHA1(SHA1 *State,char *Output,unsigned int Size)
{
  const char *Hex = "0123456789abcdef";
  unsigned int J;

  if(!State->Done || State->Error || (Size<41)) return(0);

  for(J=0 ; J<40 ; ++J)
    Output[J] = Hex[(State->Msg[J>>3]>>((~J&7)<<2))&15];

  Output[J] = '\0';
  return(Output);
}


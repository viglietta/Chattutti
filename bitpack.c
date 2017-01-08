#include "main.h"

static Uint32 *BitData;
static Uint32 BitSize; // size of BitData in bits
static Uint32 BitsUsed;
static Uint64 BitScratch;
static Uint32 ScratchUsed;

static Uint32 *sBitData;
static Uint32 sBitSize; // size of BitData in bits
static Uint32 sBitsUsed;
static Uint64 sBitScratch;
static Uint32 sScratchUsed;

void ResetBitPacker(void *data,Uint32 words){ // 32-bit words
    BitData=data;
    BitSize=words<<5;
    BitScratch=BitsUsed=ScratchUsed=0;
}

void SaveBitPacker(){
    sBitData=BitData;
    sBitSize=BitSize;
    sBitsUsed=BitsUsed;
    sBitScratch=BitScratch;
    sScratchUsed=ScratchUsed;
}

void LoadBitPacker(){
    BitData=sBitData;
    BitSize=sBitSize;
    BitsUsed=sBitsUsed;
    BitScratch=sBitScratch;
    ScratchUsed=sScratchUsed;
}

Uint32 UsedBitPacker(){
    return(BitsUsed);
}

Uint32 LeftBitPacker(){
    return(BitSize-BitsUsed);
}

void FlushBitPacker(){
    if(ScratchUsed){
        *BitData++=SDL_SwapLE32(BitScratch);
        BitsUsed+=32-ScratchUsed;
        BitScratch=ScratchUsed=0;
    }
}

SDL_bool WriteBits(const Uint32 value,Uint32 bits){ // 0 <= bits <= 32
    if(BitsUsed+bits>BitSize)return(SDL_FALSE);
    BitScratch|=(Uint64)(value&(((Uint64)1<<bits)-1))<<ScratchUsed;
    ScratchUsed+=bits;
    if(ScratchUsed>=32){
        *BitData++=SDL_SwapLE32(BitScratch);
        BitScratch>>=32;
        ScratchUsed-=32;
    }
    BitsUsed+=bits;
    return(SDL_TRUE);
}

SDL_bool WriteBytes(const Uint8 *data,Uint32 bytes){
    Uint32 i;
    if(BitsUsed+(bytes<<3)>BitSize)return(SDL_FALSE); // ignoring alignment bits
    Uint32 r=BitsUsed&7;
    if(r)WriteBits(0,8-r);
    Uint32 h=(4-((BitsUsed&31)>>3))&3;
    if(h>bytes)h=bytes;
    for(i=0;i<h;i++)WriteBits(data[i],8);
    if(h==bytes)return(SDL_TRUE);
    Uint32 n=(bytes-h)>>2;
    if(n){
        memcpy(BitData,data+h,n<<2);
        BitsUsed+=n<<5;
        BitData+=n;
    }
    for(i=h+(n<<2);i<bytes;i++)WriteBits(data[i],8);
    return(SDL_TRUE);
}

SDL_bool ReadBits(Uint32 *value,Uint32 bits){ // 0 <= bits <= 32
    if(BitsUsed+bits>BitSize)return(SDL_FALSE);
    if(ScratchUsed<bits){
        BitScratch|=(Uint64)SDL_SwapLE32(*BitData++)<<ScratchUsed;
        ScratchUsed+=32;
    }
    *value=BitScratch&(((Uint64)1<<bits)-1);
    BitScratch>>=bits;
    ScratchUsed-=bits;
    BitsUsed+=bits;
    return(SDL_TRUE);
}

SDL_bool ReadBytes(Uint8 *data,Uint32 bytes){
    Uint32 i;
    if(BitsUsed+(bytes<<3)>BitSize)return(SDL_FALSE); // ignoring alignment bits
    Uint32 r=BitsUsed&7;
    if(r){
        Uint32 v=0;
        ReadBits(&v,8-r);
        if(v)return(SDL_FALSE);
    }
    Uint32 h=(4-((BitsUsed&31)>>3))&3;
    if(h>bytes)h=bytes;
    for(i=0;i<h;i++){
        Uint32 v=0;
        ReadBits(&v,8);
        if(data)data[i]=v;
    }
    if(h==bytes)return(SDL_TRUE);
    Uint32 n=(bytes-h)>>2;
    if(n){
        if(data)memcpy(data+h,BitData,n<<2);
        BitsUsed+=n<<5;
        BitData+=n;
    }
    for(i=h+(n<<2);i<bytes;i++){
        Uint32 v=0;
        ReadBits(&v,8);
        if(data)data[i]=v;
    }
    return(SDL_TRUE);
}

Uint32 BitsRequired(Uint32 min,Uint32 max){
    return(SDL_MostSignificantBitIndex32(max-min)+1);
}

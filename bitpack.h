void ResetBitPacker(void *data,Uint32 bytes);
void SaveBitPacker();
void LoadBitPacker();
Uint32 UsedBitPacker();
Uint32 LeftBitPacker();
void FlushBitPacker();
SDL_bool WriteBits(const Uint32 value,Uint32 bits);
SDL_bool WriteBytes(const Uint8 *data,Uint32 bytes);
SDL_bool ReadBits(Uint32 *value,Uint32 bits);
SDL_bool ReadBytes(Uint8 *data,Uint32 bytes);

Uint32 BitsRequired(Uint32 min,Uint32 max);

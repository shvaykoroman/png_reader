#ifndef PNG
#define PNG

#pragma pack(push,1)

#define FOURCC(string) (((u32)string[0] << 0) | ((u32)string[1] << 8) | ((u32)string[2] << 16) | ((u32)string[3] << 24))


struct streaming_chunk
{
    u32 size;
    void *memory;
    
    streaming_chunk *next;
};


struct streaming_buffer
{
    u32 size;
    void *memory;
    
    u32 bitBuf;
    u32 bitCount;
    
    bool underflow;
    
    streaming_chunk *last;
    streaming_chunk *first;
};


struct png_header
{
    u8 signature[8];
};

struct png_chunk_header
{
    u32 length;
    union
    {
        u32 u32Type;
        char type[4];
    };
    
};

struct ihdr_chunk
{
    u32 width;
    u32 height;
    u8  bitDepth;
    u8  colorType;
    u8  compressionMethod;
    u8  filterMethod;
    u8  interlaceMethod;
};

struct png_idat_header
{
    u8 zLibMethodFlags;
    u8 additionalFlags;
    
};

struct png_idat_footer
{
    u32 checkValue;
};

struct png_chunk_footer
{
    u32 CRC; 
};

#pragma pack(pop)

struct png_huffman_entry
{
    u16 symbol;
    u16 bitsUsed;
};

struct png_huffman
{
    u32 maxCodeLengthBits;
    u32 entryCount;
    png_huffman_entry *entries;
};


#endif //PNG

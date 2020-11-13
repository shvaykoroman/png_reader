#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <memory.h>
#include <assert.h>
#include <math.h>
#define arrayCount(count) (sizeof(count) / sizeof(count[0]))

typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t  u8;

typedef int8_t  s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef float  f32;
typedef double f64;


#include "png_reader.h"

u8 baseLengthExtraBit[] = 
{
    0, 0, 0, 0, 0, 0, 0, 0, //257 - 264
    1, 1, 1, 1, //265 - 268
    2, 2, 2, 2, //269 - 273 
    3, 3, 3, 3, //274 - 276
    4, 4, 4, 4, //278 - 280
    5, 5, 5, 5, //281 - 284
    0           //285
};

u32 baseLengths[] = 
{
    3, 4, 5, 6, 7, 8, 9, 10, //257 - 264
    11, 13, 15, 17,          //265 - 268
    19, 23, 27, 31,          //269 - 273 
    35, 43, 51, 59,          //274 - 276
    67, 83, 99, 115,         //278 - 280
    131, 163, 195, 227,      //281 - 284
    258                      //285
};

u32 distanceCodes[] = 
{
    /*0*/ 1, 2, 3, 4,    //0-3
    /*1*/ 5, 7,          //4-5
    /*2*/ 9, 13,         //6-7
    /*3*/ 17, 25,        //8-9
    /*4*/ 33, 49,        //10-11
    /*5*/ 65, 97,        //12-13
    /*6*/ 129, 193,      //14-15
    /*7*/ 257, 385,      //16-17
    /*8*/ 513, 769,      //18-19
    /*9*/ 1025, 1537,    //20-21
    /*10*/ 2049, 3073,   //22-23
    /*11*/ 4097, 6145,   //24-25
    /*12*/ 8193, 12289,  //26-27
    /*13*/ 16385, 24577,  //28-29
    0   , 0      //30-31, error
};

u32 distExtraBits[] = 
{
    /*0*/ 0, 0, 0, 0, //0-3
    /*1*/ 1, 1,       //4-5
    /*2*/ 2, 2,       //6-7
    /*3*/ 3, 3,       //8-9
    /*4*/ 4, 4,       //10-11
    /*5*/ 5, 5,       //12-13
    /*6*/ 6, 6,       //14-15
    /*7*/ 7, 7,       //16-17
    /*8*/ 8, 8,       //18-19
    /*9*/ 9, 9,       //20-21
    /*10*/ 10, 10,    //22-23
    /*11*/ 11, 11,    //24-25
    /*12*/ 12, 12,    //26-27
    /*13*/ 13, 13,     //28-29
    0 , 0      //30-31 error
};

streaming_chunk *
allocateChunk()
{
    streaming_chunk *result = (streaming_chunk *)malloc(sizeof(streaming_chunk));
    return result;
}

streaming_buffer
readEntireFile(char *filename)
{
    streaming_buffer result = {};
    
    FILE *file = fopen(filename, "rb");
    
    if(file)
    {
        fseek(file,0,SEEK_END);
        result.size = ftell(file);
        fseek(file,0,SEEK_SET);
        
        result.memory = malloc(result.size);
        fread(result.memory, result.size, 1,file);
        fclose(file);
    }
    else
    {
        fprintf(stdout, "cannot open file : %s\n",filename);
    }
    
    return result;
}

#define consume(file, type) (type*)consumeSize(file,sizeof(type))

void *
consumeSize(streaming_buffer *file,u32 size)
{
    void *result = 0;
    if((file->size == 0) && file->first)
    {
        streaming_chunk *This = file->first;
        file->size = This->size;
        file->memory = This->memory;
        file->first = This->next;
    }
    if(file->size >= 0)
    {
        result = file->memory;
        file->memory = (u8*)file->memory + size;
        file->size -= size;
    }
    else
    {
        file->size = 0;
        fprintf(stderr, "underflow\n");
        file->underflow = true;
    }
    
    return result;
}

void
changeEndianness(u32 *v)
{
    
    
#if 0
    
    s8 b0 = ((v >> 0)  & 0xFF);
    s8 b1 = ((v >> 8)  & 0xFF);
    s8 b2 = ((v >> 16) & 0xFF);
    s8 b3 = ((v >> 24) & 0xFF);
    
    v = (
         (b0 << 24) |
         (b1 << 16) |
         (b2 << 8) |
         (b3 << 0));
#else
    *v = (
          (*v << 24) |
          ((*v & 0xFF00) << 8 ) |
          ((*v >> 8) & 0xFF00) |
          (*v >> 24));
#endif
    
}

void *
allocatePixels(u32 width,u32 height, u32 BPP, u32 additionalData)
{
    void *result = malloc(width * height * BPP + additionalData * height);
    return result;
}


u32
peekBites(streaming_buffer *buf, u32 bitCount)
{
    u32 result = 0;
    
    while((buf->bitCount < bitCount) && (!buf->underflow))
    {
        u32 byte = *consume(buf, u8);
        buf->bitBuf |= (byte << buf->bitCount);
        buf->bitCount += 8;
    }
    
    if(buf->bitCount >= bitCount)
    {
        result = buf->bitBuf & ((1 << bitCount) - 1);
    }
    else
    {
        buf->underflow = true;
    }
    return result;
}

void
discardBits(streaming_buffer *buf, u32 count)
{
    
    buf->bitCount -= count;
    buf->bitBuf >>= count;
    
}

u32
consumeBits(streaming_buffer *buf, u32 bitCount)
{
    u32 result = peekBites(buf, bitCount);
    discardBits(buf,bitCount);
    
    return result;
}


void
flushBytes(streaming_buffer *buf)
{
    u32 bitsCount = buf->bitCount % 8;
    consumeBits(buf,bitsCount);
}

png_huffman
allocateHuffman(u32 maxCodeLengthBits)
{
    
    assert(maxCodeLengthBits <= 16);
    
    png_huffman result = {};
    result.maxCodeLengthBits = maxCodeLengthBits;
    result.entryCount = 1 << maxCodeLengthBits;
    result.entries = (png_huffman_entry*)malloc(sizeof(png_huffman_entry) * result.entryCount);
    return result;
}

void
buildHuffman(u32 symbolCount,u32 *symbolCodeLength,png_huffman *result,u32 symbolAddend = 0)
{
    u32 codeLengthCount[16] = {};
    
    // NOTE(shvayko): count number times each bit length came up
    for(u32 symbolIndex = 0; symbolIndex < symbolCount;symbolIndex++)
    {
        u32 count = symbolCodeLength[symbolIndex];
        assert(count <= arrayCount(codeLengthCount));
        ++codeLengthCount[count];
    }
    
    // NOTE(shvayko):generating codes for each bit lengths(smallest code for each bit length) 
    u32 nextUnusedCode[16];
    nextUnusedCode[0] = 0;
    // NOTE(shvayko): we don't count the 0
    codeLengthCount[0] = 0;
    for(u32 bitIndex = 1; bitIndex < arrayCount(nextUnusedCode); bitIndex++)
    {
        nextUnusedCode[bitIndex] = ((nextUnusedCode[bitIndex - 1] + (codeLengthCount[bitIndex - 1]) << 1));
    }
    
    
    // NOTE(shvayko): assign numerical values to all codes
    for(u32 symbolIndex = 0; symbolIndex < symbolCount;symbolIndex++)
    {
        u32 codeLengthBits = symbolCodeLength[symbolIndex];
        if(codeLengthBits)
        {
            assert(codeLengthBits < arrayCount(nextUnusedCode));
            
            u32 code = nextUnusedCode[codeLengthBits]++;
            u32 arbitaryBits =  result->maxCodeLengthBits - codeLengthBits;
            u32 entryCount = (1 << arbitaryBits);
            
            
            for(u32 entryIndex = 0; entryIndex < entryCount;entryIndex++)
            {
                u32 baseIndex = (code << arbitaryBits) | entryIndex;
                u32 index = 0;
                
                // NOTE(shvayko):reverting bits
#if 1
                for(u32 bitIndex = 0; bitIndex <= (result->maxCodeLengthBits / 2);bitIndex++)
                {
                    u32 inv = result->maxCodeLengthBits - (bitIndex + 1);
                    index |= ((baseIndex >> bitIndex) & 0x1) << inv;
                    index |= ((baseIndex >> inv) & 0x1) << bitIndex;
                }
#else
                index = baseIndex;
#endif
                
                png_huffman_entry *entry = result->entries + index;
                
                u32 symbolVal = (symbolIndex + symbolAddend);
                entry->bitsUsed = (u16)codeLengthBits;
                entry->symbol = (u16)symbolVal;
                
                assert(entry->bitsUsed == codeLengthBits);
                assert(entry->symbol == symbolVal);
            }
        }
    }
}

u32
decodeHuffman(png_huffman *huffman,streaming_buffer *buf)
{
    
    u32 entryIndex = peekBites(buf,huffman->maxCodeLengthBits);
    assert(entryIndex < huffman->entryCount);
    
    png_huffman_entry entry = huffman->entries[entryIndex];
    
    u32 result = entry.symbol;
    discardBits(buf,entry.bitsUsed);
    assert(entry.bitsUsed);
    return result;
}

/*
void
filterType0(u32 width, u8 *result, u8 *src)
{
    for(u32 x = 0; x < width * 4; x++)
    {
        *result++ = *src++;
    }
}

void
filterType1(u32 width, u8 *result, u8 *src)
{
    
    for(u32 x = 0; x < width * 4; x++)
    {
        u8 *a = src - 1;
        *result++ = *src++ + *a;
    }
}

void
filterType2(u32 width, u8 *result, u8 *src, u8 *prevScanline, u32 stride)
{
    for(u32 x = 0; x < width * 4; x++)
    {
        u8 *b = src - stride;
        *result++ = *src++ + *b; 
    }
}
*/

u8
filterType1(u8 *x, u8 *a,u32 channel)
{
    u8 result = 0;
    
    result = (u8)x[channel] + (u8)a[channel];
    
    return result = 0;
}

u8
filterType2(u8 *x, u8 *b,u32 channel)
{
    u8 result = 0;
    
    result = (u8)x[channel] + (u8)b[channel];
    
    return result = 0;
}

u8 
filterType3(u8 *x, u8 *a, u8 *b, u32 channel)
{
    u8 result = 0;
    
    result = (u8)x[channel] + (u8)(((u32)a[channel] + (u32)b[channel])  / 2);
    
    return result;
}

u8 
filterType4(u8 *x,u8 *aIn, u8 *bIn,u8 *cIn,u32 channel)
{
    u8 result = 0;
    
    s8 a = aIn[channel];
    s8 b = bIn[channel];
    s8 c = cIn[channel];
    
    s32 p = a + b - c;
    
    s32 pa = abs(p - a);
    s32 pb = abs(p - b);
    s32 pc = abs(p - c);
    
    u8 pr = 0;
    if((pa <= pb) && (pa <= pc))
    {
        pr = a;
    }
    else if(pb <= pc)
    {
        pr = b;
    }
    else
    {
        pr = c;
    }
    
    result = c;
    
    return result;
}

void
parsingPNG(streaming_buffer *file)
{
    streaming_buffer *at = file;
    streaming_buffer compData = {};
    u8 *decompressedPixels = 0;
    u8 *imagePixels = 0;
    u8 *imagePixelsEnd = 0;
    bool supported = true;
    u32 width = 0;
    u32 height = 0;
    
    png_header *fileHeader = consume(file, png_header);
    if(fileHeader)
    {
        while(at->size > 0)
        {
            png_chunk_header *chunkHeader = consume(file,png_chunk_header);
            if(chunkHeader)
            {
                changeEndianness(&chunkHeader->length);
                void *chunkData = consumeSize(file, chunkHeader->length);
                
                if(chunkData)
                {
                    
                    png_chunk_footer *chunkFooter = consume(file, png_chunk_footer);
                    changeEndianness(&chunkFooter->CRC);
                    if(FOURCC("IHDR") == chunkHeader->u32Type)
                    {
                        fprintf(stderr, "type: IHDR\n");
                        ihdr_chunk *ihdr = (ihdr_chunk*)chunkData;
                        
                        changeEndianness(&ihdr->width);
                        changeEndianness(&ihdr->height);
                        
                        fprintf(stdout, "  width: %d\n", ihdr->width);
                        fprintf(stdout, "  height: %d\n", ihdr->height);
                        fprintf(stdout, "  bitDepth: %d\n", ihdr->bitDepth);
                        fprintf(stdout, "  colorType: %d\n", ihdr->colorType);
                        fprintf(stdout, "  compressionMethod: %d\n", ihdr->compressionMethod);
                        fprintf(stdout, "  filterMethod: %d\n", ihdr->filterMethod);
                        fprintf(stdout, "  interlaceMethod: %d\n", ihdr->interlaceMethod);
                        
                        width = ihdr->width;
                        height = ihdr->height;
                        
                        if(ihdr->colorType != 6)
                        {
                            fprintf(stdout, "The color type of %u is not supported. Supported only 6(RGBA)\n",ihdr->colorType);
                            assert(ihdr->colorType == 6);
                        }
                        
                        if(ihdr->compressionMethod != 0)
                        {
                            fprintf(stderr,"Only 0 compression method is defined by International standart\n");
                            supported = false;
                        }
                        decompressedPixels = (u8*)allocatePixels(width,height,4, 1);
                        imagePixels = (u8*)allocatePixels(width,ihdr->height,4, 0);
                        
                        imagePixelsEnd = imagePixels + (width * height * 4);
                    }
                    else if(FOURCC("IDAT") == chunkHeader->u32Type)
                    {
                        
                        fprintf(stdout,"IDAT(%u)\n",chunkHeader->length);
                        
                        
                        streaming_chunk *chunk = allocateChunk();
                        chunk->size = chunkHeader->length;
                        chunk->memory = chunkData;
                        chunk->next = 0;
                        
                        if(!compData.first)
                        {
                            compData.first = chunk;
                        }
                        else
                        {
                            compData.last->next = chunk;
                        }
                        
                        //compData.last = ((compData.last ? compData.last->next : compData.first) = chunk);
                        compData.last = chunk;
                    }
                }
            }
        }
        
        png_idat_header *idatHeader = consume(&compData,png_idat_header);
        
        u8 CM = idatHeader->zLibMethodFlags & 0xF;
        u8 CINFO = idatHeader->zLibMethodFlags >> 4;
        u8 FCHECK = idatHeader->additionalFlags & 0x1F;
        u8 FDICT = (idatHeader->additionalFlags >> 5) & 0x1;
        u8 FLEVEL = (idatHeader->additionalFlags >> 6);
        
        fprintf(stdout, "  CM: %d\n", CM);
        fprintf(stdout, "  CINFO: %d\n", CINFO);
        fprintf(stdout, "  FCHECK: %d\n", FCHECK);
        fprintf(stdout, "  FDICT: %d\n", FDICT);
        fprintf(stdout, "  FLEVEL: %d\n", FLEVEL);
        
        if(CM != 8)
        {
            fprintf(stderr,"CM is not an eight(8). Others compression methods is not supported\n");
        }
        
        if((idatHeader->zLibMethodFlags*256+idatHeader->additionalFlags) % 31 != 0)
        {
            fprintf(stderr,"bad zlib header\n");
        }
        
        u32 BFINAL = 0;
        u8 *dest = decompressedPixels;
        while(BFINAL == 0)
        {
            BFINAL = consumeBits(&compData,1);
            u32 BTYPE  = consumeBits(&compData,2);
            
            if(BTYPE == 0)
            {
                flushBytes(&compData);
                u16 LEN = *consume(&compData, u16);
                u16 NLEN = *consume(&compData,u16);
                if((u16)LEN != (u16)~NLEN)
                {
                    fprintf(stderr,"LEN/NLEN mismatch");
                }
                
                u8 *out = (u8*)consumeSize(&compData,LEN);
                while(LEN--)
                {
                    *dest++ = *out++;
                }
                
            }
            else if(BTYPE == 3)
            {
                fprintf(stdout,"BTYPE %d is not allowed", BTYPE);
            }
            else
            {
                png_huffman litLenHuffman = allocateHuffman(15);
                png_huffman distHuffman = allocateHuffman(15);
                if(BTYPE == 2) // NOTE(shvayko): dynamic huffman codes
                {
                    // NOTE(shvayko):num of lit codes
                    u32 HLIT   = consumeBits(&compData,5);
                    // NOTE(shvayko):num of distances code
                    u32 HDIST  = consumeBits(&compData,5);
                    // NOTE(shvayko):num of length codes
                    u32 HCLEN  = consumeBits(&compData,4);
                    
                    HLIT += 257;
                    HDIST += 1;
                    HCLEN += 4;
                    
                    
#if 0
                    // NOTE(shvayko): testing the buildHuffman from the spec
                    u32 codeLength[8] = {3,3,3,3,3,2,4,4};
                    png_huffman testHuf = allocateHuffman(8);
                    buildHuffman(arrayCount(codeLength), codeLength, &testHuf);
#endif
                    u32 HCLENCodeLengths[] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
                    
                    u32 HCLENtable[arrayCount(HCLENCodeLengths)] = {};
                    
                    for(u32 index = 0; index < HCLEN;index++)
                    {
                        HCLENtable[HCLENCodeLengths[index]] = consumeBits(&compData,3);
                    }
                    
                    png_huffman dictHuffman = allocateHuffman(7);
                    buildHuffman(arrayCount(HCLENCodeLengths),HCLENtable,&dictHuffman);
                    
                    u32 litLenDistTable[512];
                    u32 litLenCount = 0;
                    u32 lenCount = HLIT + HDIST;
                    while(litLenCount < lenCount)
                    {
                        u32 encodedLen = decodeHuffman(&dictHuffman,&compData);
                        if(encodedLen <= 15)
                        {
                            litLenDistTable[litLenCount++] = encodedLen;
                        }
                        else if(encodedLen == 16)
                        {
                            u32 repCount = 3 + consumeBits(&compData,2);
                            assert(lenCount > 0);
                            
                            assert(litLenCount > 0);
                            u32 repVal = litLenDistTable[litLenCount - 1];
                            while(repCount--)
                            {
                                litLenDistTable[litLenCount++] = repVal;
                            }
                        }
                        else if(encodedLen == 17)
                        {
                            u32 repCount = 3 + consumeBits(&compData,3);
                            while(repCount--)
                            {
                                litLenDistTable[litLenCount++] = 0;
                            }
                        }
                        else if(encodedLen == 18)
                        {
                            u32 repCount = 11 + consumeBits(&compData,7);
                            while(repCount--)
                            {
                                litLenDistTable[litLenCount++] = 0;
                            }
                        }
                        else
                        {
                            fprintf(stdout,"The encodedLen %u is not allowed", encodedLen);
                        }
                    }
                    assert(litLenCount == lenCount);
                    
                    buildHuffman(HLIT,litLenDistTable,&litLenHuffman);
                    buildHuffman(HDIST,litLenDistTable + HLIT,&distHuffman);
                }
                else // NOTE(shvayko): fixed huffman codes
                {
                    
                }
                // NOTE(shvayko): code for all cases no matter is fixed or dynamic 
                // huffman codes
#if 1
                for(;;)
                {
                    u32 litLen = decodeHuffman(&litLenHuffman,&compData);
                    if(litLen < 256)
                    {
                        u32 out = litLen & 0xFF;
                        *dest++ = out;
                    }
                    else if(litLen > 256)
                    {
                        u32 length = litLen - 257;
                        
                        u32 dupLen = baseLengths[length] + consumeBits(&compData,baseLengthExtraBit[length]);
                        
                        u32 distance = decodeHuffman(&distHuffman,&compData);
                        
                        u32 distanceLength = distanceCodes[distance] + consumeBits(&compData, distExtraBits[distance]);
                        
                        
                        u8 *src = (u8*)dest - distanceLength;
                        assert(src >= decompressedPixels);
                        while(dupLen--)
                        {
                            *dest++ = *src++;
                        }
                    }
                    else // NOTE(shvayko): 256 it is escape value
                    {
                        break;
                    }
                }
#endif
                BFINAL = 1;
                break;
            }
            // NOTE(shvayko):FITLERING
            u32 zero = 0;
            u8 *prevScanline = (u8*)&zero;
            u32 prevRowAdvance = 0;
            
            u8 *src = decompressedPixels;
            u8 *dest = imagePixels;
            
            
            
            for(u32 row = 0; row < height; row++)
            {
                u8 filterType = *src++;
                u8 *currentScanline = dest;
                switch(filterType)
                {
                    case 0: //NOTE(shvayko):None
                    {
                        for(u32 x = 0; x < width; x++)
                        {
                            *(u32*)dest = *(u32*)src;
                            
                            src += 4;
                            dest += 4;
                        }
                    }break;
                    case 1: //NOTE(shvayko):Sub
                    {
                        u32 a = 0;
                        for(u32 x = 0; x < width; x++)
                        {
                            dest[0] = filterType1(dest,(u8*)&a,0);
                            dest[1] = filterType1(dest,(u8*)&a,1);
                            dest[2] = filterType1(dest,(u8*)&a,2);
                            dest[3] = filterType1(dest,(u8*)&a,3);
                            
                            a = *(u32*)dest;
                            
                            src += 4;
                            dest += 4;
                        }
                    }break;
                    case 2: //NOTE(shvayko):Up
                    {
                        u8 *b = prevScanline;
                        
                        for(u32 x = 0; x < width;x++)
                        {
                            dest[0] = filterType2(dest,b,0);
                            dest[1] = filterType2(dest,b,1);
                            dest[2] = filterType2(dest,b,2);
                            dest[3] = filterType2(dest,b,3);
                            
                            b += prevRowAdvance;
                            src += 4;
                            dest += 4;
                        }
                    }break;
                    case 3:
                    {
                        u32 a = 0;
                        u8 *b = prevScanline;
                        for(u32 x = 0; x < width;x++)
                        {
                            dest[0] = filterType3(dest,(u8*)&a,b,0);
                            dest[1] = filterType3(dest,(u8*)&a,b,1);
                            dest[2] = filterType3(dest,(u8*)&a,b,2);
                            dest[3] = filterType3(dest,(u8*)&a,b,3);
                            
                            a = *(u32*)dest;
                            
                            b += prevRowAdvance;
                            src += 4;
                            dest += 4;
                        }
                    }break;
                    case 4:
                    {
                        u32 a = 0;
                        u32 c = 0;
                        u8 *b = prevScanline;
                        
                        for(u32 x = 0; x < width;x++)
                        {
                            dest[0] = filterType4(dest,(u8*)&a,b,(u8*)&c,0);
                            dest[1] = filterType4(dest,(u8*)&a,b,(u8*)&c,1);
                            dest[2] = filterType4(dest,(u8*)&a,b,(u8*)&c,2);
                            dest[3] = filterType4(dest,(u8*)&a,b,(u8*)&c,3);
                            
                            c = *(u32*)b;
                            a = *(u32*)dest;
                            
                            b += prevRowAdvance;
                            src += 4;
                            dest += 4;
                        }
                    }break;
                    default:
                    {
                        fprintf(stdout,"The filter type of %u is not supported", filterType);
                    }break;
                }
                prevScanline = currentScanline;
                prevRowAdvance += 4;
            }
            
            assert(dest == imagePixelsEnd);
            
        }
    }
}

int main(int argCount, char *argv[])
{
    if(argCount == 2) 
    {
        char *filename =  (char*)argv[1];
        fprintf(stdout, "file %s is loading...:\n",filename);
        
        streaming_buffer fileContent = readEntireFile(filename);
        parsingPNG(&fileContent);
    }
    else
    {
        fprintf(stderr, "require file\n");
    }
    
    return 0;
}
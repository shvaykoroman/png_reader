#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <memory.h>
#include <assert.h>

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

u32
swapAB(u32 c)
{
    u32 result = ((c & 0xFF00FF00) |
                  ((c >> 16)&0xFF) | 
                  ((c & 0xFF) << 16));
    return result;
}

void
writeImage(u8 *pixels,u32 width, u32 height,char *outputFileName)
{
    
    u32 outputPixelSize = 4*width*height;
    
    bmp_header header = {};
    
    header.fileType = 0x4D42;
    header.fileSize = sizeof(header) + outputPixelSize;
    header.bitmapOffset = sizeof(header);
    header.size = sizeof(header) - 14;
    header.width = width;
    header.height = height;
    header.planes = 1;
    header.bitsPerPixel = 32;
    header.compression = 0;
    header.sizeOfBitmap = outputPixelSize;
    header.horzResolution = 0;
    header.vertResolution = 0;
    header.colorsUsed = 0;
    header.colorsImportant = 0;
    
    
    u32 centerPoint = (header.height + 1) / 2;
    u32 *row0 = (u32*)pixels;
    u32 *row1 = row0 + ((height - 1) * width);
    
    // RGBA -> -> ->
    // BGRA
    for(u32 y = 0; y < centerPoint; y++)
    {
        u32 *pix0 = row0;
        u32 *pix1 = row1;
        for(u32 x = 0; x < width; x++)
        {
            u32 c0 = swapAB(*pix0);
            u32 c1 = swapAB(*pix1);
            
            *pix0++ = c1;
            *pix1++ = c0;
        }
        row0 += width;
        row1 -= width;
    }
    
    FILE *file = fopen(outputFileName, "wb");
    if(file)
    {
        fwrite(&header, sizeof(header),1,file);
        fwrite(pixels, outputPixelSize,1,file);
        fclose(file);
    }
    else
    {
        fprintf(stderr, "[ERROR] Unable to write output file %s.\n", outputFileName);
    }
    
}

png_huffman_entry pngLengthExtra[] =
{
    {3, 0}, // 257
    {4, 0}, // 258
    {5, 0}, // 259
    {6, 0}, // 260
    {7, 0}, // 261
    {8, 0}, // 262
    {9, 0}, // 263
    {10, 0}, // 264
    {11, 1}, // 265
    {13, 1}, // 266
    {15, 1}, // 267
    {17, 1}, // 268
    {19, 2}, // 269
    {23, 2}, // 270
    {27, 2}, // 271
    {31, 2}, // 272
    {35, 3}, // 273
    {43, 3}, // 274
    {51, 3}, // 275
    {59, 3}, // 276
    {67, 4}, // 277
    {83, 4}, // 278
    {99, 4}, // 279
    {115, 4}, // 280
    {131, 5}, // 281
    {163, 5}, // 282
    {195, 5}, // 283
    {227, 5}, // 284
    {258, 0}, // 285
};

png_huffman_entry pngDistExtra[] =
{
    {1, 0},  //0
    {2, 0}, // 1
    {3, 0}, // 2
    {4, 0}, // 3
    {5, 1}, // 4
    {7, 1}, // 5
    {9, 2}, // 6
    {13, 2}, // 7
    {17, 3}, // 8
    {25, 3}, // 9
    {33, 4}, // 10
    {49, 4}, // 11
    {65, 5}, // 12
    {97, 5}, // 13
    {129, 6}, //14 
    {193, 6}, //  15
    {257, 7}, //  16
    {385, 7}, //  17
    {513, 8}, //  18
    {769, 8}, //  19
    {1025, 9}, //  20
    {1537, 9}, //  21
    {2049, 10}, //  22
    {3073, 10}, //  23
    {4097, 11}, //  24
    {6145, 11}, //  25
    {8193, 12}, //  26
    {12289, 12}, // 27
    {16385, 13}, // 28
    {24577, 13}, 
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

void
refillBuffer(streaming_buffer *file)
{
    if((file->size == 0) && file->first)
    {
        streaming_chunk *This = file->first;
        file->size = This->size;
        file->memory = This->memory;
        file->first = This->next;
    }
}

#define consume(file, type) (type*)consumeSize(file,sizeof(type))
void *
consumeSize(streaming_buffer *file,u32 size)
{
    void *result = 0;
    refillBuffer(file);
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
        fprintf(stderr,"uunderflow!!!!!!\n");
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

u8
filterType1(u8 *x, u8 *a,u32 channel)
{
    u8 result = 0;
    
    result = (u8)x[channel] + (u8)a[channel];
    
    return result;
}

u8
filterType2(u8 *x, u8 *b,u32 channel)
{
    u8 result = 0;
    
    result = (u8)x[channel] + (u8)b[channel];
    
    return result;
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
    
    s32 a = (s32)aIn[channel];
    s32 b = (s32)bIn[channel];
    s32 c = (s32)cIn[channel];
    
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
    
    result = (u8)x[channel] + (u8)pr;
    
    return result;
}

u8 *
parsingPNG(Memory_arena *arena, streaming_buffer *file, u32 *widthOut, u32 *heightOut)
{
    void *result = 0;
    streaming_buffer *at = file;
    streaming_buffer compData = {};
    u8 *decompressedPixels = 0;
    u8 *imagePixels = 0;
    u8 *imagePixelsEnd = 0;
    bool supported = true;
    u32 width = 0;
    u32 height = 0;
    
    image_out_info imageInfo = {};
    
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
                        
                        imageInfo.width = width;
                        imageInfo.height = height;
                        
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
                        imagePixelsEnd = imagePixels + ((width * height * 4));
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
            supported = false;
        }
        
        if((idatHeader->zLibMethodFlags*256+idatHeader->additionalFlags) % 31 != 0)
        {
            fprintf(stderr,"bad zlib header\n");
            supported = false;
        }
        u8 *dest = decompressedPixels;
        
        u32 BFINAL = 0;
        
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
                
                while(LEN)
                {
                    refillBuffer(&compData);
                    u16 currentLen = LEN;
                    if(currentLen > compData.size)
                    {
                        currentLen = (u16)compData.size;
                    }
                    u8 *out = (u8*)consumeSize(&compData,currentLen);
                    while(LEN--)
                    {
                        *dest++ = *out++;
                    }
                    
                    LEN -= currentLen;
                    
                }
                
            }
            else if(BTYPE == 3)
            {
                fprintf(stdout,"BTYPE %d is not allowed", BTYPE);
            }
            else
            {
                u32 litLenDistTable[512];
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
                    u32 HLIT = 288;
                    u32 HDIST = 32;
                    
                    u32 fixedHuffmanTable[][2] = 
                    {
                        {143,8},
                        {255,9},
                        {279,9},
                        {287,8},
                        {319,5},
                    };
                    
                    u32 tableIndex = 0;
                    for(s32 numbersRange = 0; numbersRange < arrayCount(fixedHuffmanTable);
                        numbersRange++)
                    {
                        u32 bitCount = fixedHuffmanTable[numbersRange][1];
                        while(tableIndex < fixedHuffmanTable[numbersRange][0])
                        {
                            litLenDistTable[tableIndex++] = bitCount;
                        }
                    }
                    
                    buildHuffman(HLIT,litLenDistTable,&litLenHuffman);
                    buildHuffman(HDIST,litLenDistTable + HLIT,&distHuffman);
                    fprintf(stderr,"fixed huffman code\n");
                }
                // NOTE(shvayko): code for all cases no matter is fixed or dynamic 
                // huffman codes
                
                for(;;)
                {
                    u32 litLen = decodeHuffman(&litLenHuffman,&compData);
                    if(litLen < 256)
                    {
                        u32 out = litLen & 0xFF;
                        *dest++ = (u8)out;
                    }
                    else if(litLen > 256)
                    {
                        u32 lenTableIndex = litLen - 257;
                        png_huffman_entry lenTable = pngLengthExtra[lenTableIndex];
                        u32 length = lenTable.symbol;
                        
                        if(lenTable.bitsUsed)
                        {
                            u32 extraBits = consumeBits(&compData,lenTable.bitsUsed);
                            length += extraBits;
                        }
                        
                        u32 distTableIndex = decodeHuffman(&distHuffman,&compData);
                        png_huffman_entry distTable = pngDistExtra[distTableIndex];
                        
                        u32 dist = distTable.symbol;
                        if(distTable.bitsUsed)
                        {
                            u32 extraBits = consumeBits(&compData,distTable.bitsUsed);
                            dist += extraBits; 
                        }
                        
                        u8 *src = dest - dist;
                        
                        //assert((src + length) <= imagePixelsEnd);
                        //assert((dest + length) <= imagePixelsEnd);
                        //assert(src >= decompressedPixels);
                        
                        while(length--)
                        {
                            *dest++ = *src++;
                        }
                    }
                    else // NOTE(shvayko): 256 it is escape value
                    {
                        break;
                    }
                }
            }
        }
        // NOTE(shvayko):FITLERING
        u32 zero = 0;
        u8 *prevScanline = (u8*)&zero;
        u32 prevRowAdvance = 0;
        
        u8 *src = decompressedPixels;
        u8 *dst = imagePixels;
        
        for(u32 row = 0; row < height; row++)
        {
            u8 filterType = *src++;
            u8 *currentScanline = dst;
            switch(filterType)
            {
                case 0: //NOTE(shvayko):None
                {
                    for(u32 x = 0; x < width; x++)
                    {
                        *(u32*)dst = *(u32*)src;
                        
                        src += 4;
                        dst += 4;
                    }
                }break;
                case 1: //NOTE(shvayko):Sub
                {
                    u32 a = 0;
                    for(u32 x = 0; x < width; x++)
                    {
                        dst[0] = filterType1(src,(u8*)&a,0);
                        dst[1] = filterType1(src,(u8*)&a,1);
                        dst[2] = filterType1(src,(u8*)&a,2);
                        dst[3] = filterType1(src,(u8*)&a,3);
                        
                        a = *(u32*)dst;
                        
                        src += 4;
                        dst += 4;
                    }
                }break;
                case 2: //NOTE(shvayko):Up
                {
                    u8 *b = prevScanline;
                    
                    for(u32 x = 0; x < width;x++)
                    {
                        dst[0] = filterType2(src,b,0);
                        dst[1] = filterType2(src,b,1);
                        dst[2] = filterType2(src,b,2);
                        dst[3] = filterType2(src,b,3);
                        
                        b += prevRowAdvance;
                        src += 4;
                        dst += 4;
                    }
                }break;
                case 3:
                {
                    u32 a = 0;
                    u8 *b = prevScanline;
                    for(u32 x = 0; x < width;x++)
                    {
                        dst[0] = filterType3(src,(u8*)&a,b,0);
                        dst[1] = filterType3(src,(u8*)&a,b,1);
                        dst[2] = filterType3(src,(u8*)&a,b,2);
                        dst[3] = filterType3(src,(u8*)&a,b,3);
                        
                        a = *(u32*)dst;
                        
                        b += prevRowAdvance;
                        src += 4;
                        dst += 4;
                    }
                }break;
                case 4:
                {
                    u32 a = 0;
                    u32 c = 0;
                    u8 *b = prevScanline;
                    
                    for(u32 x = 0; x < width;x++)
                    {
                        dst[0] = filterType4(src,(u8*)&a,b,(u8*)&c,0);
                        dst[1] = filterType4(src,(u8*)&a,b,(u8*)&c,1);
                        dst[2] = filterType4(src,(u8*)&a,b,(u8*)&c,2);
                        dst[3] = filterType4(src,(u8*)&a,b,(u8*)&c,3);
                        
                        c = *(u32*)b;
                        a = *(u32*)dst;
                        
                        b += prevRowAdvance;
                        src += 4;
                        dst += 4;
                    }
                }break;
                default:
                {
                    fprintf(stdout,"The filter type of %u is not supported", filterType);
                }break;
            }
            prevScanline = currentScanline;
            prevRowAdvance = 4;
        }
        
        assert(dst == imagePixelsEnd);
        
    }
    
    *widthOut  = width;
    *heightOut = height;
    
    return imagePixels;
}


int main(int argCount, char *argv[])
{
    if(argCount == 3) 
    {
        Memory_arena arena;
        char *inFileName =  (char*)argv[1];
        char *outFileName =  (char*)argv[2];
        fprintf(stdout, "file %s is loading...:\n",inFileName);
        
        streaming_buffer fileContent = readEntireFile(inFileName);
        u32 width = 0;
        u32 height = 0;
        u8 *data = parsingPNG(&arena,&fileContent, &width, &height);
        writeImage(data,width,height,outFileName);
    }
    else
    {
        fprintf(stderr, "require file\n");
    }
    
    return 0;
}
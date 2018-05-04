/*

ABOUT:

    A C / C++ QRCode encoder

HISTORY:

    0.1     2017-04-15  - Initial version

LICENSE:

    The MIT License (MIT)

    Copyright (c) 2015 Mathias Westerdahl

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.


DISCLAIMER:

    This software is supplied "AS IS" without any warranties and support

USAGE:

    A simple example usage:

    #define JC_QRENCODE_IMPLEMENTATION
    #include "jc_qrencode.h"

    const char* text = "HELLO WORLD";
    JCQRCode* qr = jc_qrencode((const uint8_t*)text, (uint32_t)strlen(text));
    if( !qr )
    {
        fprintf(stderr, "Failed to encode text\n");
        return 1;
    }

    // Save a png file
    stbi_write_png("out.png", qr->size, qr->size, 1, qr->data, 256); // Stride is currently 256 bytes

    free(qr); // free the qr code

*/


#ifndef JC_QRENCODE_H
#define JC_QRENCODE_H

#include <stdint.h>

static const uint32_t JC_QRE_MIN_VERSION = 1;
static const uint32_t JC_QRE_MAX_VERSION = 40;

const static uint32_t JC_QRE_ERROR_CORRECTION_LEVEL_LOW        = 0;
const static uint32_t JC_QRE_ERROR_CORRECTION_LEVEL_MEDIUM     = 1;
const static uint32_t JC_QRE_ERROR_CORRECTION_LEVEL_QUARTILE   = 2;
const static uint32_t JC_QRE_ERROR_CORRECTION_LEVEL_HIGH       = 3;

typedef struct _JCQRCode
{
    uint8_t* data;      // The modules of the qrcode
    uint32_t size;      // Size (in modules) of one side of the qrcode
    uint32_t version;   // Version [1,40]
    uint32_t ecl;       // Error correction level
    uint32_t _pad;
} JCQRCode;

/** Creates a QR Code
* Automatically selects the smallest version that fits the data.
* Then it also selects the highest possible error correction level withing that version, that still fits the data.
*
* @input Byte array
* @inputlength Size of input array
* @return 0 if the qr code couldn't be created. The returned qrcode must be deallocated with free()
*/
JCQRCode* jc_qrencode(const uint8_t* input, uint32_t inputlength);

/** Creates a QR Code
* If you know which version and error collection level you need
*
* @input Byte array
* @inputlength Size of input array
* @version Version [1,40]
* @ecl Error correction level (JC_QRE_ERROR_CORRECTION_LEVEL_LOW, JC_QRE_ERROR_CORRECTION_LEVEL_MEDIUM, JC_QRE_ERROR_CORRECTION_LEVEL_QUARTILE, JC_QRE_ERROR_CORRECTION_LEVEL_HIGH)
* @return 0 if the qr code couldn't be created. The returned qrcode must be deallocated with free()
*/
JCQRCode* jc_qrencode_version(const uint8_t* input, uint32_t inputlength, uint32_t version, uint32_t ecl);


#if defined(JC_QRENCODE_IMPLEMENTATION)

#if defined(__LP64__) || defined(_WIN64) || (defined(__x86_64__) && !defined(__ILP32__) ) || defined(_M_X64) || defined(__ia64) || defined (_M_IA64) || defined(__aarch64__) || defined(__powerpc64__)
    #define JC_QRE_IS64BIT 1
    #define JC_QRE_PAD(_X_) uint8_t _pad[_X_]
#else
    #define JC_QRE_PAD(_X_)
#endif


static void print_bits(const char* tag, uint32_t v, uint32_t count)
{
    printf("%s 0b", tag);
    for( uint32_t i = 0; i < count; ++i)
    {
        printf("%u", (v >> (count - 1 - i)) & 1);
        if( i == 31 )
        {
            printf(" ...");
            break;
        }
    }
    printf("\n");
}


static const uint8_t JC_QRE_INPUT_TYPE_NUMERIC         = 0;
static const uint8_t JC_QRE_INPUT_TYPE_ALPHANUMERIC    = 1;
static const uint8_t JC_QRE_INPUT_TYPE_BYTE            = 2;
static const uint8_t JC_QRE_INPUT_TYPE_KANJI           = 3;


static uint32_t JC_QRE_TYPE_BITS[] = {1, 2, 4, 8};

static int32_t JC_QRE_ALPHANUMERIC_MAPPINGS[] = {
// ' ',  !,  ",  #,  $,  %,  &,  ',  (,  ),  *,  +,  ,,  -,  .,  /,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  :,  ;,  <,  =,  >,  ?,  @,  A,  B,  C,  D,  E,  F,  G,  H,  I,  J,  K,  L,  M,  N,  O,  P,  Q,  R,  S,  T,  U,  V,  W,  X,  Y,  Z
    36, -1, -1, -1, 37, 38, -1, -1, -1, -1, 39, 40, -1, 41, 42, 43,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 44, -1, -1, -1, -1, -1, -1, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35
};

// Each array has 4 levels of error correction, and one extra (first) element in each
#define JC_QRE_INDEX(_ECL, _I) ((_ECL)*(JC_QRE_MAX_VERSION+1) + (_I))

//     0,    1,    2,    3,    4,    5,    6,    7,    8,    9,   10,   11,   12,   13,   14,   15,   16,   17,   18,   19,   20,   21,   22,   23,   24,   25,   26,   27,   28,   29,   30,   31,   32,   33,   34,   35,   36,   37,   38,   39,   40,
static uint32_t JC_QRE_DATA_CODEWORD_COUNT[] = {
       0,   19,   34,   55,   80,  108,  136,  156,  194,  232,  274,  324,  370,  428,  461,  523,  589,  647,  721,  795,  861,  932, 1006, 1094, 1174, 1276, 1370, 1468, 1531, 1631, 1735, 1843, 1955, 2071, 2191, 2306, 2434, 2566, 2702, 2812, 2956,
       0,   16,   28,   44,   64,   86,  108,  124,  154,  182,  216,  254,  290,  334,  365,  415,  453,  507,  563,  627,  669,  714,  782,  860,  914, 1000, 1062, 1128, 1193, 1267, 1373, 1455, 1541, 1631, 1725, 1812, 1914, 1992, 2102, 2216, 2334,
       0,   13,   22,   34,   48,   62,   76,   88,  110,  132,  154,  180,  206,  244,  261,  295,  325,  367,  397,  445,  485,  512,  568,  614,  664,  718,  754,  808,  871,  911,  985, 1033, 1115, 1171, 1231, 1286, 1354, 1426, 1502, 1582, 1666,
       0,    9,   16,   26,   36,   46,   60,   66,   86,  100,  122,  140,  158,  180,  197,  223,  253,  283,  313,  341,  385,  406,  442,  464,  514,  538,  596,  628,  661,  701,  745,  793,  845,  901,  961,  986, 1054, 1096, 1142, 1222, 1276,
};
static const uint32_t JC_QRE_ERROR_CORRECTION_CODEWORD_COUNT[] = {
       0,    7,   10,   15,   20,   26,   18,   20,   24,   30,   18,   20,   24,   26,   30,   22,   24,   28,   30,   28,   28,   28,   28,   30,   30,   26,   28,   30,   30,   30,   30,   30,   30,   30,   30,   30,   30,   30,   30,   30,   30,
       0,   10,   16,   26,   18,   24,   16,   18,   22,   22,   26,   30,   22,   22,   24,   24,   28,   28,   26,   26,   26,   26,   28,   28,   28,   28,   28,   28,   28,   28,   28,   28,   28,   28,   28,   28,   28,   28,   28,   28,   28,
       0,   13,   22,   18,   26,   18,   24,   18,   22,   20,   24,   28,   26,   24,   20,   30,   24,   28,   28,   26,   30,   28,   30,   30,   30,   30,   28,   30,   30,   30,   30,   30,   30,   30,   30,   30,   30,   30,   30,   30,   30,
       0,   17,   28,   22,   16,   22,   28,   26,   26,   24,   28,   24,   28,   22,   24,   24,   30,   28,   28,   26,   28,   30,   24,   30,   30,   30,   30,   30,   30,   30,   30,   30,   30,   30,   30,   30,   30,   30,   30,   30,   30,
};
static const uint32_t JC_QRE_CHARACTER_COUNT_BIT_SIZE[] = {
       0,   10,   10,   10,   10,   10,   10,   10,   10,   10,   12,   12,   12,   12,   12,   12,   12,   12,   12,   12,   12,   12,   12,   12,   12,   12,   12,   14,   14,   14,   14,   14,   14,   14,   14,   14,   14,   14,   14,   14,   14,
       0,    9,    9,    9,    9,    9,    9,    9,    9,    9,   11,   11,   11,   11,   11,   11,   11,   11,   11,   11,   11,   11,   11,   11,   11,   11,   11,   13,   13,   13,   13,   13,   13,   13,   13,   13,   13,   13,   13,   13,   13,
       0,    8,    8,    8,    8,    8,    8,    8,    8,    8,   16,   16,   16,   16,   16,   16,   16,   16,   16,   16,   16,   16,   16,   16,   16,   16,   16,   16,   16,   16,   16,   16,   16,   16,   16,   16,   16,   16,   16,   16,   16,
       0,    8,    8,    8,    8,    8,    8,    8,    8,    8,   10,   10,   10,   10,   10,   10,   10,   10,   10,   10,   10,   10,   10,   10,   10,   10,   10,   12,   12,   12,   12,   12,   12,   12,   12,   12,   12,   12,   12,   12,   12,
};
static const uint32_t JC_QRE_BLOCK_COUNT_GROUP1[] = {
       0,    1,    1,    1,    1,    1,    2,    2,    2,    2,    2,    4,    2,    4,    3,    5,    5,    1,    5,    3,    3,    4,    2,    4,    6,    8,   10,    8,    3,    7,    5,   13,   17,   17,   13,   12,    6,   17,    4,   20,   19,
       0,    1,    1,    1,    2,    2,    4,    4,    2,    3,    4,    1,    6,    8,    4,    5,    7,   10,    9,    3,    3,   17,   17,    4,    6,    8,   19,   22,    3,   21,   19,    2,   10,   14,   14,   12,    6,   29,   13,   40,   18,
       0,    1,    1,    2,    2,    2,    4,    2,    4,    4,    6,    4,    4,    8,   11,    5,   15,    1,   17,   17,   15,   17,    7,   11,   11,    7,   28,    8,    4,    1,   15,   42,   10,   29,   44,   39,   46,   49,   48,   43,   34,
       0,    1,    1,    2,    4,    2,    4,    4,    4,    4,    6,    3,    7,   12,   11,   11,    3,    2,    2,    9,   15,   19,   34,   16,   30,   22,   33,   12,   11,   19,   23,   23,   19,   11,   59,   22,    2,   24,   42,   10,   20,
};
static const uint32_t JC_QRE_BLOCK_COUNT_GROUP2[] = {
       0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    2,    0,    2,    0,    1,    1,    1,    5,    1,    4,    5,    4,    7,    5,    4,    4,    2,    4,   10,    7,   10,    3,    0,    1,    6,    7,   14,    4,   18,    4,    6,
       0,    0,    0,    0,    0,    0,    0,    0,    2,    2,    1,    4,    2,    1,    5,    5,    3,    1,    4,   11,   13,    0,    0,   14,   14,   13,    4,    3,   23,    7,   10,   29,   23,   21,   23,   26,   34,   14,   32,    7,   31,
       0,    0,    0,    0,    0,    2,    0,    4,    2,    4,    2,    4,    6,    4,    5,    7,    2,   15,    1,    4,    5,    6,   16,   14,   16,   22,    6,   26,   31,   37,   25,    1,   35,   19,    7,   14,   10,   10,   14,   22,   34,
       0,    0,    0,    0,    0,    2,    0,    1,    2,    4,    2,    8,    4,    4,    5,    7,   13,   17,   19,   16,   10,    6,    0,   14,    2,   13,    4,   28,   31,   26,   25,   28,   35,   46,    1,   41,   64,   46,   32,   67,   61,
};
static const uint32_t JC_QRE_CODEWORDS_PER_BLOCK_GROUP1[] = {
       0,   19,   34,   55,   80,  108,   68,   78,   97,  116,   68,   81,   92,  107,  115,   87,   98,  107,  120,  113,  107,  116,  111,  121,  117,  106,  114,  122,  117,  116,  115,  115,  115,  115,  115,  121,  121,  122,  122,  117,  118,
       0,   16,   28,   44,   32,   43,   27,   31,   38,   36,   43,   50,   36,   37,   40,   41,   45,   46,   43,   44,   41,   42,   46,   47,   45,   47,   46,   45,   45,   45,   47,   46,   46,   46,   46,   47,   47,   46,   46,   47,   47,
       0,   13,   22,   17,   24,   15,   19,   14,   18,   16,   19,   22,   20,   20,   16,   24,   19,   22,   22,   21,   24,   22,   24,   24,   24,   24,   22,   23,   24,   23,   24,   24,   24,   24,   24,   24,   24,   24,   24,   24,   24,
       0,    9,   16,   13,    9,   11,   15,   13,   14,   12,   15,   12,   14,   11,   12,   12,   15,   14,   14,   13,   15,   16,   13,   15,   16,   15,   16,   15,   15,   15,   15,   15,   15,   15,   16,   15,   15,   15,   15,   15,   15,
};
static const uint32_t JC_QRE_CODEWORDS_PER_BLOCK_GROUP2[] = {
       0,    0,    0,    0,    0,    0,    0,    0,    0,    0,   69,    0,   93,    0,  116,   88,   99,  108,  121,  114,  108,  117,  112,  122,  118,  107,  115,  123,  118,  117,  116,  116,    0,  116,  116,  122,  122,  123,  123,  118,  119,
       0,    0,    0,    0,    0,    0,    0,    0,   39,   37,   44,   51,   37,   38,   41,   42,   46,   47,   44,   45,   42,    0,    0,   48,   46,   48,   47,   46,   46,   46,   48,   47,   47,   47,   47,   48,   48,   47,   47,   48,   48,
       0,    0,    0,    0,    0,   16,    0,   15,   19,   17,   20,   23,   21,   21,   17,   25,   20,   23,   23,   22,   25,   23,   25,   25,   25,   25,   23,   24,   25,   24,   25,   25,   25,   25,   25,   25,   25,   25,   25,   25,   25,
       0,    0,    0,    0,    0,   12,    0,   14,   15,   13,   16,   13,   15,   12,   13,   13,   16,   15,   15,   14,   16,   17,    0,   16,   17,   16,   17,   16,   16,   16,   16,   16,   16,   16,   17,   16,   16,   16,   16,   16,   16,
};
// Each error correction level can have 8 format strings depending on what mask was applied
static const uint32_t JC_QRE_FORMAT_BITS[] = {
    30660, 29427, 32170, 30877, 26159, 25368, 27713, 26998,
    21522, 20773, 24188, 23371, 17913, 16590, 20375, 19104,
    13663, 12392, 16177, 14854, 9396, 8579, 11994, 11245,
    5769, 5054, 7399, 6608, 1890,  597, 3340, 2107,
};

// Each version has its version string
static const uint32_t JC_QRE_VERSION_BITS[] = {
       0,    0,    0,    0,    0,    0,    0, 31892, 34236, 39577, 42195, 48118, 51042, 55367, 58893, 63784, 68472, 70749, 76311, 79154, 84390, 87683, 92361, 96236, 102084, 102881, 110507, 110734, 117786, 119615, 126325, 127568, 133589, 136944, 141498, 145311, 150283, 152622, 158308, 161089, 167017,
};

static const uint32_t JC_QRE_ALIGNMENT_POSITIONS[] = {
       0,    0,    0,    0,    0,    0,    0,    0,
       0,    0,    0,    0,    0,    0,    0,    0,
       6,   18,    0,    0,    0,    0,    0,    0,
       6,   22,    0,    0,    0,    0,    0,    0,
       6,   26,    0,    0,    0,    0,    0,    0,
       6,   30,    0,    0,    0,    0,    0,    0,
       6,   34,    0,    0,    0,    0,    0,    0,
       6,   22,   38,    0,    0,    0,    0,    0,
       6,   24,   42,    0,    0,    0,    0,    0,
       6,   26,   46,    0,    0,    0,    0,    0,
       6,   28,   50,    0,    0,    0,    0,    0,
       6,   30,   54,    0,    0,    0,    0,    0,
       6,   32,   58,    0,    0,    0,    0,    0,
       6,   34,   62,    0,    0,    0,    0,    0,
       6,   26,   46,   66,    0,    0,    0,    0,
       6,   26,   48,   70,    0,    0,    0,    0,
       6,   26,   50,   74,    0,    0,    0,    0,
       6,   30,   54,   78,    0,    0,    0,    0,
       6,   30,   56,   82,    0,    0,    0,    0,
       6,   30,   58,   86,    0,    0,    0,    0,
       6,   34,   62,   90,    0,    0,    0,    0,
       6,   28,   50,   72,   94,    0,    0,    0,
       6,   26,   50,   74,   98,    0,    0,    0,
       6,   30,   54,   78,  102,    0,    0,    0,
       6,   28,   54,   80,  106,    0,    0,    0,
       6,   32,   58,   84,  110,    0,    0,    0,
       6,   30,   58,   86,  114,    0,    0,    0,
       6,   34,   62,   90,  118,    0,    0,    0,
       6,   26,   50,   74,   98,  122,    0,    0,
       6,   30,   54,   78,  102,  126,    0,    0,
       6,   26,   52,   78,  104,  130,    0,    0,
       6,   30,   56,   82,  108,  134,    0,    0,
       6,   34,   60,   86,  112,  138,    0,    0,
       6,   30,   58,   86,  114,  142,    0,    0,
       6,   34,   62,   90,  118,  146,    0,    0,
       6,   30,   54,   78,  102,  126,  150,    0,
       6,   24,   50,   76,  102,  128,  154,    0,
       6,   28,   54,   80,  106,  132,  158,    0,
       6,   32,   58,   84,  110,  136,  162,    0,
       6,   26,   54,   82,  110,  138,  166,    0,
       6,   30,   58,   86,  114,  142,  170,    0,
};

typedef struct _JCQRCodeBitBuffer
{
    uint8_t* bits;      // pointer to payload
    uint32_t numbits;   // current offset (in bits)
    JC_QRE_PAD(4);
} JCQRCodeBitBuffer;

typedef struct _JCQRCodeSegment
{
    JCQRCodeBitBuffer   data;
    uint32_t            offset;
    uint32_t            elementcount;   // character, bytes, or kanji characters
    uint8_t             type;           // numeric, alphanumeric, byte, kanji
    JC_QRE_PAD(7);
} JCQRCodeSegment;

typedef struct _JCQRCodeInternal
{
    JCQRCode qrcode; // the output

    uint8_t  bitbuffer[4096];       // storage for all the segments. Each segment starts at a byte boundary
    uint8_t  databuffer[4096];      // all segments merged into one buffer (appended one after each other)
    uint8_t  errorcorrection[4096]; // storage for the error correction code words
    uint8_t  interleaved[4096];     // all interleaved blocks, including error correction
    uint8_t  image[256*256];        // Enough to store the largest version (177*177)
    uint8_t  image_fun[256*256];    // Holds info about whether a module is a function module or not

    JCQRCodeSegment segments[8];
    uint32_t num_segments;
    uint32_t datasize;          // number of bytes used in databuffer
    uint32_t interleavedsize;   // number of bytes used in interleaved
} JCQRCodeInternal;


// Store the integer with the most significant bit first
static inline uint32_t _jc_qre_bitbuffer_write(uint8_t* buffer, uint32_t buffersize, uint32_t* cursor, uint32_t input, uint32_t numbits)
{
    uint32_t num_bytes_to_traverse = (numbits + 7) / 8;
    if( *cursor / 8 + num_bytes_to_traverse >= buffersize )
    {
        return 0;
    }
    if( numbits > 32 )
    {
        return 0;
    }

    uint32_t pos = *cursor; // in bits

//print_bits("encode:", input, numbits);

    for( uint32_t i = 0; i < numbits; ++i, ++pos)
    {
        uint32_t targetindex = 7 - pos & 0x7;

        uint32_t ii = numbits - 1 - i;
        uint8_t inputbit = (input >> ii) & 1;

        buffer[pos/8] |= (uint8_t)(inputbit << targetindex);
//printf(" %u  %u    %u: %u\n", pos, pos/8, i, (inputbit << targetindex));

//print_bits("byte:", buffer[pos/8], 8);

    }
    *cursor += numbits;

//printf("\n");

    return 1;
}

// Appends a previously encoded segment into another
static inline uint32_t _jc_qre_bitbuffer_append(uint8_t* buffer, uint32_t buffersize, uint32_t* cursor, uint8_t* input, uint32_t numbits)
{
    uint32_t num_bytes_to_traverse = (numbits + 7) / 8;
    if( *cursor + num_bytes_to_traverse >= buffersize )
        return 0;

    uint32_t pos = *cursor; // in bits
    for( uint32_t i = 0; i < numbits; ++i, ++pos )
    {
        uint32_t targetindex = 7 - pos & 0x7;

        uint8_t inputbit = input[i / 8] >> (7 - (i & 7)) & 1;

        // clear the target bit and OR in the input bit
        buffer[pos/8] |= (uint8_t)(inputbit << targetindex);
    }

    *cursor += numbits;
    return 1;
}

static inline uint32_t _jc_qre_bitbuffer_read(uint8_t* buffer, uint32_t buffersize, uint32_t* cursor, uint32_t numbits)
{
    uint32_t pos = *cursor; // in bits
    uint32_t value = 0;
    for( uint32_t i = 0; i < numbits; ++i, ++pos)
    {
        if( pos / 8 >= buffersize )
            break;

        uint8_t currentvalue = buffer[pos / 8];

        uint32_t srcindex = 7 - pos & 0x7;

        uint32_t srcbit = (currentvalue & (1 << srcindex)) ? 1 : 0;

        value = (value & ~(1 << i)) | (srcbit << i);
    }

    *cursor = pos;
    return value;
}

static inline uint32_t _jc_qre_guess_type_numeric(const uint8_t* input, uint32_t inputlength)
{
    for( uint32_t i = 0; i < inputlength; ++i )
    {
        if( input[i] < '0' || input[i] > '9' )
        {
            return 0;
        }
    }
    return 1;
}


static inline uint32_t _jc_qre_guess_type_alphanumeric(const uint8_t* input, uint32_t inputlength)
{
    for( uint32_t i = 0; i < inputlength; ++i )
    {
        uint8_t c = input[i];
        if( c < ' ' || c > 'Z' || JC_QRE_ALPHANUMERIC_MAPPINGS[c - ' '] == -1 )
        {
            return 0;
        }
    }
    return 1;
}

static inline uint8_t _jc_qre_guess_type(const uint8_t* input, uint32_t inputlength)
{
    if( _jc_qre_guess_type_numeric(input, inputlength) )
        return JC_QRE_INPUT_TYPE_NUMERIC;
    else if( _jc_qre_guess_type_alphanumeric(input, inputlength) )
        return JC_QRE_INPUT_TYPE_ALPHANUMERIC;

    return JC_QRE_INPUT_TYPE_BYTE;
}

static void _jc_qre_encode_numeric(JCQRCodeSegment* seg, uint32_t maxsize, const uint8_t* input, uint32_t inputlength)
{
    uint32_t accum = 0;
    for( uint32_t i = 0; i < inputlength; ++i, ++input)
    {
        char c = *(const char*)input;
        accum = accum * 10 + (uint32_t)(c - '0');
        if( i % 3 == 2 )
        {
            _jc_qre_bitbuffer_write(seg->data.bits, maxsize, &seg->data.numbits, accum, 10);
            accum = 0;
        }
    }
    if( accum )
    {
        uint32_t num = inputlength % 3;
        _jc_qre_bitbuffer_write(seg->data.bits, maxsize, &seg->data.numbits, accum, num * 3 + 1);
    }
    seg->elementcount = inputlength;
    seg->type = JC_QRE_INPUT_TYPE_NUMERIC;
}

static void _jc_qre_encode_alphanumeric(JCQRCodeSegment* seg, uint32_t maxsize, const uint8_t* input, uint32_t inputlength)
{
    const char* characters = (const char*)input;
    for( uint32_t i = 0; i < inputlength/2; ++i)
    {
        uint32_t v1 = (uint32_t)JC_QRE_ALPHANUMERIC_MAPPINGS[characters[i*2+0] - ' '];
        uint32_t v2 = (uint32_t)JC_QRE_ALPHANUMERIC_MAPPINGS[characters[i*2+1] - ' '];
        uint32_t value = (v1 * 45) + v2;

        _jc_qre_bitbuffer_write(seg->data.bits, maxsize, &seg->data.numbits, value, 11);

        //printf("hello: %c %c - > %u %u   %u bits\n", characters[i*2+0], characters[i*2+1], v1, v2, seg->data.numbits );
    }
    if( inputlength & 1 )
    {
        uint32_t v = (uint32_t)JC_QRE_ALPHANUMERIC_MAPPINGS[characters[inputlength-1] - ' '];
        _jc_qre_bitbuffer_write(seg->data.bits, maxsize, &seg->data.numbits, v, 6);
    }
    seg->elementcount = inputlength;
    seg->type = JC_QRE_INPUT_TYPE_ALPHANUMERIC;

    //printf("_jc_qre_encode_alphanumeric: %u chars   %u bits\n", inputlength, seg->data.numbits);
}

static void _jc_qre_encode_bytes(JCQRCodeSegment* seg, uint32_t maxsize, const uint8_t* input, uint32_t inputlength)
{
    (void)maxsize;
    memcpy(seg->data.bits, input, inputlength);
    seg->data.numbits = inputlength * 8;
    seg->elementcount = inputlength;
    seg->type = JC_QRE_INPUT_TYPE_BYTE;
}

static uint32_t _jc_qre_add_segment(JCQRCodeInternal* qr, const uint8_t* input, uint32_t inputlength)
{
    if( qr->num_segments >= sizeof(qr->segments)/sizeof(JCQRCodeSegment) )
        return 0xFFFFFFFF;

    JCQRCodeSegment* seg = &qr->segments[qr->num_segments];
    uint32_t offset = 0;
    for( uint32_t i = 0; i < qr->num_segments; ++i )
    {
        offset += qr->segments[i].offset;
    }
    seg->offset = offset;
    seg->data.bits = &qr->bitbuffer[offset];
    qr->num_segments++;

    seg->type = _jc_qre_guess_type(input, inputlength);

    uint32_t max_size = sizeof(qr->bitbuffer) - offset;
    if( inputlength >= max_size )
        return 0xFFFFFFFF;

    if( seg->type == JC_QRE_INPUT_TYPE_NUMERIC ) {
        _jc_qre_encode_numeric(seg, max_size, input, inputlength);
    } else if( seg->type == JC_QRE_INPUT_TYPE_ALPHANUMERIC ) {
        _jc_qre_encode_alphanumeric(seg, max_size, input, inputlength);
    } else if( seg->type == JC_QRE_INPUT_TYPE_BYTE ) {
        _jc_qre_encode_bytes(seg, max_size, input, inputlength);
    }

    return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Reed-Solomon functions

static uint8_t _jc_qre_rs_multiply(uint8_t x, uint8_t y)
{
    int32_t v = 0;
    for( int32_t i = 7; i >= 0; --i )
    {
        v = (v << 1) ^ ( (v >> 7) * 0x11D ); // Do modulo 0x11D if it overflows
        v ^= ((y >> i) & 1) * x;            // Only if y is odd
    }
    return (uint8_t)v;
}

// the generator [out] must have the size 'length'
static void _jc_qre_rs_create_generator(uint32_t length, uint8_t* coefficients)
{
    for( uint32_t i = 0; i < length; ++i )
    {
        coefficients[i] = 0;
    }
    coefficients[length-1] = 1;

    uint32_t power = 1;
    for( uint32_t i = 0; i < length; ++i )
    {
        for( uint32_t j = 0; j < length; ++j )
        {
            coefficients[j] = _jc_qre_rs_multiply(coefficients[j], (uint8_t)power);
            if( (j + 1) < length )
                coefficients[j] ^= coefficients[j+1];
        }
        uint32_t overflow = (power >> 7);           // If it became > 255, this is 1...
        power = (power << 1) ^ (overflow * 0x11D);  // ...in which case, we should do the modulo
    }
}

// length: length of coefficients
// coefficients: the rs generator coefficients
// datasize: the number of data code words
// data: the data code words
// out: the remainder coefficients
static void _jc_qre_rs_encode(uint32_t length, const uint8_t* coefficients, uint32_t datasize, const uint8_t* data, uint8_t* out)
{
    for( uint32_t i = 0; i < length; ++i )
    {
        out[i] = 0;
    }

    for( uint32_t i = 0; i < datasize; ++i )
    {
        uint32_t value = data[i] ^ out[0];
        // shift all elements one step
        for( uint32_t j = 1; j < length; ++j )
        {
            out[j-1] = out[j];
        }
        out[length-1] = 0;
        for( uint32_t j = 0; j < length; ++j )
        {
            out[j] ^= _jc_qre_rs_multiply(coefficients[j], (uint8_t)value);
        }
    }
}

static void _jc_qre_calc_error_correction(JCQRCodeInternal* qr)
{
    uint32_t index = JC_QRE_INDEX(qr->qrcode.ecl, qr->qrcode.version);
    uint32_t num_ec_codewords_per_block = JC_QRE_ERROR_CORRECTION_CODEWORD_COUNT[index];

    uint8_t coefficients[64]; // generator
    _jc_qre_rs_create_generator(num_ec_codewords_per_block, coefficients);

    uint32_t num_ec_codewords_total = 0;
    uint32_t data_offset = 0;

    uint32_t num_data_codewords_per_block = JC_QRE_CODEWORDS_PER_BLOCK_GROUP1[index];
    for( uint32_t i = 0; i < JC_QRE_BLOCK_COUNT_GROUP1[index]; ++i )
    {
        _jc_qre_rs_encode(num_ec_codewords_per_block, coefficients, num_data_codewords_per_block, &qr->databuffer[data_offset], &qr->errorcorrection[num_ec_codewords_total]);

        data_offset += num_data_codewords_per_block;
        num_ec_codewords_total += num_ec_codewords_per_block;
    }

    num_data_codewords_per_block = JC_QRE_CODEWORDS_PER_BLOCK_GROUP2[index];
    for( uint32_t i = 0; i < JC_QRE_BLOCK_COUNT_GROUP2[index]; ++i )
    {
        _jc_qre_rs_encode(num_ec_codewords_per_block, coefficients, num_data_codewords_per_block, &qr->databuffer[data_offset], &qr->errorcorrection[num_ec_codewords_total]);

        data_offset += num_data_codewords_per_block;
        num_ec_codewords_total += num_ec_codewords_per_block;
    }
}

static void _jc_qre_interleave_codewords(JCQRCodeInternal* qr)
{
    uint32_t index = JC_QRE_INDEX(qr->qrcode.ecl, qr->qrcode.version);
    uint32_t num_blocks = JC_QRE_BLOCK_COUNT_GROUP1[index] + JC_QRE_BLOCK_COUNT_GROUP2[index];

    uint32_t num_data_cw_block1 = JC_QRE_CODEWORDS_PER_BLOCK_GROUP1[index];
    uint32_t num_data_cw_block2 = JC_QRE_CODEWORDS_PER_BLOCK_GROUP2[index];
    uint32_t num_data_cw_block = num_data_cw_block1 > num_data_cw_block2 ? num_data_cw_block1 : num_data_cw_block2;

    uint32_t block_count_group1 = JC_QRE_BLOCK_COUNT_GROUP1[index];

    // where each group starts
    uint32_t groupoffset1 = 0;
    uint32_t groupoffset2 = num_data_cw_block1 * block_count_group1;

    // We have data code words in consecutive order, block after block
    // need to interleave those into final buffer
    // The blocks in group 1 have smaller size than those in group 2
    uint32_t total = 0;
    for( uint32_t cw = 0; cw < num_data_cw_block; ++cw)
    {
        for( uint32_t i = 0; i < num_blocks; ++i)
        {
            // are we in group 1 or 2?
            uint32_t group1 = i < block_count_group1;
            // number of data codewords in group
            uint32_t data_cw_count = group1 ? num_data_cw_block1 : num_data_cw_block2;
            if( cw >= data_cw_count )
            {
                // since the blocks in group 1 are shorter than those in block 2
                continue;
            }

            uint32_t offset = group1 ? groupoffset1 : groupoffset2;
            uint32_t blocknum = group1 ? i : i - block_count_group1;

            qr->interleaved[total++] = qr->databuffer[offset + blocknum * data_cw_count + cw];
        }
    }

    // Interleave the error correction codewords
    uint32_t num_ec_codewords_per_block = JC_QRE_ERROR_CORRECTION_CODEWORD_COUNT[index];
    for( uint32_t cw = 0; cw < num_ec_codewords_per_block; ++cw)
    {
        for( uint32_t i = 0; i < num_blocks; ++i)
        {
            qr->interleaved[total++] = qr->errorcorrection[i * num_ec_codewords_per_block + cw];
        }
    }
    qr->interleavedsize = total;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


static inline void _jc_qre_draw_module(JCQRCodeInternal* qr, int32_t x, int32_t y, uint8_t black)
{
    qr->image[y * 256 + x] = black ? 0 : 255;
}
static inline uint8_t _jc_qre_get_module(JCQRCodeInternal* qr, int32_t x, int32_t y)
{
    return qr->image[y * 256 + x];
}
static inline void _jc_qre_draw_function_module(JCQRCodeInternal* qr, int32_t x, int32_t y, uint8_t black)
{
    qr->image[y * 256 + x] = black ? 0 : 255;
    qr->image_fun[y * 256 + x] = 1;
}
static inline uint32_t _jc_qre_is_function_module(JCQRCodeInternal* qr, int32_t x, int32_t y)
{
    return qr->image_fun[y * 256 + x] != 0 ? 1 : 0;
}

static void _jc_qre_draw_finder_pattern(JCQRCodeInternal* qr, int32_t x, int32_t y)
{
    int size = (int)qr->qrcode.size;

    for( int j = -4; j <= 4; ++j )
    {
        int yy = y + j;
        if( yy < 0 || yy >= size )
            continue;
        for( int i = -4; i <= 4; ++i )
        {
            int xx = x + i;
            if( xx < 0 || xx >= size )
                continue;
            int iabs = i < 0 ? -i : i;
            int jabs = j < 0 ? -j : j;
            int max = iabs > jabs ? iabs : jabs;

            _jc_qre_draw_function_module(qr, xx, yy, max != 2 && max != 4);
        }
    }
}

static void _jc_qre_draw_alignment_pattern(JCQRCodeInternal* qr, int32_t x, int32_t y)
{
    if( _jc_qre_is_function_module(qr, x, y) )
        return;
    for( int j = -2; j <= 2; ++j )
    {
        int yy = y + j;
        for( int i = -2; i <= 2; ++i )
        {
            int xx = x + i;
            int iabs = i < 0 ? -i : i;
            int jabs = j < 0 ? -j : j;
            int max = iabs > jabs ? iabs : jabs;

            _jc_qre_draw_function_module(qr, xx, yy, max == 0 || max == 2);
        }
    }
}


static void _jc_qre_draw_data(JCQRCodeInternal* qr)
{
    uint32_t size = qr->qrcode.size;

    uint32_t bitindex = 0;

    uint32_t upwards = 1;
    uint32_t x = size - 1;

/*
    printf("qr->interleavedsize * 8: %u\n", qr->interleavedsize * 8);

    print_bits("byte 0: ", qr->interleaved[0], 8);
    print_bits("byte 1: ", qr->interleaved[1], 8);
    print_bits("byte 2: ", qr->interleaved[2], 8);
    print_bits("byte 3: ", qr->interleaved[3], 8);
*/

    while( bitindex < qr->interleavedsize * 8 )
    {
        for( uint32_t i = 0; i < size * 2; ++i )
        {
            uint32_t xx = x - (i & 1);
            uint32_t y = upwards ? size - 1 - (i >> 1) : (i >> 1);

            if(!_jc_qre_is_function_module(qr, xx, y))
            {
                uint32_t bit = _jc_qre_bitbuffer_read(qr->interleaved, qr->interleavedsize, &bitindex, 1);
                _jc_qre_draw_module(qr, xx, y, bit != 0);
            }
        }
        upwards ^= 1;
        x -= 2;
        if( x == 6 )
            x = 5;
    }
}


static void _jc_qre_draw_finder_patterns(JCQRCodeInternal* qr)
{
    uint32_t size = qr->qrcode.size;

    _jc_qre_draw_finder_pattern(qr, 3, 3);
    _jc_qre_draw_finder_pattern(qr, size-4, 3);
    _jc_qre_draw_finder_pattern(qr, 3, size-4);

    // alignment patterns
    if( qr->qrcode.version > 1 )
    {
        for( uint32_t iy = 0; iy < 8; ++iy )
        {
            uint32_t y = JC_QRE_ALIGNMENT_POSITIONS[qr->qrcode.version * 8 + iy];
            if( y == 0 )
                continue;
            for( uint32_t ix = 0; ix < 8; ++ix )
            {
                uint32_t x = JC_QRE_ALIGNMENT_POSITIONS[qr->qrcode.version * 8 + ix];
                if( x == 0 )
                    continue;
                _jc_qre_draw_alignment_pattern(qr, x, y);
            }
        }
    }

    // timing modules
    for( uint32_t i = 0; i < (uint32_t)size; ++i )
    {
        if( !_jc_qre_is_function_module(qr, i, 6))
            _jc_qre_draw_function_module(qr, i, 6, i % 2 == 0);
        if( !_jc_qre_is_function_module(qr, 6, i))
            _jc_qre_draw_function_module(qr, 6, i, i % 2 == 0);
    }

    // the dark module
    _jc_qre_draw_function_module(qr, 8, (4 * qr->qrcode.version) + 9, 1);
}

static void _jc_qre_draw_format(JCQRCodeInternal* qr, uint32_t pattern_mask)
{
    uint32_t size = qr->qrcode.size;
    uint32_t format = JC_QRE_FORMAT_BITS[qr->qrcode.ecl * 8 + pattern_mask];

/*
printf("version: %u  ecl: %d\n", qr->qrcode.version, qr->qrcode.ecl);
printf("pattern_mask: %u\n", pattern_mask);
    printf("format: %u, 0x%08x\n", format, format);
    print_bits("format:", format, 15);
*/
    // top right
    for( uint32_t i = 0; i < 8; ++i )
    {
        _jc_qre_draw_function_module(qr, size - 1 - i, 8, (format >> i) & 1);
    }
    // bottom left
    for( uint32_t i = 0; i < 7; ++i )
    {
        _jc_qre_draw_function_module(qr, 8, size - 7 + i, (format >> (8+i)) & 1);
    }

    // top left -- right side
    for( uint32_t i = 0; i < 6; ++i )
    {
        _jc_qre_draw_function_module(qr, 8, i, (format >> i) & 1);
    }

    _jc_qre_draw_function_module(qr, 8, 7, (format >> 6) & 1);
    _jc_qre_draw_function_module(qr, 8, 8, (format >> 7) & 1);
    _jc_qre_draw_function_module(qr, 7, 8, (format >> 8) & 1);

    for( uint32_t i = 0; i < 6; ++i )
    {
        _jc_qre_draw_function_module(qr, 5 - i, 8, (format >> (9 + i))  & 1);
    }
}

static inline uint32_t _jc_qre_is_masked(uint32_t x, uint32_t y, uint32_t pattern_mask)
{
    switch(pattern_mask)
    {
        case 0: return ((x + y) & 1) == 0;
        case 1: return (y & 1) == 0;
        case 2: return (x % 3) == 0;
        case 3: return ((x + y) % 3) == 0;
        case 4: return ((x / 3 + y / 2) & 1) == 0;
        case 5: return ((x * y) & 1 + (x * y) % 3) == 0;
        case 6: return (((x * y) & 1 + (x * y) % 3) & 1) == 0;
        case 7: return (((x + y) & 1 + (x * y) % 3) & 1) == 0;
        default: return 0;
    }
}

static void _jc_qre_draw_mask(JCQRCodeInternal* qr, uint32_t pattern_mask)
{
    uint32_t size = qr->qrcode.size;
    for( uint32_t y = 0; y < size; ++y )
    {
        for( uint32_t x = 0; x < size; ++x )
        {
            if(_jc_qre_is_function_module(qr, x, y))
            {
                continue;
            }
            if(_jc_qre_is_masked(x, y, pattern_mask))
                qr->image[y * 256 + x] ^= 255;
        }
    }
}

static inline uint32_t _jc_qre_calc_penalty(JCQRCodeInternal* qr)
{
    uint32_t size = qr->qrcode.size;
    uint32_t penalty = 0;

    // 5 or more in a row
    for( uint32_t y = 0; y < size; ++y )
    {
        uint8_t color = _jc_qre_get_module(qr, 0, y);
        uint32_t consecutive = 1;
        for( uint32_t x = 1; x < size; ++x )
        {
            uint8_t nextcolor = _jc_qre_get_module(qr, x, y);
            if( color == nextcolor )
            {
                consecutive++;
            }
            else
            {
                color = nextcolor;
                penalty += consecutive >= 5 ? 3 + (consecutive-5) : 0;
                consecutive = 1;
            }
        }
        penalty += consecutive >= 5 ? 3 + (consecutive-5) : 0;
    }
    // 5 or more in a column
    for( uint32_t x = 0; x < size; ++x )
    {
        uint8_t color = _jc_qre_get_module(qr, x, 0);
        uint32_t consecutive = 1;
        for( uint32_t y = 1; y < size; ++y )
        {
            uint8_t nextcolor = _jc_qre_get_module(qr, x, y);
            if( color == nextcolor )
            {
                consecutive++;
            }
            else
            {
                color = nextcolor;
                penalty += consecutive >= 5 ? 3 + (consecutive-5) : 0;
                consecutive = 1;
            }
        }
        penalty += consecutive >= 5 ? 3 + (consecutive-5) : 0;
    }

/*
    // find 2x2 blocks of same color
    for( uint32_t y = 0; y < size-1; ++y )
    {
        for( uint32_t x = 0; x < size-1; ++x )
        {
            uint8_t color1 = _jc_qre_get_module(qr, x, y);
            uint8_t color2 = _jc_qre_get_module(qr, x+1, y);
            uint8_t color3 = _jc_qre_get_module(qr, x, y+1);
            uint8_t color4 = _jc_qre_get_module(qr, x+1, y+1);
            penalty += (color1 & color2 & color3 & color4 & 1) * 3;
        }
    }

    // find special patterns
    uint32_t mask = (1 << 12) - 1;
    uint32_t pattern1 = 0x5d0; // 11 bits long
    uint32_t pattern2 = 0x5d;
    for( uint32_t y = 0; y < size; ++y )
    {
        uint32_t bits = 0;
        for( uint32_t x = 0; x < 11; ++x )
        {
            bits = bits << 1 | (_jc_qre_get_module(qr, x, y) & 1);
        }
        for( uint32_t x = 11; x < size; ++x )
        {
            bits = ((bits << 1) & mask) | (_jc_qre_get_module(qr, x, y) & 1);
            penalty += (bits == pattern1 || bits == pattern2) ? 40 : 0;
        }
    }

    for( uint32_t x = 0; x < size; ++x )
    {
        uint32_t bits = 0;
        for( uint32_t y = 0; y < 11; ++y )
        {
            bits = bits << 1 | (_jc_qre_get_module(qr, x, y) & 1);
        }
        for( uint32_t y = 11; y < size; ++y )
        {
            bits = ((bits << 1) & mask) | (_jc_qre_get_module(qr, x, y) & 1);
            penalty += (bits == pattern1 || bits == pattern2) ? 40 : 0;
        }
    }

    int32_t num_black = 0;
    for( uint32_t y = 0; y < size; ++y )
    {
        for( uint32_t x = 0; x < size; ++x )
        {
            num_black += _jc_qre_get_module(qr, x, y) & 1;
        }
    }
    int32_t ratio = (num_black * 100) / (size*size);
    int32_t multiples_of_5 = ratio / 5;
    int32_t value1 = (multiples_of_5 * 5) - 50;
    int32_t value2 = value1 + 5;
    value1 = value1 < 0 ? -value1 : value1;
    value2 = value2 < 0 ? -value2 : value2;
    value1 = value1 / 5;
    value2 = value2 / 5;
    penalty += ((value1 < value2) ? value1 : value2) * 10;
*/

    return penalty;
}

static void _jc_qre_draw_version(JCQRCodeInternal* qr)
{
    if( qr->qrcode.version < 7 )
        return;

    uint32_t size = qr->qrcode.size;
    uint32_t format = JC_QRE_VERSION_BITS[qr->qrcode.version];

    for( uint32_t x = 0; x < 6; ++x )
    {
        for( uint32_t y = 0; y < 3; ++y )
        {
            uint32_t bit = (format >> (x*3 + y)) & 1;
            uint32_t yy = size - 11 + y;
            // Bottom left
            _jc_qre_draw_function_module(qr, x, yy, bit != 0);
            // Top right
            _jc_qre_draw_function_module(qr, yy, x, bit != 0);
        }
    }
}

static void _jc_qre_draw_image(JCQRCodeInternal* qr)
{
    memset(qr->image, 255, sizeof(qr->image));
    memset(qr->image_fun, 0, sizeof(qr->image_fun));
    qr->qrcode.data = qr->image;
    qr->qrcode.size = (qr->qrcode.version-1)*4 + 21;

    _jc_qre_draw_finder_patterns(qr);
    _jc_qre_draw_format(qr, 0); // reserve area
    _jc_qre_draw_version(qr); // reserve area

    _jc_qre_draw_data(qr);

    // apply masks and find the best one
    uint32_t best_mask = 0;

    uint32_t lowest_score = 0xFFFFFFFF;
    for( uint32_t i = 0; i < 8; ++i )
    {
        _jc_qre_draw_format(qr, i);
        _jc_qre_draw_mask(qr, i);
        uint32_t score = _jc_qre_calc_penalty(qr);

        if( score < lowest_score )
        {
            best_mask = i;
            lowest_score = score;
        }
        _jc_qre_draw_mask(qr, i); // undo the mask (using xor)
    }

// best_mask = 0;
//     printf("best mask: %u  score: %u\n", best_mask, lowest_score);

    _jc_qre_draw_format(qr, best_mask);
    _jc_qre_draw_mask(qr, best_mask);
    _jc_qre_draw_version(qr);
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static inline uint32_t _jc_qre_get_total_bits(JCQRCodeInternal* qr, uint32_t version)
{
    uint32_t numbits = 0;
    for( uint32_t i = 0; i < qr->num_segments; ++i )
    {
        JCQRCodeSegment* seg = &qr->segments[i];
        uint32_t character_bit_count = JC_QRE_CHARACTER_COUNT_BIT_SIZE[JC_QRE_INDEX(seg->type, version)];
        numbits += 4 + character_bit_count + seg->data.numbits;
    }
    return numbits;
}

static JCQRCode* _jc_qrencode_internal(JCQRCodeInternal* qr)
{

    uint32_t capacity_bits = JC_QRE_DATA_CODEWORD_COUNT[ JC_QRE_INDEX(qr->qrcode.ecl, qr->qrcode.version) ] * 8;

//printf("Writing segments\n");
    uint32_t datasize = 0;
    for( uint32_t i = 0; i < qr->num_segments; ++i )
    {
        JCQRCodeSegment* seg = &qr->segments[i];
        _jc_qre_bitbuffer_write(qr->databuffer, sizeof(qr->databuffer), &datasize, JC_QRE_TYPE_BITS[seg->type], 4);

//print_bits("mode:", JC_QRE_TYPE_BITS[seg->type], 4);

//printf("line: %d datasize %u\n", __LINE__, datasize);
        _jc_qre_bitbuffer_write(qr->databuffer, sizeof(qr->databuffer), &datasize, seg->elementcount, JC_QRE_CHARACTER_COUNT_BIT_SIZE[JC_QRE_INDEX(seg->type, qr->qrcode.version)]);

//print_bits("numchars:", seg->elementcount, JC_QRE_CHARACTER_COUNT_BIT_SIZE[JC_QRE_INDEX(seg->type, version)]);

//printf("line: %d datasize %u\n", __LINE__, datasize);
        _jc_qre_bitbuffer_append(qr->databuffer, sizeof(qr->databuffer), &datasize, seg->data.bits, seg->data.numbits);


//printf("num bits:  %u\n", datasize);

//print_bits("byte 0:", qr->databuffer[0], 8);
//print_bits("byte 1:", qr->databuffer[1], 8);
//print_bits("byte 2:", qr->databuffer[2], 8);
//print_bits("byte 3:", qr->databuffer[3], 8);
//print_bits("byte 4:", qr->databuffer[4], 8);


    }

//printf("VERSION: %d   ECL: %d  capacity_bits: %d\n", qr->qrcode.version, qr->qrcode.ecl, capacity_bits);

//printf("Writing terminator\n");

    // add terminator (max 4 zeros)
    uint32_t terminator_length = capacity_bits - datasize;
    if( terminator_length > 4 )
        terminator_length = 4;
    uint32_t zeros = 0;
    if( terminator_length )
    {
        _jc_qre_bitbuffer_write(qr->databuffer, sizeof(qr->databuffer), &datasize, zeros, terminator_length);
    }

    // make it 8 bit aligned
    uint32_t numpadzeros = (8 - (datasize & 0x7)) & 0x7;
    _jc_qre_bitbuffer_write(qr->databuffer, sizeof(qr->databuffer), &datasize, zeros, numpadzeros);

    // pad with bytes
    uint8_t padding[2] = { 0xEC, 0x11 };
    uint32_t numpadbytes = (capacity_bits - datasize) / 8;

    for( uint32_t i = 0; i < numpadbytes; ++i )
    {
        _jc_qre_bitbuffer_write(qr->databuffer, sizeof(qr->databuffer), &datasize, padding[i&1], 8);
    }

    qr->datasize = datasize / 8;

    _jc_qre_calc_error_correction(qr);
    _jc_qre_interleave_codewords(qr);
    _jc_qre_draw_image(qr);

    return (JCQRCode*)&qr->qrcode;
}

JCQRCode* jc_qrencode(const uint8_t* input, uint32_t inputlength)
{
    JCQRCodeInternal* qr = (JCQRCodeInternal*)malloc( sizeof(JCQRCodeInternal) );
    memset(qr, 0, sizeof(JCQRCodeInternal) );

    uint32_t result = _jc_qre_add_segment(qr, input, inputlength);

    if( result == 0xFFFFFFFF )
    {
        // todo: error codes
        free(qr);
        return 0;
    }

        // Pick the smallest version that fits the data
    uint32_t version = 0;
    uint32_t numbits = 0;
    for( uint32_t i = JC_QRE_MIN_VERSION; i <= JC_QRE_MAX_VERSION; ++i )
    {
        uint32_t _numbits = _jc_qre_get_total_bits(qr, i);
        uint32_t capacity_bits = JC_QRE_DATA_CODEWORD_COUNT[ JC_QRE_INDEX(JC_QRE_ERROR_CORRECTION_LEVEL_LOW, i) ] * 8;
        if( _numbits < capacity_bits )
        {
            version = i;
            numbits = _numbits;
            break;
        }
    }

    // The data was too large
    if( !version )
    {
        free(qr);
        return 0;
    }

    qr->qrcode.version = version;
    qr->qrcode.ecl = JC_QRE_ERROR_CORRECTION_LEVEL_LOW;

    // Pick the highest error correction that fits within the same version
    for( uint32_t i = JC_QRE_ERROR_CORRECTION_LEVEL_MEDIUM; i <= JC_QRE_ERROR_CORRECTION_LEVEL_HIGH; ++i )
    {
        uint32_t capacity_bits = JC_QRE_DATA_CODEWORD_COUNT[ JC_QRE_INDEX(i, version) ] * 8;
        if( numbits < capacity_bits )
        {
            qr->qrcode.ecl = i;
        }
    }

    _jc_qrencode_internal(qr);
    return (JCQRCode*)&qr->qrcode;
}

JCQRCode* jc_qrencode_version(const uint8_t* input, uint32_t inputlength, uint32_t version, uint32_t ecl)
{
    JCQRCodeInternal* qr = (JCQRCodeInternal*)malloc( sizeof(JCQRCodeInternal) );
    memset(qr, 0, sizeof(JCQRCodeInternal) );

    uint32_t result = _jc_qre_add_segment(qr, input, inputlength);

    if( result == 0xFFFFFFFF )
    {
        // todo: error codes
        free(qr);
        return 0;
    }

    qr->qrcode.version = version;
    qr->qrcode.ecl = ecl;

    uint32_t numbits = _jc_qre_get_total_bits(qr, version);
    uint32_t capacity_bits = JC_QRE_DATA_CODEWORD_COUNT[ JC_QRE_INDEX(ecl, version) ] * 8;
    if( numbits > capacity_bits )
    {
        free(qr);
        return 0;
    }

    _jc_qrencode_internal(qr);
    return (JCQRCode*)&qr->qrcode;
}

#undef JC_QRE_INDEX

#endif // JC_QRENCODE_IMPLEMENTATION
#endif // JC_QRENCODE_H
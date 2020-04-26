/*******************************************************************************
 Copyright 2001, 2002 Georges Menie (<URL snipped>)
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU Lesser General Public License for more details.
  You should have received a copy of the GNU Lesser General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/
// *******************************************************************************
// This small printf function is a tiny implementation of the standard C library 
// function, it does not include all the capabilities of the standard one.
// It has been designed to have a small footprint and to be easy to include in 
// an embedded software
// see https://www.menie.org/georges/embedded/small_printf_source_code.html
// *******************************************************************************
/*******************************************************************************
 putchar is the only external dependency for this file, 
*/
extern int (putchar)(int /*c*/);

// Uncoment to test code
// #define  TEST_PRINTF

// ASCII character set... 
#define NUL         0x00
#define STX         0x02
#define ETX         0x03
#define EOT         0x04
#define ACK         0x06
#define BS          0x08
#define LF          0x0A
#define CR          0x0D
#define DLE         0x10
#define ESC         0x1B
#define SPC         0x20
#define MAX_PRN     0x7E
#define DEL         0x7F

typedef  unsigned char  u8 ;
typedef  unsigned int   uint ;

static uint use_leading_plus = 0 ;
static int max_output_len = -1 ;
static int curr_output_len = 0 ;

static uint fixed_overrun = 0 ;
static int last_bad_char = 0 ;
static uint invalid_char = 0 ;

// ****************************************************************************
//  This version returns the length of the output string.
//  It is more useful when implementing a walking-string function.
// ****************************************************************************
static const double round_nums[8] = {  
   0.5,
   0.05,
   0.005,
   0.0005,
   0.00005,
   0.000005,
   0.0000005,
   0.00000005
} ;

#define  PAD_RIGHT   1
#define  PAD_ZERO    2
#define PRINT_BUF_LEN 12
#define PRINT_LLBUF_LEN 22

static void printchar (char **str, int c);
static void dbl2stri(char *outbfr, double dbl, unsigned dec_digits);
static int prints (char **out, const char *string, int width, int pad);
static int printi (char **out, int i, uint base, int sign, int width, int pad, int letbase);
static int printlli (char **out, long long i, uint base, int sign, int width, int pad, int letbase);
static int print (char **out, int *varg);

#ifdef TEST_PRINTF
int termf (const char *format, ...);
int termfn(uint max_len, const char *format, ...);
#endif

int stringf(char *out, const char *format, ...);
int stringfn(char *out, uint max_len, const char *format, ...);

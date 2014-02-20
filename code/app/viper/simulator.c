/*
 *  simulator.c
 *  ------------
 *
 * Checksum simulator
 *
 *  
 *
 * XXXXXXXXXX The endianes in this code is completely botched!!!!!
 * sort it out before code release to avoid huge embarassment!!
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <linux/types.h>

#include "acenic_ioctls.h"
#include "rand.h"

typedef struct {
    int socket;
    struct ifreq ifr;
    struct acenic_ioc_req req;
} alteon_st;

alteon_st st;

#define TRUE  1
#define FALSE 0
#define STARS 0x42424242
#define SWATT_BASE 0x8b00
#define FW_SIZE 3072   /* 3K bytes*/

// coupon collector problem
// E[T] = N * (ln(N) + 0.6)
// N = 642
// ln(N) = 6.46
//#define NONCE_NUM 300//5000//4096
#define NONCE_FULL 0
#define MIPS_REG_NUM 32  /* 32 registers */
#define ADDR_MASK 0x7fc
#define JUMP_MASK 0x1c0
#define RANDOMNESS 0x43434343 /* initial seed of nonce. we can also use local time as initial nonce.*/
#define NONCE_COUNT NONCE_NUM
#define LOOP_COUNT 100000
#define I_NUM 4
#define SWATT_LOOP_NUM NONCE_NUM

#define DEBUG if(0)
#define DEBUG_CHECKSUM_SIMULATOR if(0) 
#define DEBUG_CHECKSUM_RESULT if(0)
#define DEBUG_OUTPUT_ZERO_BUF if(0)
#define DEBUG_OUTPUT_NONCE if(0)
#define ATTESTATION_CPUA
#define ATTESTATION_CPUB

/* global variables*/
__u32 g_swatt_buf[FW_SIZE/4];   /* 2k bytes*/
__u32 g_swatt_buf_s[FW_SIZE/4];
__u32 g_nonce_pool[NONCE_NUM];   /* the size of nonce pool is 2048 */
__u32 g_reg[MIPS_REG_NUM];
__u32 g_reg_state[MIPS_REG_NUM];
__u32 g_checksum[NONCE_NUM][2];
__u32 g_mm[FW_SIZE/4];
__u32 g_mm_s[FW_SIZE/4];
__u32 g_nonce_count;

/* MIPS is big-endian but PCI but gives us little-endian values, so
 * the ioctl routines need to switch them to big_endian so GDB gets
 * the contents right.  We can use ntohl, htonl to access control
 * register contents, &c. here in the stub.
 *
 * NB: 'offset' and card addresses are _host-endian_ and do not need htonl().
 */

static __inline__ __u32 
other_endian (__u32 val)
{
    return (((val & 0xff) << 24)
	    | ((val & 0xff00) << 8)
	    | ((val & 0xff0000) >> 8)
	    | ((val & 0xff000000) >> 24));
}

/* Read and write shared memory locations */

static __inline__ int
read_smem (alteon_st *nic, __u32 offset, __u32 *dpr)
{
    nic->req.cardoffset = offset;
    if (ioctl(nic->socket, ACENIC_IOCTL_READ_SMEM, &nic->ifr))
    {
	*dpr = other_endian(nic->req.data);
	return FALSE;
    }
    *dpr = other_endian(nic->req.data);
    return TRUE;
}

static __inline__ int 
write_smem (alteon_st *nic, __u32 offset, __u32 data)
{
    nic->req.cardoffset = offset;
    nic->req.data = other_endian(data);
    if (ioctl(nic->socket, ACENIC_IOCTL_WRITE_SMEM, &nic->ifr))
    {
	return FALSE;
    }

    //  printf("off %x : %x %x\n",offset, data, other_endian(nic->req.data) );
    return TRUE;
}

static __inline__ void
die (char *why)
{
    fprintf (stderr, "\nPanic -- %s\n\n", why);
    exit (-1);
}


static void
read_tbuf (__u32 offset, __u32 *buf)
{
    __u32 frame_base, current;
    int i;

    read_smem(&st, 0x68, &frame_base);
    frame_base = htonl(frame_base);
    current = offset;
    
    for (i=0; i < 8; i++) 
    {
	if ((current & 0xfffff800) != frame_base)
	{
	    frame_base = (current & 0xfffff800);
	    write_smem (&st, 0x68, ntohl(frame_base));
	}
	read_smem(&st, (current & 0x7ff) + 0x800, buf + i);
	current += 4;
    } 

    for (i = 0; i < 8; i++)
	buf[i] = ntohl(buf[i]);

}

void dump_swatt_code(__u32 tstart, __u32 tlen)
{
    
    __u32 p = tstart;
    __u32 *entry = g_swatt_buf;
    while( p < (tstart + tlen))
    {       
	read_tbuf (p, entry);
	p = p + 32;
        entry = entry + 8;
    }
    
    //printf (" %#010x %#010x %#010x %#010x %#010x %#010x %#010x %#010x\n",
    //	    g_swatt_buf[0], g_swatt_buf[1], g_swatt_buf[2], g_swatt_buf[3],
    //      g_swatt_buf[4], g_swatt_buf[5], g_swatt_buf[6], g_swatt_buf[7]);
  return;
}
void save_swatt_code(void)
{
  FILE *pFile;
  __u8 *buffer = (__u8*)g_swatt_buf;
  __u32 count = 0;

  pFile = fopen("fw.bin", "wb");
  count = fwrite(buffer, 1, FW_SIZE, pFile);
  //printf("write %d bytes\n", count);
  fclose(pFile);
  return;
}

void read_swatt_code(void)
{
  FILE *pFile;
  long lSize;
  size_t result;
  __u32 i = 0;

  __u8 *buffer = (__u8*)g_swatt_buf;

  for (i = 0; i < FW_SIZE/4; i++)
    g_swatt_buf[i] = 0;

  pFile = fopen("fw.bin", "r");
  fseek(pFile, 0, SEEK_END);
  lSize = ftell(pFile);
  rewind(pFile);
  
  if (lSize != FW_SIZE)
  {
    printf("file size is too large. %d\n", (int)lSize);
    fclose(pFile);
    return;
  }

  result = fread(buffer, 1, lSize, pFile);
  
  if (result != lSize)
  {
    printf("Read file error. %d\n", result);
    fclose(pFile);    
  }
  fclose(pFile);
  return;
}

void init_buf(void)
{
  __u32 i = 0;
  for (i = 0; i < FW_SIZE/4; i++)
  { 
     g_swatt_buf[i] = 0;
     g_swatt_buf_s[i] = 0;
     g_mm[i] = 0;
     g_mm_s[i] = 0;
  }

  return;
}

__u32 mms_left(void)
{
  __u32 count = 0;
  __u32 i = 0;
  __u32 istart = (0x8c00 - SWATT_BASE)/4;
  __u32 iend = (0x8c00 - SWATT_BASE + 16*4*8 + 8 + 2048)/4;
  
  for (i = istart; i < iend; i++)
  {
    if (g_mm_s[i] == 0)
      count++;
  }
  return count; 
}

__u32 mm_left(void)
{
  __u32 count = 0;
  __u32 i = 0;
  __u32 istart = (0x8c00 - SWATT_BASE)/4;
  __u32 iend = (0x8c00 - SWATT_BASE + 16*4*8 + 8 + 2048)/4;
  
  for (i = istart; i < iend; i++)
  {
    if (g_mm[i] == 0)
      count++;
  }
  return count; 
}

__u32 mms_updated(void)
{
  __u32 count = 0;
  __u32 i = 0;
  __u32 istart = (0x8c00 - SWATT_BASE)/4;
  __u32 iend = (0x8c00 - SWATT_BASE + 16*4*8 + 8 + 2048)/4;
  
  for (i = istart; i < iend; i++)
  {
    if ( (g_mm[i] == 0) && (g_mm_s[i] == 1))
      count++;
  }
  return count;
}

void zero_mms(void)
{
  __u32 i = 0;
  for (i = 0; i < FW_SIZE/4; i++)
  {
    g_mm_s[i] = 0;
  }
}


void copy_mms2mm(void)
{
  __u32 i = 0;
  for (i = 0; i < FW_SIZE/4; i++)
  {
    g_mm[i] = g_mm_s[i];
  }

}

void copy_mm2mms(void)
{
  __u32 i = 0;
  for (i = 0; i < FW_SIZE/4; i++)
  {
    g_mm_s[i] = g_mm[i];
  }

}

void clear_regs(void)
{
  __u32 i = 0;
  for (i = 0; i < MIPS_REG_NUM; i++)
    g_reg[i] = i;

  return;
}

void save_regs(void)
{
  __u32 i = 0;
  for (i = 0; i < MIPS_REG_NUM; i++)
    g_reg_state[i] = g_reg[i];

  return;
}

void restore_regs(void)
{
  __u32 i = 0;
  for (i = 0; i < MIPS_REG_NUM; i++)
    g_reg[i] = g_reg_state[i];

  return;
}

void return_checksum(__u32 index, __u32 nonce_index)
{
  __u32 *r = g_reg;
   __u32 tmp = index;
  __u32 inonce = nonce_index - 1;
  __u32 i = 0;
  DEBUG_CHECKSUM_RESULT printf("\nnonce_index %d\n", inonce);
    if (tmp == 0)
    {
      DEBUG_CHECKSUM_RESULT printf("\n");
      DEBUG_CHECKSUM_RESULT printf("return %d, checksum[%d]=%#x\n", tmp, (inonce-1), r[2]);
      g_checksum[(inonce-1)][0] = r[2];
      g_checksum[(inonce-1)][1] = 0;

    }  


    if (tmp == 1)
    {
      DEBUG_CHECKSUM_RESULT printf("\n");
     DEBUG_CHECKSUM_RESULT printf("return %d, checksum[%d]=%#x", tmp, (inonce-1), r[3]);
      g_checksum[(inonce-1)][0] = r[3];
      g_checksum[(inonce-1)][1] = 1;

    }  


    if (tmp == 2)
    {
      DEBUG_CHECKSUM_RESULT printf("\n");
DEBUG_CHECKSUM_RESULT      printf("return %d, checksum[%d]=%#x", tmp, (inonce-1), r[11]);
      g_checksum[(inonce-1)][0] = r[11];
      g_checksum[(inonce-1)][1] = 2;

    }  


    if (tmp == 3)
    {
      DEBUG_CHECKSUM_RESULT printf("\n");
DEBUG_CHECKSUM_RESULT      printf("return %d, checksum[%d]=%#x", tmp, (inonce-1), r[12]);
      g_checksum[(inonce-1)][0] = r[12];
      g_checksum[(inonce-1)][1] = 3;

    }  


    if (tmp == 4)
    {
      DEBUG_CHECKSUM_RESULT printf("\n");
DEBUG_CHECKSUM_RESULT      printf("return %d, checksum[%d]=%#x", tmp, (inonce-1), r[13]);
      g_checksum[(inonce-1)][0] = r[13];
      g_checksum[(inonce-1)][1] = 4;

    }  


    if (tmp == 5)
    {
      DEBUG_CHECKSUM_RESULT printf("\n");
DEBUG_CHECKSUM_RESULT      printf("return %d, checksum[%d]=%#x", tmp, (inonce-1), r[14]);
      g_checksum[(inonce-1)][0] = r[14];
      g_checksum[(inonce-1)][1] = 5;

    }  


    if (tmp == 6)
    {
      DEBUG_CHECKSUM_RESULT printf("\n");
DEBUG_CHECKSUM_RESULT      printf("return %d, checksum[%d]=%#x", tmp, (inonce-1), r[15]);
      g_checksum[(inonce-1)][0] = r[15];
      g_checksum[(inonce-1)][1] = 6;
    }  


    if (tmp == 7)
    {
      DEBUG_CHECKSUM_RESULT printf("\n");
DEBUG_CHECKSUM_RESULT      printf("return %d, checksum[%d]=%#x", tmp, (inonce-1), r[16]);
      g_checksum[(inonce-1)][0] = r[16];
      g_checksum[(inonce-1)][1] = 7;
    }  


    if (tmp == 8)
    {
      DEBUG_CHECKSUM_RESULT printf("\n");
DEBUG_CHECKSUM_RESULT      printf("return %d, checksum[%d]=%#x", tmp, (inonce-1), r[17]);
      g_checksum[(inonce-1)][0] = r[17];
      g_checksum[(inonce-1)][1] = 8;
    }  

    if (tmp == 9)
    {
      DEBUG_CHECKSUM_RESULT printf("\n");
DEBUG_CHECKSUM_RESULT      printf("return %d, checksum[%d]=%#x", tmp, (inonce-1), r[18]);
      g_checksum[(inonce-1)][0] = r[18];
      g_checksum[(inonce-1)][1] = 9;

    }  

    if (tmp == 10)
    {
      DEBUG_CHECKSUM_RESULT printf("\n");
DEBUG_CHECKSUM_RESULT      printf("return %d, checksum[%d]=%#x", tmp, (inonce-1), r[19]);
      g_checksum[(inonce-1)][0] = r[19];
      g_checksum[(inonce-1)][1] = 10;

    }  


    if (tmp == 11)
    {
      DEBUG_CHECKSUM_RESULT printf("\n");
DEBUG_CHECKSUM_RESULT      printf("return %d, checksum[%d]=%#x", tmp, (inonce-1), r[20]);
      g_checksum[(inonce-1)][0] = r[20];
      g_checksum[(inonce-1)][1] = 11;
    }  


    if (tmp == 12)
    {
      DEBUG_CHECKSUM_RESULT printf("\n");
DEBUG_CHECKSUM_RESULT      printf("return %d, checksum[%d]=%#x", tmp, (inonce-1), r[21]);
      g_checksum[(inonce-1)][0] = r[21];
      g_checksum[(inonce-1)][1] = 12;

    }  


    if (tmp == 13)
    {
      DEBUG_CHECKSUM_RESULT printf("\n");
DEBUG_CHECKSUM_RESULT      printf("return %d, checksum[%d]=%#x", tmp, (inonce-1), r[22]);
      g_checksum[(inonce-1)][0] = r[22];
      g_checksum[(inonce-1)][1] = 13;

    }  


    if (tmp == 14)
    {
      DEBUG_CHECKSUM_RESULT printf("\n");
DEBUG_CHECKSUM_RESULT      printf("return %d, checksum[%d]=%#x", tmp, (inonce-1), r[23]);
      g_checksum[(inonce-1)][0] = r[23];
      g_checksum[(inonce-1)][1] = 14;

    }  


    if (tmp == 15)
    {
      DEBUG_CHECKSUM_RESULT printf("\n");
DEBUG_CHECKSUM_RESULT      printf("return %d, checksum[%d]=%#x", tmp, (inonce-1), r[24]);
      g_checksum[(inonce-1)][0] = r[24];
      g_checksum[(inonce-1)][1] = 15;

    }  


    if (tmp == 16)
    {
      DEBUG_CHECKSUM_RESULT printf("\n");
DEBUG_CHECKSUM_RESULT      printf("return %d, checksum[%d]=%#x", tmp, (inonce-1), r[25]);
      g_checksum[(inonce-1)][0] = r[25];
      g_checksum[(inonce-1)][1] = 16;

    }  


    if (tmp == 17)
    {
      DEBUG_CHECKSUM_RESULT printf("\n");
DEBUG_CHECKSUM_RESULT      printf("return %d, checksum[%d]=%#x", tmp, (inonce-1), r[26]);
      g_checksum[(inonce-1)][0] = r[26];
      g_checksum[(inonce-1)][1] = 17;

    }  


    if (tmp == 18)
    {
      DEBUG_CHECKSUM_RESULT printf("\n");
DEBUG_CHECKSUM_RESULT      printf("return %d, checksum[%d]=%#x", tmp, (inonce-1), r[27]);
      g_checksum[(inonce-1)][0] = r[27];
      g_checksum[(inonce-1)][1] = 18;

    }  


    if (tmp == 19)
    {
      DEBUG_CHECKSUM_RESULT printf("\n");
DEBUG_CHECKSUM_RESULT      printf("return %d, checksum[%d]=%#x", tmp, (inonce-1), r[28]);
      g_checksum[(inonce-1)][0] = r[28];
      g_checksum[(inonce-1)][1] = 19;

    }  

    if (tmp == 20)
    {
      DEBUG_CHECKSUM_RESULT printf("\n");
DEBUG_CHECKSUM_RESULT      printf("return %d, checksum[%d]=%#x", tmp, (inonce-1), r[31]);
      g_checksum[(inonce-1)][0] = r[31];
      g_checksum[(inonce-1)][1] = 20;

    }  

    if (tmp == 21)
    {
      DEBUG_CHECKSUM_RESULT printf("\n");
DEBUG_CHECKSUM_RESULT      printf("return %d, checksum[%d]=%#x", tmp, (inonce-1), r[2]);
      g_checksum[(inonce-1)][0] = r[2];
      g_checksum[(inonce-1)][1] = 0;

    }  


    if (tmp == 22)
    {
      DEBUG_CHECKSUM_RESULT printf("\n");
DEBUG_CHECKSUM_RESULT      printf("return %d, checksum[%d]=%#x", tmp, (inonce-1), r[3]);
      g_checksum[(inonce-1)][0] = r[3];
      g_checksum[(inonce-1)][1] = 1;

    }  


    if (tmp == 23)
    {
      DEBUG_CHECKSUM_RESULT printf("\n");
DEBUG_CHECKSUM_RESULT      printf("return %d, checksum[%d]=%#x", tmp, (inonce-1), r[11]);
      g_checksum[(inonce-1)][0] = r[11];
      g_checksum[(inonce-1)][1] = 2;

    }  


    if (tmp == 24)
    {
      DEBUG_CHECKSUM_RESULT printf("\n");
DEBUG_CHECKSUM_RESULT      printf("return %d, checksum[%d]=%#x", tmp, (inonce-1), r[12]);
      g_checksum[(inonce-1)][0] = r[12];
      g_checksum[(inonce-1)][1] = 3;

    }  


    if (tmp == 25)
    {
      DEBUG_CHECKSUM_RESULT printf("\n");
DEBUG_CHECKSUM_RESULT      printf("return %d, checksum[%d]=%#x", tmp, (inonce-1), r[13]);
      g_checksum[(inonce-1)][0] = r[13];
      g_checksum[(inonce-1)][1] = 4;

    }  


    if (tmp == 26)
    {
      DEBUG_CHECKSUM_RESULT printf("\n");
DEBUG_CHECKSUM_RESULT      printf("return %d, checksum[%d]=%#x", tmp, (inonce-1), r[14]);
      g_checksum[(inonce-1)][0] = r[14];
      g_checksum[(inonce-1)][1] = 5;

    }  


    if (tmp == 27)
    {
      DEBUG_CHECKSUM_RESULT printf("\n");
DEBUG_CHECKSUM_RESULT      printf("return %d, checksum[%d]=%#x", tmp, (inonce-1), r[15]);
      g_checksum[(inonce-1)][0] = r[15];
      g_checksum[(inonce-1)][1] = 6;

    }  


    if (tmp == 28)
    {
      DEBUG_CHECKSUM_RESULT printf("\n");
DEBUG_CHECKSUM_RESULT      printf("return %d, checksum[%d]=%#x", tmp, (inonce-1), r[16]);
      g_checksum[(inonce-1)][0] = r[16];
      g_checksum[(inonce-1)][1] = 7;

    }  


    if (tmp == 29)
    {
      DEBUG_CHECKSUM_RESULT printf("\n");
DEBUG_CHECKSUM_RESULT      printf("return %d, checksum[%d]=%#x", tmp, (inonce-1), r[17]);
      g_checksum[(inonce-1)][0] = r[17];
      g_checksum[(inonce-1)][1] = 8;

    }  


    if (tmp == 30)
    {
      DEBUG_CHECKSUM_RESULT printf("\n");
DEBUG_CHECKSUM_RESULT      printf("return %d, checksum[%d]=%#x", tmp, (inonce-1), r[18]);
      g_checksum[(inonce-1)][0] = r[18];
      g_checksum[(inonce-1)][1] = 9;

    }  

    if (tmp == 31)
    {
      DEBUG_CHECKSUM_RESULT printf("\n");
DEBUG_CHECKSUM_RESULT      printf("return %d, checksum[%d]=%#x", tmp, (inonce-1), r[19]);
      g_checksum[(inonce-1)][0] = r[19];
      g_checksum[(inonce-1)][1] = 10;

    }  

}

void generate_nonce(void)
{
  __u32 *r = g_reg;
  __u32 *buf = g_swatt_buf;
  __u32 *mm = g_mm;
  __u32 *mms = g_mm_s;
  __u32 tmp = 0;
  __u32 *nonce_buf = g_nonce_pool;
  __u32 nonce = RANDOMNESS;
  __u8 u8_nonce_buf[4];
  __u32 count = 0;
  __u32 icount = 0;
  __u32 inonce = 0;
  __u32 i = 0;
  __u32 inew = 0;
  __u32 ileft = 0;

  DEBUG printf("begin to generate nonce pool\n");
  // initialization
  // initialize r2, r3, r11 to r28
  r[2] = 0x2023;
  r[3] = 0x2084;
  r[11] = 0x33f0;
  r[12] = 0x33f1;
  r[13] = 0x33f2;
  r[14] = 0x33f3;
  r[15] = 0x33f4;
  r[16] = 0x33f5;
  r[17] = 0x33f6;
  r[18] = 0x33f7;
  r[19] = 0x33f8;
  r[20] = 0x33f9;
  r[21] = 0x33fa;
  r[22] = 0x33fb;
  r[23] = 0x33fc;
  r[24] = 0x33fd;
  r[25] = 0x33fe;
  r[26] = 0x33ff;
  r[27] = 0x33f0;
  r[28] = 0x33f1;    

  // initialize r7 and r8
  r[7] = 0x33f2;
  r[8] = 0x33f3;
  r[7] = r[7] & ADDR_MASK;
  r[8] = r[8] & ADDR_MASK;

  //initialize r9, r9
  r[9] = 0x8c00;  // base address
  r[10] = 0x8c00; 
  r[31] = 0xbffe4c;   

  save_regs();
  nonce = 0x33f233f3;

 __block0:
  r[5] = r[10] + r[7];    //      add $5, $10, $7 
  tmp = r[5] - SWATT_BASE;
  mms[tmp/4] = 1;

  r[6] = buf[tmp/4];    //        lw $6, 0($5)
  DEBUG   printf("block 0 offset is %d, buf value %#x, r[7] is %#x\n", tmp, r[6], r[7]);
  
  r[5] = r[6] ^ r[5];   //        xor $5, $6, $5
 
  r[11] = r[26] ^ r[5]; //        xor $11, $26, $5
  //  r[7] = r[31] & ADDR_MASK;   //  and $7, $31, 0x07fc  
  r[7] = r[11] & ADDR_MASK;
  r[5] = r[9] + r[7];   //        add $5, $9, $7
  tmp = r[5] - SWATT_BASE;
  mms[tmp/4] = 1;

  r[6] = buf[tmp/4];    //        lw $6, 0($5)
  r[5] = r[6] ^ r[5];   //        xor $5, $6, $5
  r[2] = r[27] ^ r[5];  //        xor $2, $27, $5
  r[3] = r[2] + r[2];   //        add $3, $2, $2   
  r[7] = r[3] & ADDR_MASK;//        and $7, $3, 0x07fc
  r[5] = r[3] & JUMP_MASK;//        and $5, $3, 0x1c0    # mask, 1,1100,0000
  r[9] = r[9] ^ r[5];   //        xor $9, $9, $5
  DEBUG   printf("block 0, r3 is %#x, r7 is %#x\n", r[3], r[7]);

  icount++;
  if (icount >= I_NUM)
  {
    icount = I_NUM + 1;
    goto __checksum_end;
  }

  r[10] = 0x8c00 + 16*4 + 8 - 8;

  goto __jump_table;
  //goto __checksum_end;
  
  //block 1
 __block1:
  r[5] = r[9] + r[8];        //        add $5, $9, $8 
  tmp = r[5] - SWATT_BASE;
  mms[tmp/4] = 1;

  r[6] = buf[tmp/4];         //        lw $6, 0($5)
  DEBUG   printf("block 1 offset is %d, buf value %#x\n", tmp, r[6]);
  r[5] = r[6] + r[5];        //        add $5, $6, $5
                             //        #xor $6, $10, $5
  r[11] = r[2] + r[5];       //        add $11, $2, $5
  r[8] = r[11] & ADDR_MASK;  //        and $8, $11, 0x07fc  # maks, 0000, 0111, 1111, 1100
  r[5] = r[10] + r[8];       //        add $5, $10, $8
  tmp = r[5] - SWATT_BASE;   //        lw $6, 0($5)
  mms[tmp/4] = 1;

  r[6] = buf[tmp/4];
  r[5] = r[6] + r[5];        //        add $5, $6, $5
                             //        #xor $6, $9, $5
  r[12] = r[2] + r[5];       //        add $12, $2, $5
  r[13]= r[11] ^ r[12];      //        xor $13, $11, $12      # should keep this, for debugging
  r[8] = r[13] & ADDR_MASK;  //        and $8, $13, 0x07fc
  r[5] = r[13] & JUMP_MASK;  //        and $5, $13, 0x1c0    # mask, 1,1100,0000
  r[9] = r[9] ^ r[5];        //        xor $9, $9, $5
  DEBUG   printf("block 1, r13 is %#x\n", r[13]);

  icount++;
  if (icount >= I_NUM)
  {
    icount = I_NUM + 1;
    goto __checksum_end;
  }

  r[10] = 0x8c00 + 32*4 + 8 - 8;
  goto __jump_table;
                             //        #jalr $10, $9
  //goto __checksum_end;       //        j __checksum_end

  
  // block 2
 __block2:                   //                        # block 2, address: 0x8c80
  r[5] = r[10] + r[7];       //        add $5, $10, $7 
  tmp = r[5] - SWATT_BASE;   //        lw $6, 0($5)
  mms[tmp/4] = 1; 

  r[6] = buf[tmp/4];
  DEBUG   printf("block 2 offset is %d, buf value %#x\n", tmp, r[6]);
  r[5] = r[6] ^ r[5];        //        xor $5, $6, $5
                             //        #add $6, $9, $5
  // r[14] = r[31] ^ r[5];      //        xor $14, $31, $5
  r[14] = r[11]^r[5];
  r[7] = r[14] & ADDR_MASK;  //        and $7, $14, 0x07fc  # maks, 0000, 0111, 1111, 1100
  r[5] = r[9] + r[7];        //        add $5, $9, $7
  tmp = r[5] - SWATT_BASE;   //        lw $6, 0($5)
  mms[tmp/4] = 1;

  r[6] = buf[tmp/4];
  r[5] = r[6] ^ r[5];        //        xor $5, $6, $5
                             //        #add $6, $10, $5
  r[15] = r[2] ^ r[5];       //        xor $15, $2, $5
  r[16] = r[3] + r[15];      //        add $16, $3, $15      # should keep this, for debugging
  r[7] = r[16] & ADDR_MASK;  //        and $7, $16, 0x07fc
  r[5] = r[16] & JUMP_MASK;  //        and $5, $16, 0x1c0    # mask, 1,1100,0000
  r[9] = r[9] ^ r[5];        //        xor $9, $9, $5
  DEBUG   printf("block 2, r16 is %#x\n", r[16]);


  icount++;
  if (icount >= I_NUM)
  {
    icount = I_NUM + 1;
    goto __checksum_end;
  }
  r[10] = 0x8c00 + 16*4*3 + 8 - 8;

                             //        #jalr $10, $9
  //goto __checksum_end;       //        j __checksum_end
    goto __jump_table;

  // block3
 __block3: 
  r[5] = r[9] + r[8];        //        add $5, $9, $8 
  tmp = r[5] - SWATT_BASE;   //        lw $6, 0($5)
  mms[tmp/4] = 1;

   DEBUG  printf("block 3 offset is %d, buf value %#x\n", tmp, r[6]);
  r[6] = buf[tmp/4];
  r[5] = r[6] + r[5];        //        add $5, $6, $5
  r[17] = r[11] + r[5];      //        add $17, $11, $5
  r[8] = r[17] & ADDR_MASK;  //        and $8, $17, 0x07fc
  r[5] = r[10] + r[8];       //        add $5, $10, $8
  tmp = r[5] - SWATT_BASE;   //        lw $6, 0($5)
  mms[tmp/4] = 1;

  r[6] = buf[tmp/4];
  r[5] = r[5] + r[6];        //        add $5, $6, $5
  r[18] = r[12] + r[5];      //        add $18, $12, $5
  r[19] = r[13] ^ r[18];     //        xor $19, $13, $18      # should keep this, for debugging
  r[8] = r[19] & ADDR_MASK;  //        and $8, $19, 0x07fc
  r[5] = r[19] & JUMP_MASK;  //        and $5, $19, 0x1c0   
  r[9] = r[9] ^ r[5];        //        xor $9, $9, $5
  DEBUG   printf("block 3, r19 is %#x\n", r[19]);

  icount++;
  if (icount >= I_NUM)
  {
    icount = I_NUM + 1;
    goto __checksum_end;
  }


  r[10] = 0x8c00 + 16*4*4 + 8 - 8;

  //        #jalr $10, $9
  //  goto __checksum_end;       //        j __checksum_end
      goto __jump_table; 

  // block 4                
 __block4: 
  r[5] = r[10] + r[7];      //        add $5, $10, $7 
  tmp = r[5] - SWATT_BASE;   //        lw $6, 0($5)
  mms[tmp/4] = 1;

  r[6] = buf[(r[5] - SWATT_BASE)/4];//  lw $6, 0($5)

  DEBUG   printf("block 4 offset is %d, buf value %#x\n", tmp, r[6]);
  r[5] = r[6] ^ r[5];       //        xor $5, $6, $5
  r[20] = r[14] ^ r[5];     //        xor $20, $14, $5
  r[7] = r[20] & ADDR_MASK; //        and $7, $20, 0x07fc  
  r[5] = r[9] + r[7];       //        add $5, $9, $7

  tmp = r[5] - SWATT_BASE;   //        lw $6, 0($5)
  mms[tmp/4] = 1;

  r[6] = buf[tmp/4];//lw $6, 0($5)
  r[5] = r[6] ^ r[5];       //        xor $5, $6, $5
  r[21] = r[15] ^ r[5];     //        xor $21, $15, $5
  r[22] = r[26] + r[21];    //        add $22, $26, $21  
  r[7] = r[22] & ADDR_MASK; //        and $7, $22, 0x07fc
  r[5] = r[22] & JUMP_MASK; //        and $5, $22, 0x1c0  
  r[9] = r[9] ^ r[5];       //        xor $9, $9, $5
  DEBUG   printf("block 4, r22 is %#x\n", r[22]);

  icount++;
  if (icount >= I_NUM)
  {
    icount = I_NUM + 1;
    goto __checksum_end;
  }

  r[10] = 0x8c00 + 16*4*5 + 8 - 8;
  //        #jalr $10, $9
  //  goto __checksum_end;      //        j __checksum_end
   goto __jump_table;
              
  //block5
 __block5:
  r[5] = r[9] + r[8];         //      add $5, $9, $8
  tmp =  r[5] - SWATT_BASE;
  mms[tmp/4] = 1; 

  r[6] = buf[(r[5] - SWATT_BASE)/4];//lw $6, 0($5)
  r[5] = r[6] + r[5];       //        add $5, $6, $5
  DEBUG   printf("block 5 offset is %d, buf value %#x\n", tmp, r[6]);
  r[23] = r[17] + r[5];     //        add $23, $17, $5
  r[8] = r[23] & ADDR_MASK;  //        and $8, $23, 0x07fc 
  r[5] = r[10] + r[8];      //        add $5, $10, $8
  tmp = r[5] - SWATT_BASE;   //        lw $6, 0($5)
  mms[tmp/4] = 1;


  r[6] = buf[tmp/4];//lw $6, 0($5)
  r[5] = r[6] + r[5];       //        add $5, $6, $5
  //        #xor $6, $9, $5
  r[24] = r[18] + r[5];     //        add $24, $18, $5
  r[25] = r[19] ^ r[24];    //        xor $25, $19, $24 
  r[8] = r[25] & ADDR_MASK; //        and $8, $25, 0x07fc
  r[5] = r[25] & JUMP_MASK; //        and $5, $25, 0x1c0 
  r[9] = r[9] ^ r[5];       //        xor $9, $9, $5
  DEBUG   printf("block 5, r25 is %#x\n", r[25]);


  icount++;
  if (icount >= I_NUM)
  {
    icount = I_NUM + 1;
    goto __checksum_end;
  }
  r[10] = 0x8c00 + 16*4*6 + 8 - 8;
  //        #jalr $10, $9
  //goto __checksum_end;      //        j __checksum_end
      goto __jump_table;


  //block6
 __block6:
  r[5] = r[10] + r[7];    //          add $5, $10, $7 
  tmp = r[5] - SWATT_BASE;
  mms[tmp/4] = 1;

  r[6] = buf[tmp/4];        //        lw $6, 0($5)
  r[5] = r[6] ^ r[5];       //        xor $5, $6, $5
  DEBUG   printf("block 6 offset is %d, buf value %#x, r7 is %#x\n", tmp, r[6], r[7]);
  r[26] = r[20] ^ r[5];     //        xor $26, $20, $5
  r[7] = r[26] & ADDR_MASK; //        and $7, $26, 0x07fc 
  r[5] = r[9] + r[7];       //        add $5, $9, $7
  tmp = r[5] - SWATT_BASE;  //        lw $6, 0($5)
  mms[tmp/4] = 1;


  r[6] = buf[tmp/4];//lw $6, 0($5)
  r[5] = r[6] ^ r[5];       //        xor $5, $6, $5
  r[27] = r[21] ^ r[5];     //        xor $27, $21, $5
  r[28] = r[22] + r[27];    //        add $28, $22, $27 
  r[7] = r[28] & ADDR_MASK; //        and $7, $28, 0x07fc
  r[5] = r[28] & JUMP_MASK; //        and $5, $28, 0x1c0
  r[9] = r[9] ^ r[5];       //        xor $9, $9, $5
  DEBUG   printf("block 6, r28 is %#x, r5 is %#x\n", r[28], r[5]);

  icount++;
  if (icount >= I_NUM)
  {
    icount = I_NUM + 1;
    goto __checksum_end;
  }
  r[10] = 0x8c00 + 16*4*7 + 8 - 8;
  //        #jalr $10, $9
  //goto __checksum_end;      //        j __checksum_end
  goto __jump_table;

  //block7
 __block7:
  r[5] = r[9] + r[8];      //          add $5, $9, $8
  tmp = r[5] - SWATT_BASE;
  mms[tmp/4] = 1; 

  r[6] = buf[tmp/4];//        lw $6, 0($5)
  DEBUG   printf("block 7 offset is %d, buf value %#x\n", tmp, r[6], r[6]);
  r[5] = r[6] + r[5];        //        add $5, $6, $5
  r[2] = r[23] + r[5];       //        add $2, $23, $5
  r[8] = r[2] & ADDR_MASK;   //        and $8, $2, 0x07fc 
  r[5] = r[10] + r[8];       //        add $5, $10, $8
  tmp = r[5] - SWATT_BASE;   //        lw $6, 0($5)
  mms[tmp/4] = 1;

  r[6] = buf[tmp/4]; //lw $6, 0($5)
  r[5] = r[6] + r[5];        //        add $5, $6, $5
  r[3] = r[24] + r[5];       //        add $3, $24, $5
  r[11] = r[25] ^ r[3];      //        xor $11, $25, $3  
  r[8] = r[11] & ADDR_MASK;  //        and $8, $11, 0x07fc
  r[5] = r[11] & JUMP_MASK;  //        and $5, $11, 0x1c0  
  r[9] = r[9] ^ r[5];        //        xor $9, $9, $5
  DEBUG   printf("block 7, r11 is %#x\n", r[11]);

  r[10] = 0x8c00 + 16*4*8 + 8 - 8;

  icount++;
  if (icount == I_NUM)
  {
    goto __checksum_end;
  }

  //        #jalr $10, $9
  goto __checksum_end;//       j __checksum_end


 __jump_table: 
  DEBUG   printf("\njump table: r9 is %#x\n\n", r[9]);
    if (r[9] == 0x8c00)
       goto __block0;

    if (r[9] == 0x8c40)
       goto __block1;

    if (r[9] == 0x8c80)
       goto __block2;

    if (r[9] == 0x8cc0)
       goto __block3;

    if (r[9] == 0x8d00)
       goto __block4;

    if (r[9] == 0x8d40)
       goto __block5;

    if (r[9] == 0x8d80)
       goto __block6;

    if (r[9] == 0x8dc0)
       goto __block7;


   DEBUG    printf("cannot find jump target\n");
   DEBUG    printf("address to jump is %#x\n", r[9]);
    goto __return;

 __checksum_end:

    // obtain one
    if (icount == I_NUM)
    {  
      inew =  mms_updated();
      ileft = mms_left();    
  
      if (inonce >= NONCE_FULL)
      {    
        //printf("inonce = %d, inew = %d, ileft = %d\n", inonce, inew, ileft);        
        if (inew >= 1)
	{

          copy_mms2mm();
          save_regs();
          nonce_buf[inonce++] = nonce;
          DEBUG_OUTPUT_NONCE printf("nonce[%d] = %#x\n", (inonce -1), nonce);
          if (ileft == 0)
	  {
	    printf("Nonce is full\n");
              goto __return;
          }
        }
        else
          restore_regs();
        copy_mm2mms();
      }
      else
      {
	  copy_mms2mm();
          save_regs();
          nonce_buf[inonce++] = nonce;
      }
    }
    else
    {
      copy_mm2mms();
      restore_regs();  
    }

    icount = 0;

    count++;
  DEBUG     printf("\n checksum_end: count %d, icount %d, r9 is %#x\n", count, icount, r[9]);
  if ((inonce >= NONCE_COUNT) || (count >= LOOP_COUNT))
    {
      printf("%d nonces are generated...\n", inonce);
      goto __return;
    }     

    // update nonce
    u8_nonce_buf[0] = random_byte();
    u8_nonce_buf[1] = random_byte();
    u8_nonce_buf[2] = random_byte();
    u8_nonce_buf[3] = random_byte();
    nonce = *((__u32*)u8_nonce_buf);
  DEBUG printf("new nonce:\n");
  DEBUG printf("nonce: %#x, %#x, %#x, %#x\n", u8_nonce_buf[0], u8_nonce_buf[1], u8_nonce_buf[2], u8_nonce_buf[3]);
   DEBUG printf("nonce32 bit: %#x \n", nonce);
    // update r7 and r8
    tmp = nonce;
    r[7] = (tmp>>16) & ADDR_MASK;
    r[8] = nonce & ADDR_MASK;
   DEBUG    printf("update r7 and r8: %#x, %#x\n", r[7], r[8]);

    // update r9
    tmp = nonce;
    tmp = tmp & JUMP_MASK;
    r[9] = r[9] ^ tmp;

    goto __jump_table;

 __return:
    g_nonce_count = inonce;
    
    //printf("the number of generated nonces is %d\n", inonce);
    //for (i = 0; i < inonce; i++)
    //  printf("nonce[%d] = %#x\n", i, nonce_buf[i]);

  return;
}

void checksum_simulator(void)
{
  __u32 *r = g_reg;
  __u32 *buf = g_swatt_buf;
  __u32 *buf_s = g_swatt_buf_s;
  __u32 tmp = 0;
  __u32 *nonce_buf = g_nonce_pool;
  __u32 nonce = RANDOMNESS;
  __u8 u8_nonce_buf[4];
  __u32 count = 0;
  __u32 icount = 0;
  __u32 inonce = 0;
  __u32 index = 0;
  __u32 i = 0;
  __u32 istart = 0;
  __u32 iend = 0;
  __u32 sum = 0;

  DEBUG_CHECKSUM_SIMULATOR printf("\nbegin checksum computation\n");
  // initialization
  // initialize r2, r3, r11 to r28
  r[2] = 0x2023;
  r[3] = 0x2084;
  r[11] = 0x33f0;
  r[12] = 0x33f1;
  r[13] = 0x33f2;
  r[14] =  0x33f3;
  r[15] =  0x33f4;
  r[16] =  0x33f5;
  r[17] =  0x33f6;
  r[18] =  0x33f7;
  r[19] =  0x33f8;
  r[20] =  0x33f9;
  r[21] =  0x33fa;
  r[22] =  0x33fb;
  r[23] =  0x33fc;
  r[24] =  0x33fd;
  r[25] = 0x33fe;
  r[26] = 0x33ff;
  r[27] = 0x33f0;
  r[28] = 0x33f1;    

  // initialize r7 and r8
  r[7] = 0x33f2;
  r[8] = 0x33f3;
  r[7] = r[7] & ADDR_MASK;
  r[8] = r[8] & ADDR_MASK;

  //initialize r9, r9
  r[9] = 0x8c00;  // base address
  r[10] = 0x8c00; 
  r[31] = 0xbffe4c;   


  nonce = nonce_buf[inonce++];

  // update r7 and r8
  tmp = nonce;
  r[7] = (tmp>>16) & ADDR_MASK;
  r[8] = nonce & ADDR_MASK;

  DEBUG_CHECKSUM_SIMULATOR printf("initialization: r7=%#x, r8=%#x\n", r[7], r[8]);

  tmp = nonce;
  tmp = tmp & JUMP_MASK;
  DEBUG_CHECKSUM_SIMULATOR printf("tmp=%#x\n", tmp);
  r[9] = r[9] ^ tmp;
  
  goto __jump_table; 
  
 __block0:
  r[5] = r[10] + r[7];    //      add $5, $10, $7 
  tmp = r[5] - SWATT_BASE;
  buf_s[tmp/4] = 1;

  r[6] = buf[tmp/4];    //        lw $6, 0($5)
  DEBUG_CHECKSUM_SIMULATOR   printf("block 0 addr %#x, offset is %d, buf value %#x, r[7] is %#x\n", r[5], tmp, r[6], r[7]);
  DEBUG_CHECKSUM_SIMULATOR printf("other buf: buf[n-1]=%#x, buf[n+1]=%#x\n", buf[(tmp/4) - 1], buf[(tmp/4) + 1]);
  r[5] = r[6] ^ r[5];   //        xor $5, $6, $5
 
  r[11] = r[26] ^ r[5]; //        xor $11, $26, $5
  //  r[7] = r[31] & ADDR_MASK;   //  and $7, $31, 0x07fc  
  r[7] = r[11] & ADDR_MASK;
  r[5] = r[9] + r[7];   //        add $5, $9, $7

  tmp = r[5] - SWATT_BASE; 
  buf_s[tmp/4] = 1;

  r[6] = buf[tmp/4];    //        lw $6, 0($5)
  r[5] = r[6] ^ r[5];   //        xor $5, $6, $5
  r[2] = r[27] ^ r[5];  //        xor $2, $27, $5
  r[3] = r[2] + r[2];   //        add $3, $2, $2   
  r[7] = r[3] & ADDR_MASK;//        and $7, $3, 0x07fc
  r[5] = r[3] & JUMP_MASK;//        and $5, $3, 0x1c0    # mask, 1,1100,0000
  r[9] = r[9] ^ r[5];   //        xor $9, $9, $5
  DEBUG_CHECKSUM_SIMULATOR   printf("block 1, r3 is %#x\n", r[3]);

  icount++;
  if (icount >= I_NUM)
  {
    icount = I_NUM + 1;
    goto __checksum_end;
   }

  r[10] = 0x8c00 + 16*4 + 8 - 8;
  goto __jump_table;
  //goto __checksum_end;
  
  //block 1
 __block1:
  r[5] = r[9] + r[8];        //        add $5, $9, $8 
  tmp = r[5] - SWATT_BASE;
  buf_s[tmp/4] = 1;
  r[6] = buf[tmp/4];         //        lw $6, 0($5)
  DEBUG_CHECKSUM_SIMULATOR   printf("block 1 offset is %d, buf value %#x\n", tmp, r[6]);
  r[5] = r[6] + r[5];        //        add $5, $6, $5
                             //        #xor $6, $10, $5
  r[11] = r[2] + r[5];       //        add $11, $2, $5
  r[8] = r[11] & ADDR_MASK;  //        and $8, $11, 0x07fc  # maks, 0000, 0111, 1111, 1100
  r[5] = r[10] + r[8];       //        add $5, $10, $8
  tmp = r[5] - SWATT_BASE;   //        lw $6, 0($5)

  buf_s[tmp/4] = 1;

  r[6] = buf[tmp/4];
  r[5] = r[6] + r[5];        //        add $5, $6, $5
                             //        #xor $6, $9, $5
  r[12] = r[2] + r[5];       //        add $12, $2, $5
  r[13]= r[11] ^ r[12];      //        xor $13, $11, $12      # should keep this, for debugging
  r[8] = r[13] & ADDR_MASK;  //        and $8, $13, 0x07fc
  r[5] = r[13] & JUMP_MASK;  //        and $5, $13, 0x1c0    # mask, 1,1100,0000
  r[9] = r[9] ^ r[5];        //        xor $9, $9, $5
  DEBUG_CHECKSUM_SIMULATOR   printf("block 1, r13 is %#x\n", r[13]);

  icount++;
  if (icount >= I_NUM)
  {
    icount = I_NUM + 1;
    goto __checksum_end;
  }

  r[10] = 0x8c00 + 32*4 + 8 - 8;
  goto __jump_table;
                             //        #jalr $10, $9
  //goto __checksum_end;       //        j __checksum_end

  
  // block 2
 __block2:                   //                        # block 2, address: 0x8c80
  r[5] = r[10] + r[7];       //        add $5, $10, $7 
  tmp = r[5] - SWATT_BASE;   //        lw $6, 0($5)
  buf_s[tmp/4] = 1;
  r[6] = buf[tmp/4];
  DEBUG_CHECKSUM_SIMULATOR   printf("block 2 offset is %d, buf value %#x\n", tmp, r[6]);
  r[5] = r[6] ^ r[5];        //        xor $5, $6, $5
                             //        #add $6, $9, $5
  //  r[14] = r[31] ^ r[5];      //        xor $14, $31, $5
  r[14] = r[11] ^ r[5];
  r[7] = r[14] & ADDR_MASK;  //        and $7, $14, 0x07fc  # maks, 0000, 0111, 1111, 1100
  r[5] = r[9] + r[7];        //        add $5, $9, $7
  tmp = r[5] - SWATT_BASE;   //        lw $6, 0($5)

  buf_s[tmp/4] = 1;

  r[6] = buf[tmp/4];
  r[5] = r[6] ^ r[5];        //        xor $5, $6, $5
                             //        #add $6, $10, $5
  r[15] = r[2] ^ r[5];       //        xor $15, $2, $5
  r[16] = r[3] + r[15];      //        add $16, $3, $15      # should keep this, for debugging
  r[7] = r[16] & ADDR_MASK;  //        and $7, $16, 0x07fc
  r[5] = r[16] & JUMP_MASK;  //        and $5, $16, 0x1c0    # mask, 1,1100,0000
  r[9] = r[9] ^ r[5];        //        xor $9, $9, $5
  DEBUG_CHECKSUM_SIMULATOR  printf("block 2, r16 is %#x\n", r[16]);


  icount++;
  if (icount >= I_NUM)
  {
    icount = I_NUM + 1;
    goto __checksum_end;
  }
  r[10] = 0x8c00 + 16*4*3 + 8 - 8;

                             //        #jalr $10, $9
  //goto __checksum_end;       //        j __checksum_end
    goto __jump_table;

  // block3
 __block3: 
  r[5] = r[9] + r[8];        //        add $5, $9, $8 
  tmp = r[5] - SWATT_BASE;   //        lw $6, 0($5)
  buf_s[tmp/4] = 1;
  DEBUG_CHECKSUM_SIMULATOR  printf("block 3 offset is %d, buf value %#x\n", tmp, r[6]);
  r[6] = buf[tmp/4];
  r[5] = r[6] + r[5];        //        add $5, $6, $5
  r[17] = r[11] + r[5];      //        add $17, $11, $5
  r[8] = r[17] & ADDR_MASK;  //        and $8, $17, 0x07fc
  r[5] = r[10] + r[8];       //        add $5, $10, $8
  tmp = r[5] - SWATT_BASE;   //        lw $6, 0($5)

  buf_s[tmp/4] = 1;

  r[6] = buf[tmp/4];
  r[5] = r[5] + r[6];        //        add $5, $6, $5
  r[18] = r[12] + r[5];      //        add $18, $12, $5
  r[19] = r[13] ^ r[18];     //        xor $19, $13, $18      # should keep this, for debugging
  r[8] = r[19] & ADDR_MASK;  //        and $8, $19, 0x07fc
  r[5] = r[19] & JUMP_MASK;  //        and $5, $19, 0x1c0   
  r[9] = r[9] ^ r[5];        //        xor $9, $9, $5
  DEBUG_CHECKSUM_SIMULATOR   printf("block 3, r19 is %#x\n", r[19]);

  icount++;
  if (icount >= I_NUM)
  {
    icount = I_NUM + 1;
    goto __checksum_end;
  }


  r[10] = 0x8c00 + 16*4*4 + 8 - 8;

  //        #jalr $10, $9
  //  goto __checksum_end;       //        j __checksum_end
      goto __jump_table; 

  // block 4                
 __block4: 
  r[5] = r[10] + r[7];      //        add $5, $10, $7 
  tmp = r[5] - SWATT_BASE;
  buf_s[tmp/4] = 1;
  r[6] = buf[tmp/4];//  lw $6, 0($5)
  DEBUG_CHECKSUM_SIMULATOR   printf("block 4 addr is %#x, buf value %#x\n", r[5], r[6]);
  r[5] = r[6] ^ r[5];       //        xor $5, $6, $5
  r[20] = r[14] ^ r[5];     //        xor $20, $14, $5
  DEBUG_CHECKSUM_SIMULATOR printf("block 4, r[20] is %#x\n", r[20]);
  r[7] = r[20] & ADDR_MASK; //        and $7, $20, 0x07fc  
  r[5] = r[9] + r[7];       //        add $5, $9, $7
  tmp = r[5] - SWATT_BASE; 
  buf_s[tmp/4] = 1;

  r[6] = buf[tmp/4];//lw $6, 0($5)
  r[5] = r[6] ^ r[5];       //        xor $5, $6, $5
  r[21] = r[15] ^ r[5];     //        xor $21, $15, $5
  r[22] = r[26] + r[21];    //        add $22, $26, $21  
  r[7] = r[22] & ADDR_MASK; //        and $7, $22, 0x07fc
  r[5] = r[22] & JUMP_MASK; //        and $5, $22, 0x1c0  
   
  r[9] = r[9] ^ r[5];       //        xor $9, $9, $5
  DEBUG_CHECKSUM_SIMULATOR   printf("block 4, r21 = %#x, r22 is %#x\n", r[21], r[22]);

  icount++;
  if (icount >= I_NUM)
  {
    icount = I_NUM + 1;
    goto __checksum_end;
  }

  r[10] = 0x8c00 + 16*4*5 + 8 - 8;
  //        #jalr $10, $9
  //  goto __checksum_end;      //        j __checksum_end
   goto __jump_table;
              
  //block5
 __block5:
  r[5] = r[9] + r[8];         //      add $5, $9, $8
  tmp = r[5] - SWATT_BASE; 
  buf_s[tmp/4] = 1;
  r[6] = buf[tmp/4];//lw $6, 0($5)
  r[5] = r[6] + r[5];       //        add $5, $6, $5
  DEBUG_CHECKSUM_SIMULATOR   printf("block 5 offset is %d, buf value %#x\n", tmp, r[6]);
  r[23] = r[17] + r[5];     //        add $23, $17, $5
  r[8] = r[23] & ADDR_MASK;  //        and $8, $23, 0x07fc 
  r[5] = r[10] + r[8];      //        add $5, $10, $8
  tmp = r[5] - SWATT_BASE; 
  buf_s[tmp/4] = 1;

  r[6] = buf[tmp/4];//lw $6, 0($5)
  r[5] = r[6] + r[5];       //        add $5, $6, $5
  //        #xor $6, $9, $5
  r[24] = r[18] + r[5];     //        add $24, $18, $5
  r[25] = r[19] ^ r[24];    //        xor $25, $19, $24 
  r[8] = r[25] & ADDR_MASK; //        and $8, $25, 0x07fc
  r[5] = r[25] & JUMP_MASK; //        and $5, $25, 0x1c0 
  r[9] = r[9] ^ r[5];       //        xor $9, $9, $5
  DEBUG_CHECKSUM_SIMULATOR   printf("block 5, r25 is %#x\n", r[25]);


  icount++;
  if (icount >= I_NUM)
  {
    icount = I_NUM + 1;
    goto __checksum_end;
  }
  r[10] = 0x8c00 + 16*4*6 + 8 - 8;
  //        #jalr $10, $9
  //goto __checksum_end;      //        j __checksum_end
      goto __jump_table;


  //block6
 __block6:
  r[5] = r[10] + r[7];    //          add $5, $10, $7 
  tmp = r[5] - SWATT_BASE;
  DEBUG_CHECKSUM_SIMULATOR printf("block 6, addr is %#x\n", r[5]);
  buf_s[tmp/4] = 1;
  r[6] = buf[tmp/4];//lw $6, 0($5)
  r[5] = r[6] ^ r[5];       //        xor $5, $6, $5
  DEBUG_CHECKSUM_SIMULATOR   printf("block 6, r10=%#x, r9=%#x\n", r[10], r[9]);
  DEBUG_CHECKSUM_SIMULATOR   printf("block 6 offset is %d, buf value %#x, r7 is %#x\n", tmp, r[6], r[7]);
  r[26] = r[20] ^ r[5];     //        xor $26, $20, $5
  r[7] = r[26] & ADDR_MASK; //        and $7, $26, 0x07fc 
  r[5] = r[9] + r[7];       //        add $5, $9, $7
  tmp = r[5] - SWATT_BASE; 
  buf_s[tmp/4] = 1;

  r[6] = buf[tmp/4];//lw $6, 0($5)
  r[5] = r[6] ^ r[5];       //        xor $5, $6, $5
  r[27] = r[21] ^ r[5];     //        xor $27, $21, $5
  r[28] = r[22] + r[27];    //        add $28, $22, $27 
  r[7] = r[28] & ADDR_MASK; //        and $7, $28, 0x07fc
  r[5] = r[28] & JUMP_MASK; //        and $5, $28, 0x1c0

  DEBUG_CHECKSUM_SIMULATOR printf("block 6, r9 is %#x, ", r[9]);
  r[9] = r[9] ^ r[5];       //        xor $9, $9, $5
  
  DEBUG_CHECKSUM_SIMULATOR   printf("xor, r9 is %#x\n block 6, r28 is %#x, r5 is %#x\n", r[9], r[28], r[5]);

  icount++;
  if (icount >= I_NUM)
  {
    icount = I_NUM + 1;
    goto __checksum_end;
  }
  r[10] = 0x8c00 + 16*4*7 + 8 - 8;
  //        #jalr $10, $9
  //goto __checksum_end;      //        j __checksum_end
  goto __jump_table;

  //block7
 __block7:
  r[5] = r[9] + r[8];      //          add $5, $9, $8
  tmp = r[5] - SWATT_BASE; 
  buf_s[tmp/4] = 1;
  r[6] = buf[tmp/4];//        lw $6, 0($5)
 DEBUG_CHECKSUM_SIMULATOR   printf("block 7 offset is %d, buf value %#x\n", tmp, r[6], r[6]);
  r[5] = r[6] + r[5];        //        add $5, $6, $5
  r[2] = r[23] + r[5];       //        add $2, $23, $5
  r[8] = r[2] & ADDR_MASK;   //        and $8, $2, 0x07fc 
  r[5] = r[10] + r[8];       //        add $5, $10, $8

  tmp = r[5] - SWATT_BASE; 
  buf_s[tmp/4] = 1;

  r[6] = buf[tmp/4]; //lw $6, 0($5)
  r[5] = r[6] + r[5];        //        add $5, $6, $5
  r[3] = r[24] + r[5];       //        add $3, $24, $5
  r[11] = r[25] ^ r[3];      //        xor $11, $25, $3  
  r[8] = r[11] & ADDR_MASK;  //        and $8, $11, 0x07fc
  r[5] = r[11] & JUMP_MASK;  //        and $5, $11, 0x1c0  
  r[9] = r[9] ^ r[5];        //        xor $9, $9, $5
  DEBUG_CHECKSUM_SIMULATOR   printf("block 7, r11 is %#x\n", r[11]);

  r[10] = 0x8c00 + 16*4*8 + 8 - 8;

  icount++;
  if (icount == I_NUM)
  {
    goto __checksum_end;
  }

  //        #jalr $10, $9
  goto __checksum_end;//       j __checksum_end


 __jump_table: 
  DEBUG_CHECKSUM_SIMULATOR  printf("\njump table: r9 is %#x\n\n", r[9]);
    if (r[9] == 0x8c00)
       goto __block0;

    if (r[9] == 0x8c40)
       goto __block1;

    if (r[9] == 0x8c80)
       goto __block2;

    if (r[9] == 0x8cc0)
       goto __block3;

    if (r[9] == 0x8d00)
       goto __block4;

    if (r[9] == 0x8d40)
       goto __block5;

    if (r[9] == 0x8d80)
       goto __block6;

    if (r[9] == 0x8dc0)
       goto __block7;


   DEBUG_CHECKSUM_SIMULATOR    printf("cannot find jump target\n");
   DEBUG_CHECKSUM_SIMULATOR    printf("address to jump is %#x\n", r[9]);
    goto __return;

 __checksum_end:

    // obtain one
    if (icount != I_NUM)
    {
      printf("icounter is wrong\n");
      printf("icounter is %d\n", icount);
      goto __return;
      //restore_regs();  
    }

    icount = 0;

    count++;
    DEBUG_CHECKSUM_SIMULATOR     printf("\n checksum_end: count %d, icount %d, r9 is %#x\n", count, icount, r[9]);
    if (inonce >= g_nonce_count)
    {
      DEBUG_CHECKSUM_SIMULATOR printf("\nall nonce are used.\n");
      goto __return;
    }     

    // update nonce
     nonce = nonce_buf[inonce++];
     
     DEBUG printf("new nonce:\n");
     DEBUG_CHECKSUM_SIMULATOR printf("nonce: %#x, %#x, %#x, %#x\n", u8_nonce_buf[0], u8_nonce_buf[1], u8_nonce_buf[2], u8_nonce_buf[3]);
     DEBUG_CHECKSUM_SIMULATOR printf("nonce32 bit: %#x \n", nonce);
    // update r7 and r8
    tmp = nonce;
    r[7] = (tmp>>16) & ADDR_MASK;
    r[8] = nonce & ADDR_MASK;
    DEBUG_CHECKSUM_SIMULATOR   printf("update r7 and r8: %#x, %#x\n", r[7], r[8]);

    // update r9
    tmp = nonce;
    tmp = tmp & JUMP_MASK;
    r[9] = r[9] ^ tmp;
    DEBUG_CHECKSUM_SIMULATOR printf("update r9, %#x\n", r[9]);

    tmp = nonce;
    tmp = nonce & 0x1f;
    DEBUG_CHECKSUM_SIMULATOR printf("return index %d\n", tmp);       
    //tmp = 0x2023 & 0x1f;
    return_checksum(tmp, inonce);

    if (inonce >= SWATT_LOOP_NUM)
    {
      printf("\nreach swatt loop num(%d)! exit\n",inonce);
      goto __return;
    }

    goto __jump_table;

 __return:
    DEBUG_CHECKSUM_SIMULATOR printf("checksum computation return\n");
    DEBUG_CHECKSUM_SIMULATOR printf("count: %d\n", count);

    istart = (0x8c00 - SWATT_BASE)/4;
    iend = (0x8c00 + 16*8*4 + 8 - SWATT_BASE + 2048)/4;
    DEBUG_CHECKSUM_SIMULATOR printf("start %d, end %d\n", istart, iend);
    sum = 0;
    for (i = istart; i < iend; i++)
      sum += buf_s[i];
    DEBUG_CHECKSUM_SIMULATOR printf("the total mem num is %d, sum is %d\n", (iend - istart), sum);

    sum = 0;
    for (i = 0; i < FW_SIZE/4; i++)
      sum += buf_s[i];

    DEBUG_CHECKSUM_SIMULATOR printf("verification: sum is %d\n", sum);

    DEBUG_OUTPUT_ZERO_BUF
   {
       for (i = istart; i < iend; i++)
       { 
          if (buf_s[i] == 0)
            printf("buf[%d]=%d\n", i, buf_s[i]);
       }
   }

    for (i = 0; i < FW_SIZE/4; i++)
      buf_s[i] = 0;

  return;
}

static __inline__ int
swatt (alteon_st *nic, __u32 *dpr, __u32 *dp, __u32 cmd, __u32 offset, __u32 exp_num)
{
    nic->req.cardoffset = offset;
    nic->req.data = exp_num;
    if (ioctl(nic->socket, cmd, &nic->ifr))
    {
        printf("ioctl fail\n");
	*dpr = other_endian(nic->req.data);
	return FALSE;
    }
    *dpr = other_endian(nic->req.data);
    *dp = other_endian(nic->req.cardoffset);
    return TRUE;
}


int 
main (int argc, char **argv)
{
    __u32 ra, ra_b, rb, rb_b, entry[8];
    __u32 cmd, i;
    __u32 offset, tptr, tstart, tlen, p;
    __u32 pc, state, pc_b, state_b, o_state, o_state_b, o_frame_base;
    __u32 swatt_a, swatt_b;

#if 1
    /* argument */
    
    if (argc != 3) die ("Usage: %s <i/f name>");
    if (strlen(argv[1]) >= IFNAMSIZ) die ("i/f name too long");
    if (geteuid() != 0) die ("must be root to access shared memory");
    
    memset (&st, 0, sizeof (st));
    
    printf("Creat a socket with the NIC to verify\n");
    /* Open a socket for our ioctls */
    st.socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    
    /* Set up basic ioctl structure */
    st.ifr.ifr_data = (char *)&st.req;
    strcpy(st.ifr.ifr_name, argv[optind]);

    //printf("First, dump firmware out\n");
    /* Stop both CPUs */
    if( read_smem(&st, 0x140, &o_state) == FALSE )
      exit(-1);

    o_state = ntohl( o_state );

    if( write_smem(&st, 0x140, ntohl(o_state | 0x10000) ) == FALSE )
      exit(-1);

    if( read_smem(&st, 0x240, &o_state_b) == FALSE )
      exit(-1);

    o_state_b = ntohl( o_state_b );

    if( write_smem(&st, 0x240, ntohl(o_state_b | 0x10000) ) == FALSE )
      exit(-1);

    /* Read out CPU states */
    read_smem(&st, 0x144, &pc);
    read_smem(&st, 0x244, &pc_b);

    pc = ntohl(pc);
    state = o_state;
    pc_b = ntohl(pc_b);
    state_b = o_state_b;
    // pause cpu end

    read_smem(&st, 0x1f4, &ra);         // $29 == $sp
    read_smem(&st, 0x2f4, &ra_b);
    ra = ntohl(ra);
    ra = ntohl(ra_b);

    read_smem(&st, 0x68, &o_frame_base);
    o_frame_base = htonl(o_frame_base);

    /* NIC is big-endian */
    tstart = SWATT_BASE;
    tlen = FW_SIZE; /*2k bytes*/
    //printf ("SWATT buffer is %i bytes at %#x.\n"
    //    "CPU A PC=0x%08x, state=0x%08x : CPU B PC=0x%08x, state=0x%08x\n"
    //    "      SP=0x%08x               :       SP=0x%08x\n", 
    //    tlen, tstart, 
    //    pc, state, pc_b, state_b,ra,ra_b); 

    //printf("begin to read swatt code from NIC\n");

#endif
    printf("initialization....\n");
    init_buf();
    //printf("dump firmware from NIC\n");
    dump_swatt_code(tstart, tlen);  
    write_smem (&st, 0x68, ntohl(o_frame_base));
    save_swatt_code();    
    read_swatt_code();
   
    printf("Generating nonce-checksum pairs...\n");
    generate_nonce();
    
    checksum_simulator();
    // printf("done\n");
    /* Restart both CPUs */
    write_smem(&st, 0x140, ntohl(o_state & ~0x10000 ) );
    write_smem(&st, 0x240, ntohl(o_state_b & ~0x10000 ) );


    printf("start attestation...\n...\n");

    st.ifr.ifr_data = (char *)&st.req;
    strcpy(st.ifr.ifr_name, argv[optind]);

#ifdef ATTESTATION_CPUB
    printf("\nAttestation for CPU B\n");
    cmd = ACENIC_IOCTL_SWATT_B;
    offset = 0x658;
    // copy nonce and checksum result to buf
    st.req.nonce_num = g_nonce_count;
    for (i = 0; i < st.req.nonce_num; i++)
    {   
      st.req.nonce[i]=  g_nonce_pool[i];
      st.req.checksum[i] = g_checksum[i][0];
    }

    if(swatt(&st, &ra_b, &rb_b, cmd, offset, (__u32)atoi(argv[2])) == FALSE)
    {
	printf("IOCTL fails.\n");
	return 0;
    }
    ra = ntohl(ra_b);
    rb = ntohl(rb_b);
    //printf("Attestation for CPU B is done!\n\n");

    if (ra == 1)
    { 
      swatt_b = st.req.nonce_num; 
      // print out the checksum results and timing results
       printf("The last 5 checksum results are:\n%#x, %#x, %#x, %#x, %#x\n", st.req.checksum[swatt_b - 6],st.req.checksum[swatt_b - 5],st.req.checksum[swatt_b - 4],st.req.checksum[swatt_b - 3],st.req.checksum[swatt_b - 2]);
       printf("Timing results for the last 5 nonce-checksum pairs are:\n%d, %d, %d, %d, %d (cpu cycles)\n", st.req.nonce[swatt_b - 6],st.req.nonce[swatt_b - 5],st.req.nonce[swatt_b - 4],st.req.nonce[swatt_b - 3],st.req.nonce[swatt_b - 2]);
      swatt_b = 1;
      printf("Attestation for CPU B succeeds!\nFirmware on CPU B is trusted\n");
    }
    else
    { 
      swatt_b = 0;
      printf("Attestation for CPU B fails!\nUse dmesg to look at detailed info!\n");
    close (st.socket);
    
    return 0;

    }

#endif

#ifdef ATTESTATION_CPUA
    printf("\nAttestation for CPU A\n");
    cmd = ACENIC_IOCTL_SWATT;
    offset = 0x658;
    // copy nonce and checksum result to buf
    st.req.nonce_num = g_nonce_count;
    for (i = 0; i < g_nonce_count; i++)
    {   
      st.req.nonce[i]=  g_nonce_pool[i];
      st.req.checksum[i] = g_checksum[i][0];
    }

    //printf("call ioctl to start attestation, cmd %#x\n", cmd);
    if(swatt(&st, &ra_b, &rb_b, cmd, offset, (__u32)atoi(argv[2])) == FALSE)
    {
	printf("IOCTL fails.\n");
	return 0;
    }
    ra = ntohl(ra_b);
    rb = ntohl(rb_b);

    if (ra == 1)
    {  
       swatt_a = st.req.nonce_num;  // attestation succeeds 
       //printf("Attestation for CPU A finishes !\n\n");
       // print out checksum results and timing results
       printf("The last 5 checksum results are:\n%#x, %#x, %#x, %#x, %#x\n", st.req.checksum[swatt_a-6],st.req.checksum[swatt_a-5],st.req.checksum[swatt_a-4],st.req.checksum[swatt_a-3],st.req.checksum[swatt_a-2]);
       printf("Timing results for the last 5 nonce-checksum pairs are:\n%d, %d, %d, %d, %d (cpu cycles)\n", st.req.nonce[swatt_a-6],st.req.nonce[swatt_a-5],st.req.nonce[swatt_a-4],st.req.nonce[swatt_a-3],st.req.nonce[swatt_a-2]);
       swatt_a = 1;
       printf("Attestation for CPU A succeeds!\n");
       printf("Waiting for sha1 results....\n");


       cmd = ACENIC_IOCTL_SWATT_READ_SHA1;
       offset = 0x658;
       if(swatt(&st, &ra_b, &rb_b, cmd, offset, (__u32)atoi(argv[2])) == FALSE)
       {
	printf("IOCTL fails.\n");
	return 0;
       }
       printf("sha1 hash results are: ");
       printf("%x%x%x%x%x\n", st.req.sha1[0], st.req.sha1[1],st.req.sha1[2],st.req.sha1[3],st.req.sha1[4]);
       printf("Finish\n");
    }
    else
    {  
      swatt_a = 0;
      printf("Attestation fails!\n Use dmesg to look at detailed info!\n");
    }

    // printf("\nPlease use 'dmesg' to check detailed attestation results, including checksums, attestation time, and hash results\n");
#endif

    close (st.socket);
    
    return 0;
}


/*
 *  EOF trace_dump.c
 */

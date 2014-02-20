/*
 *  acenic_ioctls.h
 *  ---------------
 *
 *  tjd 25.03.99
 *
 *  Definitions for using the SIOCDEVPRIVATE ioctls to debug the firmware in
 *  Alteon's Tigon II chipset Gigabit Ethernet cards.
 *
 */

#include <asm/types.h>

/* 
 * If we provide methods to read & write data to and from the card, 
 * we can do anything we want on top of that.
 */
#define NONCE_NUM 300
#define ACENIC_IOCTL_READ_SMEM  SIOCDEVPRIVATE
#define ACENIC_IOCTL_WRITE_SMEM (SIOCDEVPRIVATE + 1)
#define ACENIC_IOCTL_SWATT      (SIOCDEVPRIVATE + 2)
#define ACENIC_IOCTL_SWATT_READ_SHA1 (SIOCDEVPRIVATE + 3)
/* The structures we pass in as the arguments */
struct acenic_ioc_req {
    unsigned long int cardoffset; 
    /* Where on the card - must be a multiple of 4 and < 0x3FFc */
    __u32  data;  
    /* What to put there (read ioctl puts return val here) */
    __u32  nonce_num;
    __u32 nonce[NONCE_NUM];
    __u32  checksum[NONCE_NUM];
  __u32 sha1[5];
};
/*
struct acenic_ioc_buf {
  __u32 cmd;
  __u32 nonce_num;
  __u32 nonce[NONCE_NUM];
  __u32 checksum[NONCE_NUM];
};
*/
/*
 *  EOF
 */

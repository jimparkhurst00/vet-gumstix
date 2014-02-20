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
#include <sys/time.h>
#include "acenic_ioctls.h"

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


#define DEBUG if(0)

/* global variables*/

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
swatt (alteon_st *nic, __u32 *dpr)
{
//:wq
//printf("cmd = %d\n", ACENIC_IOCTL_SWATT);
    if (ioctl(nic->socket, ACENIC_IOCTL_SWATT, &nic->ifr))
    {
        printf("ioctl fail\n");
	*dpr = other_endian(nic->req.data);
	return FALSE;
    }
    *dpr = other_endian(nic->req.data);
    return TRUE;
}




int 
main (int argc, char **argv)
{
    __u32 ra,  rb, rb_b, entry[8];
    __u32 cmd, i;
    __u32 offset, tptr, tstart, tlen, p;
    __u32 pc, state, pc_b, state_b, o_state, o_state_b, o_frame_base;
    __u32 swatt_a, swatt_b;
    struct timeval tv, tvend;
    struct timezone tz, tzend;
    /* argument */    
    if (argc != 3) 
    {
       printf("Usage: <i/f name>\n");
       return;
    }

    if (strlen(argv[1]) >= IFNAMSIZ) 
    {
       printf("i/f name too long\n");
       return;
    }

    if (geteuid() != 0) 
    {
       printf("must be root to access shared memory\n");
       return;
    }

    memset (&st, 0, sizeof (st));
    
    //printf("Creat a socket with NIC\n");
    /* Open a socket for our ioctls */
    st.socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    
    /* Set up basic ioctl structure */
    st.ifr.ifr_data = (char *)&st.req;
    strcpy(st.ifr.ifr_name, argv[optind]);

    gettimeofday(&tv, &tz);
    //printf("First, dump firmware out\n");
    /* Stop both CPUs */
    if( swatt(&st, &o_state) == FALSE )
      exit(-1);
    //if( read_smem(&st, 0x140, &o_state) == FALSE )
    //  exit(-1);
   gettimeofday(&tvend, &tzend);
    o_state = ntohl( o_state );
    //printf("swatt return %#x...\n...\n", o_state);
    printf("tvend second = %d, usec=%d\n", tvend.tv_sec, tvend.tv_usec);
    printf("tv sec=%d, usec=%d\n", tv.tv_sec,tv.tv_usec); 
    
 
    printf("time diff is %d usecond\n", (tvend.tv_sec - tv.tv_sec)*1000000 + (tvend.tv_usec-tv.tv_usec));



    //st.ifr.ifr_data = (char *)&st.req;
    //strcpy(st.ifr.ifr_name, argv[optind]);



    close (st.socket);
    
    return 0;
}


/*
 *  EOF trace_dump.c
 */

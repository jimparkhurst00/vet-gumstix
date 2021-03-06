#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#ifdef __MACH__
#include <mach/clock.h>
#include <mach/mach.h>
#endif

#define VET_ICMP_TYPE 0x02
#define VET_ICMP_CODE 0x00

struct icmphdr
{
  u_int8_t type;		/* message type */
  u_int8_t code;		/* type sub-code */
  u_int16_t checksum;
//  char zeros[4];
};

unsigned short checksum(void *b, int len){
    unsigned short *buf = b;
    unsigned int sum=0;
    unsigned short result;

    for ( sum = 0; len > 1; len -= 2 )
        sum += *buf++;
    if ( len == 1 )
        sum += *(unsigned char*)buf;
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    result = ~sum;
    return result;
}

int main(int argc, char* argv[]){
    if (argc != 2) {
        printf("Usage: %s TARGET_IP\n",argv[0]);
        return 0;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(argv[1]);
    
    int sock = socket(PF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sock == -1){
        printf("Socket error, %s\n",strerror(errno));
        return 0;
    }

    unsigned char packet[500];
    struct icmphdr* hdr = (struct icmphdr*)packet;

    hdr -> type = VET_ICMP_TYPE;
    hdr -> code = VET_ICMP_CODE;
    hdr -> checksum = 0x0;

    unsigned int* data = (unsigned int*)&packet[sizeof(struct icmphdr)];
    printf("Preparing nonce: \n");
    srand(time(0));
    unsigned int i;
    for (i=0;i<64;i++){
        data[i]=rand();
    }
    for (i=0;i<8;i++) printf("%08x",data[i]);
    printf("\n");

    hdr -> checksum = checksum(packet, sizeof(struct icmphdr) + 64*sizeof(unsigned int));

    printf("Sending nonce to device and start to time \n waiting for respond...\n\n");

    clock_serv_t cclock;
    mach_timespec_t smts, rmts;
    host_get_clock_service(mach_host_self(), CALENDAR_CLOCK, &cclock);
    clock_get_time(cclock, &smts);
    int ret = sendto(sock, packet, sizeof(struct icmphdr) + 64*sizeof(unsigned int), 0, (struct sockaddr*)&addr, sizeof(struct sockaddr));
    if (ret == -1){
        printf("Send error, %s\n",strerror(errno));
    }
    while (1){
        socklen_t addrlen = sizeof(struct sockaddr);
        int size = recvfrom(sock, packet, sizeof(packet), 0, (struct sockaddr*)&addr, &addrlen);
        if (packet[20] == VET_ICMP_TYPE){
            clock_get_time(cclock, &rmts);
            printf("Checksum received:\n");
            data = (unsigned int*)&packet[20+sizeof(struct icmphdr)];
            for (i=0;i<8;i++) printf("%08x",data[i]);
            printf("\n");
            long tdiff = (rmts.tv_sec - smts.tv_sec)*1000000000L + rmts.tv_nsec - smts.tv_nsec;
            double tdiff_d = (double)tdiff / 1000000;
            printf("Attestation time is %lf ms\n", tdiff_d);
            if (tdiff < 1100000000)
                printf("Results are correct, attestation is within the threshold, so no malware\n");
            else printf("Attestation time is over the threshold!\n");
            break;
        }
    }
    mach_port_deallocate(mach_task_self(), cclock);

    close(sock);

    return 0;
}

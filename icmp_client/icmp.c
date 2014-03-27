#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#define VET_ICMP_TYPE 0x02

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
    hdr -> code = 0x00;
    hdr -> checksum = 0x0;

    unsigned int* data = (unsigned int*)&packet[sizeof(struct icmphdr)];
    unsigned int i;
    for (i=0;i<64;i++){
        data[i]=i+1;
    }

    hdr -> checksum = checksum(packet, sizeof(struct icmphdr) + 64*sizeof(unsigned int));

    int ret = sendto(sock, packet, sizeof(struct icmphdr) + 64*sizeof(unsigned int), 0, (struct sockaddr*)&addr, sizeof(struct sockaddr));
    if (ret == -1){
        printf("Send error, %s\n",strerror(errno));
    }

    printf("Request sent, waiting for respond...\n");
    while (1){
        socklen_t addrlen = sizeof(struct sockaddr);
        int size = recvfrom(sock, packet, sizeof(packet), 0, (struct sockaddr*)&addr, &addrlen);
        if (packet[20] == VET_ICMP_TYPE){
            printf("Got response:\n");
            data = (unsigned int*)&packet[20+sizeof(struct icmphdr)];
            for (i=0;i<64;i++) printf("%08x\n",data[i]);
            break;
        }
    }

    close(sock);

    return 0;
}

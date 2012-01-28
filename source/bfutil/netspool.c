#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>

const char *RHOST = "::ffff:172.31.251.3";
const char *RPORT = "24555";

#define STRBUF (1024)

void readstr(int s, char *buf, int len) {
    char c=0;
    int r;
    while(1) {
      r=recv(s, &c, 1, 0);
      if(r==0){
        puts("remote socket shutdown");
        exit(-3);
      }
      if(r==-1){
        puts("error reading string");
        printf("%d %s\n", errno, strerror(errno));
        exit(-3);
      }
      if(c=='\n') {
        *buf=0;
        return;
      }
      *buf++=c;
      if(c=='\n')
        return;
      len--;
      if(!len) {
        puts("string buffer overflow");
        exit(-3);
      }
    }
}

void sendstr(int s, const char *buf) {
    int l = strlen(buf);
    int c;
    char nl='\n';
    while(l>0) {
	c=send(s, buf, l, 0);
        if(c==-1){
            puts("error sending string");
	    printf("%d %s\n", errno, strerror(errno));
	    exit(-3);
        }
        if(c==0){
            puts("zero send: may loop infinitely");
	    exit(-3);
        }
        l-=c;
    }
    c=send(s, &nl, 1, 0);
        if(c==-1){
	    puts("error sending string");
	    printf("%d %s\n", errno, strerror(errno));
	    exit(-3);
        }
        if(c==0){
	    puts("zero send: may loop infinitely");
	    exit(-3);
        }

}

void savefile(const char *fn, unsigned long long l, int s)
{
    char BUF[STRBUF];
    int n, n1;
    int f;
    f=open(fn, O_CREAT|O_EXCL|O_WRONLY, 0666);
    if(f==-1) {
	    puts("error open file");
	    printf("%d %s\n", errno, strerror(errno));
	    exit(-3);
    }
    while(l) {
	n=recv(s, BUF, l>STRBUF?STRBUF:l, 0);
        if(n==0){
	    puts("remote socket shutdown");
	    exit(-3);
        }
        if(n==-1){
	    puts("error reading data");
	    printf("%d %s\n", errno, strerror(errno));
	    exit(-3);
        }
	n1 = write(f, BUF, n);
	if(n1!=n) {
	    puts("error writing file");
	    printf("%d %s\n", errno, strerror(errno));
	    exit(-3);
	}
	l-=n;
    }
    close(f);
}

void main() {
    int s, r;
    struct addrinfo hint;
    struct addrinfo *addrs, *a;
    char strbuf[STRBUF];
    char filename[STRBUF];
    unsigned long long length;

    s = socket(AF_INET6, SOCK_STREAM, 0);

    memset(&hint, 0, sizeof(struct addrinfo));
    hint.ai_socktype = SOCK_STREAM;
    r = getaddrinfo(RHOST, RPORT, &hint, &addrs);
    if( r ) {
	puts(gai_strerror(r));
	exit(-1);
    }

    a=addrs;
    r=-1;
    while(a) {
	r = connect(s, a->ai_addr, a->ai_addrlen);
	if(r==-1) {
	    printf("%d %s\n", errno, strerror(errno));
	} else {
	    break;
	}
	a=a->ai_next;
    }
    if(r==-1) {
	printf("could not connect\n");
	exit(-2);
    }
    freeaddrinfo(addrs);

    readstr(s, strbuf, STRBUF);
    puts(strbuf);
    sendstr(s, "ADDRESS 2:111/11");
    sendstr(s, "PASSWORD 123");
    sendstr(s, "GET ALL");
    
    filename[0]=0;
    while(1) {
        readstr(s, strbuf, STRBUF);
	puts(strbuf);
	if(strcmp(strbuf, "QUEUE EMPTY")==0) break;
	if(strncmp(strbuf, "FILENAME ", 9)==0) {
	    strcpy(filename, strbuf+9);
	    puts(filename);
	    continue;
	}
	if(strncmp(strbuf, "BINARY ", 7)==0) {
	    if(filename[0]==0) {
		puts("no filename");
		exit(-5);
	    }
	    sscanf(strbuf+7, "%Lu", &length);
	    printf("length=%Lu\n", length);
	    savefile(filename, length, s);
	    sprintf(strbuf, "DONE %s", filename);
	    sendstr(s, strbuf);
	    filename[0]=0;
	    continue;
	}
	
	puts("!!!");
	exit(-1);
    }
    
    sendstr(s, "END satisfied");
    close(s);

}

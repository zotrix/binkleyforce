#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>

#include "netspool.h"

const char *RHOST = "172.31.251.3";
const char *RPORT = "24555";


int readstr(int s, char *buf, int len) {
    char c=0;
    int r;
    while(1) {
      r=recv(s, &c, 1, 0);
      if(r==0){
        puts("remote socket shutdown");
        return -3;
      }
      if(r==-1){
        puts("error reading string");
        printf("%d %s\n", errno, strerror(errno));
        return -3;
      }
      if(c=='\n') {
        *buf=0;
        break;
      }
      *buf++=c;
      len--;
      if(!len) {
        puts("string buffer overflow");
        return -3;
      }
    }
    return 0;
}

int sendstr(int s, const char *buf) {
    int l = strlen(buf);
    int c;
    char nl='\n';
    while(l>0) {
	c=send(s, buf, l, 0);
        if(c==-1){
            puts("error sending string");
	    printf("%d %s\n", errno, strerror(errno));
	    return -3;
        }
        if(c==0){
            puts("zero send: may loop infinitely");
	    return -3;
        }
        l-=c;
    }
    c=send(s, &nl, 1, 0);
        if(c==-1){
	    puts("error sending string");
	    printf("%d %s\n", errno, strerror(errno));
	    return -3;
        }
        if(c==0){
	    puts("zero send: may loop infinitely");
	    return -3;
        }
    return 0;
}



void netspool_start(s_netspool_state *state, const char *host, const char *port, const char *address, const char *password)
{
    int s, r;
    struct addrinfo hint;
    struct addrinfo *addrs, *a;
    char strbuf[STRBUF];

    s = socket(AF_INET6, SOCK_STREAM, 0);
    state->socket = s;
    if( s==-1 )
    {
	state->state = NS_ERROR;
	state->error = "socket error";
	return;
    }

    memset(&hint, 0, sizeof(struct addrinfo));
    hint.ai_socktype = SOCK_STREAM;
    hint.ai_family = PF_INET6;
    hint.ai_flags = AI_V4MAPPED;

    r = getaddrinfo(host, port!=NULL? port: "24555", &hint, &addrs);

    if( r ) {
	state->state = NS_ERROR;
	state->error = gai_error(r);
	return;
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
    freeaddrinfo(addrs);

    if(r==-1) {
	state->state = NS_ERROR;
	state->error = "could not connect\n"
	return;
    }

    r = readstr(s, strbuf, STRBUF);
    if( r ) { state->state = NS_ERROR; state->error = "IO error"; }
    puts(strbuf);

    snprintf(strbuf, STRBUF, "ADDRESS %s", address);
    r = sendstr(s, strbuf);
    if( r ) { state->state = NS_ERROR; state->error = "IO error"; }

    snprintf(strbuf, STRBUF, "PASSWORD %s", password);
    r = sendstr(s, strbuf);
    if( r ) { state->state = NS_ERROR; state->error = "IO error"; }

    state->state = NS_READY;
}

void netspool_query(s_netspool_state *state, const char *what)
{
    int r = sendstr(state->socket, "GET ALL");
    if( r ) {
	state->state = NS_ERROR;
	state->error = "IO error";
	return;
    }
    state->state = NS_RECEIVING;
}

void netspool_receive(s_netspool_state *state)
{
    char strbuf[STRBUF];
    int r;

    r = readstr(s, strbuf, STRBUF);
    if( r ) { state->state = NS_ERROR; state->error = "IO error"; return; }
    puts(strbuf);
    if(strcmp(strbuf, "QUEUE EMPTY")==0) {
	state->state = NS_READY;
	return;
    }

    if(strncmp(strbuf, "FILENAME ", 9)==0) {
        strcpy(filename, strbuf+9);
        puts(filename);
    } else {
	state->state = NS_ERROR;
	state->error = "expected filename or queue empty"
	return;
    }

    r = readstr(s, strbuf, STRBUF);
    if( r ) { state->state = NS_ERROR; state->error = "IO error"; return; }
    if(strncmp(strbuf, "BINARY ", 7)==0) {
	    /*if(filename[0]==0) {
		puts("no filename");
		exit(-5);
	    } */
	sscanf(strbuf+7, "%Lu", &state->length);
	printf("length=%Lu\n", state->length);
	state->state = NS_RECVFILE;
    } else {
	state->state = NS_ERROR;
	state->error = "expected binary"
	return;
    }

}

/*
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
    char filename[STRBUF];
    unsigned long long length;


    
    filename[0]=0;
    while(1) {
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
*/

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>

#include "includes.h"
#include "netspool.h"

#ifdef NETSPOOL

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
	state->error = gai_strerror(r);
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
	state->error = "could not connect";
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
    char strbuf[STRBUF];
    int r;
    snprintf(strbuf, STRBUF, "GET %s", what); /* ALL or comma separated NETMAIL, ECHOMAIL... */
    r = sendstr(state->socket, strbuf);
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

    r = readstr(state->socket, strbuf, STRBUF);
    if( r ) { state->state = NS_ERROR; state->error = "IO error"; return; }
    puts(strbuf);
    if(strcmp(strbuf, "QUEUE EMPTY")==0) {
	state->state = NS_READY;
	return;
    }

    if(strncmp(strbuf, "FILENAME ", 9)==0) {
        strcpy(state->filename, strbuf+9);
        puts(state->filename);
    } else {
	state->state = NS_ERROR;
	state->error = "expected filename or queue empty";
	return;
    }

    r = readstr(state->socket, strbuf, STRBUF);
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
	state->error = "expected binary";
	return;
    }

}

int netspool_read(s_netspool_state *state, void *buf, int buflen)
{
    int n;
    if( state->length == 0 ) {
	puts("everithing is read");
	return 0;
    }
    
    n = recv(state->socket, buf, state->length>buflen? buflen: state->length, 0);

    if(n==0) {
	state->state = NS_ERROR;
	state->error = "remote socket shutdown";
	return -1;
    }
    
    if(n==-1) {
	puts("error reading data");
	printf("%d %s\n", errno, strerror(errno));
	state->state = NS_ERROR;
	state->error = "IO error";
	return -1;
    }

    state->length -= n;
    return n;
}

void netspool_acknowledge(s_netspool_state *state)
{
    char strbuf[STRBUF];
    int r;

    if( state->length > 0 ) {
	state->state = NS_ERROR;
	state->error = "Too early acknowledgement";
	return;
    }

    snprintf(strbuf, STRBUF, "DONE %s", state->filename);
    r = sendstr(state->socket, strbuf);
    if( r ) { state->state = NS_ERROR; state->error = "IO error"; return; }

    state->state = NS_RECEIVING;
    state->filename[0] = 0;
}

void netspool_end(s_netspool_state *state)
{
    sendstr(state->socket, "END satisfied");
    close(state->socket);
    state->state = NS_NOTINIT;
}

/*
void savefile(const char *fn, unsigned long long l, int s)
{
    char BUF[STRBUF];
    int n, n1;
    int f;
    f=open(fn, O_CREAT|O_EXCL|O_WRONLY, 0664);
    if(f==-1) {
	    puts("error open file");
	    printf("%d %s\n", errno, strerror(errno));
	    exit(-3);
    }
    while(l) {
	n1 = write(f, BUF, n);
	if(n1!=n) {
	    puts("error writing file");
	    printf("%d %s\n", errno, strerror(errno));
	    exit(-3);
	}
    }
    close(f);
}
*/
#endif

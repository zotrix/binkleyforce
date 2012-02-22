/*
 *	binkleyforce -- unix FTN mailer project
 *	
 *	Copyright (c) 1998-2000 Alexander Belkin, 2:5020/1398.11
 *	
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *	
 *	$Id$
 */

#include "includes.h"
#include "confread.h"
#include "logger.h"
#include "util.h"
#include "io.h"
#include "session.h"
#include "outbound.h"
#include "prot_common.h"
#include "prot_binkp.h"

#define BINKP_HEADER (2)
#define BINKP_MAXBLOCK (32768)

#define BINKP_BLK_CMD (1)
#define BINKP_BLK_DATA (2)

typedef enum {
    frs_nothing,
    frs_data,
    frs_didget,
    frs_skipping
} e_file_receive_status;

typedef struct {
    e_binkp_mode mode;
    int phase;
    int subphase;
    s_binkp_sysinfo *local_data;
    s_binkp_sysinfo *remote_data;
    s_protinfo *pi;
    bool address_established; // indicates that remote address is verified and disallows further change
    char extracmd[BINKP_MAXBLOCK+1];
    bool extraislast;
    bool password_received;
    bool NR;
    bool MB;
    bool complete;
    int batchsendcomplete;
    int batchreceivecomplete;
    bool waiting_got;
    int batch_send_count;
    int batch_recv_count;
    e_file_receive_status frs;
    int emptyloop;
    bool continuesend;
} s_binkp_state;

int binkp_getforsend(s_binkp_state *bstate, char *buf, int *block_type, unsigned short *block_length);
int binkp_doreceiveblock(s_binkp_state *bstate, char *buf, int block_type, unsigned short block_length);
void binkp_process_NUL(s_binkp_sysinfo *remote_data, char *buffer);
void binkp_process_ADR(s_binkp_sysinfo *remote_data, char *buffer);

int binkp_loop(s_binkp_state *bstate) {
    unsigned char readbuf[BINKP_HEADER+BINKP_MAXBLOCK+1];
    unsigned char writebuf[BINKP_HEADER+BINKP_MAXBLOCK+1];

    int rc = HRC_OK;

    bstate->phase = 0;
    bstate->subphase = 0;
    bstate->extracmd[0] = -1;
    bstate->extraislast = false;
    bstate->password_received = false;
    bstate->NR = false;
    if(bstate->remote_data->options & BINKP_OPT_NR) {
        bstate->NR = true;
    }
    bstate->MB = false;
    if(bstate->remote_data->options & BINKP_OPT_MB) {
        bstate->MB = true;
    }
    bstate->complete = false; // end in this mode (handshake or session)
    bstate->batchsendcomplete = 0;
    bstate->batchreceivecomplete = 0;
    bstate->waiting_got = false;
    bstate->batch_send_count = 0;
    bstate->batch_recv_count = 0;
    bstate->frs = frs_nothing;
    bstate->emptyloop = 0;
    bstate->continuesend = false;

    unsigned short read_pos=0;
    unsigned short read_blklen=0;
    unsigned short want_read=BINKP_HEADER;

    unsigned short write_pos=0;
    unsigned short have_to_write=0;

    int n, m;
    bool no_more_to_send = false;
    bool no_more_read = false;
    bool canread, canwrite;

    int timeout = conf_number(cf_binkp_timeout);
    if( timeout==0 )
        timeout = 60;

    /* used for higher level calls */
    int block_type;
    unsigned short block_length;

    //     session criterium           handshake criterium
    while (!bstate->complete || bstate->waiting_got) {
        log("loop s: %d r: %d", bstate->batchsendcomplete, bstate->batchreceivecomplete);
        if (bstate->continuesend) {
            no_more_to_send = false;
            bstate->continuesend = false;
        }
        if (have_to_write==0 && (!no_more_to_send || bstate->extracmd[0]!=-1)) {
            m = binkp_getforsend(bstate, writebuf+BINKP_HEADER, &block_type, &block_length);
            if(m==1 || m==3) {
                //log("got block for sending %d %hu", block_type, block_length);
                write_pos = 0;
                have_to_write = block_length+BINKP_HEADER;
                if( block_type == BINKP_BLK_CMD ) {
                    writebuf[0] = (block_length>>8)|0x80;
                }
                else if( block_type == BINKP_BLK_DATA ) {
                    writebuf[0] = (block_length>>8)&0x7f;
                } else {
                    log("block for sending has invalid type, aborting");
                    return -1;
                }
                writebuf[1] = block_length&0xff;
            }
            if (m==2 || m==3) {
                log("no more to send");
                no_more_to_send = true;
            }
            if (m==0) {
                log("binkp: nothing to write");
            }
            if (m<0 || m>3) {
                log("getforsend error");
                return -1;
            }
        }

        if (bstate->batchsendcomplete && bstate->batchreceivecomplete) {
            log("batch is complete");
            if (bstate->MB && (bstate->batch_send_count || bstate->batch_recv_count)) {
                log("starting one more batch");
                bstate->batchsendcomplete -= 1;
                bstate->batchreceivecomplete -= 1;
                //bstate->firstbatch = false;
                bstate->batch_send_count = 0;
                bstate->batch_recv_count = 0;
                no_more_to_send = false;
                bstate->phase = 0;
                bstate->frs = frs_nothing;
                want_read = BINKP_HEADER;
                continue;
            }
            else {
                if (bstate->waiting_got) {
                    log("waiting for all files have being confirmed");
                }
                else {
                    log("finishing session");
                    bstate->complete = true;
                    want_read = 0;
                }
            }
        }

        log("select read: %d write %d", want_read, have_to_write);
        if (want_read || have_to_write) {
          n = tty_select(want_read?&canread:NULL, have_to_write?&canwrite:NULL, timeout);
          if( n<0 ) {
              log("binkp error on tty_select");
              return -1;
          }
        }
        else {
            log("empty loop %d", ++bstate->emptyloop);
            if (bstate->emptyloop==10) {
                return -1;
            }
        }

        if(want_read && canread) {
            n = tty_read(readbuf+read_pos, want_read);
            if( n<0 ) {
                log("binkp: tty read error");
                return -1;
            } 
            else if (n==0) {
                log("read: remote socket shutdown");
                return -1;
            }
            want_read -= n;
            read_pos += n;
            if (read_pos == BINKP_HEADER) {
                // have read header, want read body
                log("it should be 0: %d", want_read);
                want_read = ((unsigned short)(readbuf[0]&0x7F)<<8) | readbuf[1];
                log("pending block, length %u", want_read);
            } // no else here: if want_read may be zero here for zero length block
        }

        // no_more_read only signs that read thread do not keep connection anymore but messages should be processed
        if (want_read==0 && read_pos) { // check every loop, not only just after read as accepting may be deferred
                block_type = readbuf[0]&0x80? BINKP_BLK_CMD: BINKP_BLK_DATA;
                block_length = read_pos - BINKP_HEADER;
                log("binkp: complete block is received %d %hu", block_type, block_length);
                m = binkp_doreceiveblock(bstate, readbuf+BINKP_HEADER, block_type, block_length);
                if(m==1) {
                    log("block is successfully accepted");
                    read_pos = 0;
                    want_read = BINKP_HEADER;
                } else if (m==2) {
                    log("block accepted and no more is needed in this mode");
                    no_more_read = true;
                    read_pos = 0;
                    want_read = 0; //BINKP_HEADER;
                }
                else if (m==0) {
                    log("binkp: keeping buffer");
                }
                else if (m==3) {
                    log("aborting session");
                    bstate->complete = true;
                    rc = HRC_OTHER_ERR;
                }
                else {
                    log("doreceiveblock error");
                    return -1;
                }
        }

        if (have_to_write && canwrite) {
            log("writing %d pos %d", have_to_write, write_pos);
            n = tty_write(writebuf+write_pos, have_to_write);
            if( n<0 ) {
                log("binkp: tty write error");
                return -1;
            } 
            else if (n==0) {
                log("write: remote socket shutdown");
                return -1;
            }
            //log("%d bytes sent", n);
            write_pos += n;
            have_to_write -= n;
        }
    }
    return rc;
}

int binkp_outgoing(s_binkp_sysinfo *local_data, s_binkp_sysinfo *remote_data)
{
    s_binkp_state s;
    s.mode = bmode_outgoing_handshake;
    s.local_data = local_data;
    s.remote_data = remote_data;
    s.pi = NULL;
    s.address_established = false;
    return binkp_loop(&s);
}

int binkp_incoming(s_binkp_sysinfo *local_data, s_binkp_sysinfo *remote_data)
{
    s_binkp_state s;
    s.mode = bmode_incoming_handshake;
    s.local_data = local_data;
    s.remote_data = remote_data;
    s.pi = NULL;
    s.address_established = false;
    return binkp_loop(&s);
}

int binkp_transfer(s_binkp_sysinfo *local_data, s_binkp_sysinfo *remote_data, s_protinfo *pi)
{
    log("start transfer");
    s_binkp_state s;
    s.mode = bmode_transfer;
    s.local_data = local_data;
    s.remote_data = remote_data;
    s.pi = pi;
    return binkp_loop(&s);
}


// hanshake
// send
//  all nuls
//  address
//  wait password
//  send OK/ERR
//  -- end handshake
//  M_FILE
//  (wait M_GET)
//  data
//  wait M_GOT
//  next file
//  send EOB

// recv
//  accept NULs
//  accept address
//  accept password
//  -- end handshake
//  accept M_GET or
//  accept M_FILE
//  accept data
//  accept next file or EOB


int binkp_getforsend(s_binkp_state *bstate, char *buf, int *block_type, unsigned short *block_length)
{
    int my_sf, wr_pos;
    int n; // read file
    if (bstate->extracmd[0]!=-1) {
        log("extra command from receiver %d %s", bstate->extracmd[0], bstate->extracmd+1);
        buf[0] = bstate->extracmd[0];
        strcpy(buf+1, bstate->extracmd+1);
        *block_type = BINKP_BLK_CMD;
        *block_length = strlen(buf+1)+1;
        bstate->extracmd[0] = -1;
        if (bstate->extraislast) {
            bstate->phase = 100;
            log("extracmd is last");
            bstate->complete = true;
        }
        return 1;
    }
    if (bstate->mode==bmode_incoming_handshake || bstate->mode==bmode_outgoing_handshake ) {
	switch( bstate->phase ) {
case 0: // MD5 challenge
        bstate->phase+=1;
        bstate->subphase=0;
	if( bstate->mode==bmode_incoming_handshake && bstate->local_data->challenge_length > 0 )
	{
                log("send challenge");
		char challenge[128];
		string_bin_to_hex(challenge, bstate->local_data->challenge, bstate->local_data->challenge_length);
		buf[0] = BPMSG_NUL;
		sprintf(buf+1, "OPT CRAM-MD5-%s", challenge);
		log("sent %s", buf+1);
		*block_type = BINKP_BLK_CMD;
		*block_length = strlen(buf+1)+1;
		return 1;
	}	

case 1: // send sysinfo
        my_sf = bstate->subphase;
        bstate->subphase+=1;
        *block_type = BINKP_BLK_CMD;
        switch( my_sf ) {
case 0:
            buf[0]=BPMSG_NUL;
	    *block_length = 1 + sprintf(buf+1, "SYS %s", bstate->local_data->systname);
	    return 1;
case 1:
	    buf[0]=BPMSG_NUL;
	    *block_length = 1 + sprintf(buf+1, "ZYZ %s", bstate->local_data->sysop);
	    return 1;
case 2:
	    buf[0]=BPMSG_NUL;
	    *block_length = 1 + sprintf(buf+1, "LOC %s", bstate->local_data->location);
	    return 1;
case 3:
	    buf[0]=BPMSG_NUL;
	    *block_length = 1 + sprintf(buf+1, "PHN %s", bstate->local_data->phone);
	    return 1;
case 4:
	    buf[0]=BPMSG_NUL;
	    *block_length = 1 + sprintf(buf+1, "NDL %s", bstate->local_data->flags);
	    return 1;
case 5:
	    buf[0]=BPMSG_NUL;
	    *block_length = 1 + sprintf(buf+1, "TIME %s", bstate->local_data->timestr);
	    return 1;
case 6:
            buf[0]=BPMSG_NUL;
	    *block_length = 1 + sprintf(buf+1, "VER %s %s/%d.%d",
		bstate->local_data->progname, bstate->local_data->protname,
		bstate->local_data->majorver, bstate->local_data->minorver);
	    return 1;
case 7:
	    if (bstate->mode==bmode_outgoing_handshake) {
	        buf[0]=BPMSG_NUL;
		strcpy(buf+1, "OPT MB");
		if (!nodelist_checkflag (state.node.flags, "NR"))
			strcat(buf+1, " NR");
		// ND is too complicated and have unclear gain
		// seems not to remove files from inbound until successful session end is enough to eliminate dupes
		// if (!nodelist_checkflag (state.node.flags, "ND"))
		//	strcat(buf+1, " ND");
		*block_length = 1 + strlen(buf+1);
		return 1;
	    }
	    // else skip subphase
	    my_sf += 1;
	    bstate->subphase += 1;
//case 8:
	}
	// here if subphase==8
	bstate->phase += 1;
	bstate->subphase = 0;
	// p

case 2:
        log("send address");
        bstate->phase += 1;
        buf[0] = BPMSG_ADR;
        wr_pos = 1;
        int i;
	for( i = 0; i < bstate->local_data->anum; i++ )
	{
		if (i) wr_pos += sprintf(buf+wr_pos, " ");
		if (bstate->local_data->addrs[i].addr.point) {
                    wr_pos += sprintf(buf+wr_pos, "%d:%d/%d.%d@%s",
	                 bstate->local_data->addrs[i].addr.zone, bstate->local_data->addrs[i].addr.net,
	                 bstate->local_data->addrs[i].addr.node, bstate->local_data->addrs[i].addr.point,
	                 bstate->local_data->addrs[i].addr.domain);
	        }
	        else {
                    wr_pos += sprintf(buf+wr_pos, "%d:%d/%d@%s",
	                 bstate->local_data->addrs[i].addr.zone, bstate->local_data->addrs[i].addr.net,
	                 bstate->local_data->addrs[i].addr.node,
	                 bstate->local_data->addrs[i].addr.domain);
	        }
	}
	*block_type = BINKP_BLK_CMD;
	*block_length = wr_pos;
	log("address: %s", buf+1);
	return 1;

case 3: // send password on outgoing or pw confirmation on incoming
        // special empty password is sent if there is no password for the remote addr
        if (bstate->mode==bmode_incoming_handshake) {
            if (bstate->password_received) {
                log("password verified");
                buf[0] = BPMSG_OK;
                *block_type = BINKP_BLK_CMD;
                *block_length = 1;
                bstate->phase += 1;
                return 1;
            }
            log("waiting for password from remote");
            return 0; // nothing to send
        }
        else if (bstate->mode==bmode_outgoing_handshake) {
            if (!bstate->address_established) {
                log("address not received still");
                return 0;
            }
            log("sending password");

            buf[0] = BPMSG_PWD;
            *block_type = BINKP_BLK_CMD;

            if( bstate->local_data->passwd == '\0' ) {
                *block_length = 1 + sprintf(buf+1, "-");
	    }
            else if( bstate->remote_data->options & BINKP_OPT_MD5 ) {
		char digest_bin[16];
		char digest_hex[33];
		
		if(bstate->remote_data->challenge_length==0) {
		    log("waiting for challenge");
		    return 0;
		}
		md5_cram_get(bstate->local_data->passwd, bstate->remote_data->challenge,
			     bstate->remote_data->challenge_length, digest_bin);
		
		/* Encode digest to the hex string */
		string_bin_to_hex(digest_hex, digest_bin, 16);
		
		*block_length = 1 + sprintf(buf+1, "CRAM-MD5-%s", digest_hex);
	    }
	    else {
                *block_length = 1 + sprintf(buf+1, "%s", bstate->local_data->passwd);
            }
            bstate->phase += 1;
            return 1;
        }
        else {
            log("impossible mode");
            return -1;
        }


case 4:
        if (bstate->mode==bmode_incoming_handshake) {
            log("incoming handshake is complete");
            bstate->complete = true;
        }
        else {
            log("outgoing handshake: everything is sent");
        }
        return 2;
        }

    }
    else if (bstate->mode == bmode_transfer) {

        switch (bstate->phase) {
            send_next_file:
            case 0:
                log("fetch file from queue");
                if (p_tx_fopen(bstate->pi, NULL)) {
                    log("queue empty");
                    bstate->phase = 4;
                    goto send_EOB;
                }
                bstate->waiting_got = true;
                bstate->batch_send_count += 1;

                //send M_FILE -1
                if (bstate->NR) {
                    log("send M_FILE with -1");
                    buf[0] = BPMSG_FILE;
                    *block_length = 1+sprintf(buf+1, "%s %ld %ld -1", bstate->pi->send->net_name, 
                            (long)bstate->pi->send->bytes_total, (long)bstate->pi->send->mod_time);
                    *block_type = BINKP_BLK_CMD;
                    return 3; // no state change. phase would be changed to 1 by recv when M_GET is received
                }
                bstate->phase += 1;

            case 1: //send M_FILE - M_GET forcibly sets this phase. M_GET must open needed file
                log("send M_FILE");
                buf[0] = BPMSG_FILE;
                *block_length = 1+sprintf(buf+1, "%s %ld %ld 0", bstate->pi->send->net_name, 
                        bstate->pi->send->bytes_total, bstate->pi->send->mod_time);
                *block_type = BINKP_BLK_CMD;
                bstate->phase += 1;
                return 1;

            case 2: //send file data
                n = p_tx_readfile (buf, 4096, bstate->pi); // BINKP_MAXBLOCK
                if (n>0) {
                    *block_type = BINKP_BLK_DATA;
                    *block_length = n;
                    bstate->pi->send->bytes_sent += n;
                    return 1;
                }
                else if (n<0) {
                    log("p_tx_readfile error");
                    return -1;
                }
                log("file is sent");
                bstate->pi->send->status = FSTAT_WAITACK;
                
                bstate->phase += 1;
                
            case 3: //wait for acknowlede
            
                if (bstate->pi->send->waitack) {
                    log("file must be acknowledged with M_GOT");
                    int i;
                    bool ack = false;
                    for(i = 0; i < bstate->pi->n_sentfiles; i++ ) {
                        if (p_compfinfo(&bstate->pi->sentfiles[i], bstate->pi->send->net_name, bstate->pi->send->bytes_total, bstate->pi->send->mod_time) == 0) {
                            if (bstate->pi->sentfiles[i].status == FSTAT_SUCCESS) {
                                ack = true;
                                log("acknowledged");
                                break;
                            }
                        }
                    }
                    if (!ack) {
                        log("wait for M_GOT");
                        return 0;
                    }
                    log("M_GOT received, going to next file");
                } else {
                    log("do not wait M_GOT");
                }
                bstate->phase = 0;
                goto send_next_file;

            send_EOB:
            case 4:
                log("send EOB n_sentfile=%d", bstate->pi->n_sentfiles);
                buf[0] = BPMSG_EOB;
                *block_type = BINKP_BLK_CMD;
                *block_length = 1;
                bstate->batchsendcomplete += 1;
                bstate->phase += 1;
                return 1;

            case 5:
                log("nothing to send");
                return 2;




        }

    } else {
        log("invalid mode");
        return -1;
    }
    log("unrecognized state, shutting down");
    bstate->complete = true;
    return 2;

}


int binkp_doreceiveblock(s_binkp_state *bstate, char *buf, int block_type, unsigned short block_length)
{
    switch (block_type) {
case BINKP_BLK_CMD:
        if (block_length<1) {
            log("zero length command received");
            return -1;
        }
        buf[block_length] = 0; // fencing for easy processing
        switch (buf[0]) {
case BPMSG_NUL:          /* Site information, just logging */
            log("M_NUL");
            binkp_process_NUL(bstate->remote_data, buf+1);
            return 1;
case BPMSG_ADR:              /* List of addresses */
            log("M_ADR");
            if (bstate->address_established) {
                log("remote tries to change address");
                return -1;
            }
            if( bstate->extracmd[0] !=-1 ) return 0; // suspend !!!
            binkp_process_ADR(bstate->remote_data, buf+1);

	    if( !bstate->remote_data->anum ) {
	        log("error: remote did not supplied any addresses");
	        if( bstate->extracmd[0] !=-1 ) return 0; // suspend
	        bstate->extracmd[0] = BPMSG_BSY;
	        strcpy(bstate->extracmd+1, "No addresses was presented");
		bstate->extraislast = true;
        	return 1;
            }

            if (bstate->mode == bmode_incoming_handshake) {
		int i;
		log("sending options");
		bstate->extracmd[0] = BPMSG_NUL;
		bstate->extraislast = false;
		sprintf(bstate->extracmd+1,"OPT MB");
		s_override ovr;
		for(i = 0; i < bstate->remote_data->anum; i++) {
		    ovr.sFlags = "";
		    override_get (&ovr, bstate->remote_data->addrs[i].addr, 0);
		    if (nodelist_checkflag (ovr.sFlags, "NR")==0) {
			strcat (bstate->extracmd+1, " NR");
			break;
		    }
	        }
            }
            // further use extracmd only for errors

	    if (bstate->mode == bmode_outgoing_handshake) {
	        // check that remote has the address we call
		if( session_addrs_check_genuine(bstate->remote_data->addrs, bstate->remote_data->anum,
			                             state.node.addr) ) {
                    log("error: remote does not have the called address");
	            bstate->extracmd[0] = BPMSG_ERR;
	            strcpy(bstate->extracmd+1, "Sorry, you are not who I need");
	            bstate->extraislast = true;
	            return 1;
                }
                // check that all addresses of remote has the same password
		strncpy(bstate->remote_data->passwd, bstate->local_data->passwd, BINKP_MAXPASSWD);
		bstate->remote_data->passwd[BINKP_MAXPASSWD] = '\0';

		if( session_addrs_check(bstate->remote_data->addrs, bstate->remote_data->anum,
			                             bstate->remote_data->passwd, NULL, 0) ) {
	            log("error: Security violation");
	            bstate->extracmd[0] = BPMSG_ERR;
	            strcpy(bstate->extracmd+1, "Security violation");
	            bstate->extraislast = true;
        	    return 1;
		}
            }

            bstate->address_established = true;
            return 1;
case BPMSG_PWD:              /* Session password */
            log("M_PWD received");
            if (bstate->mode != bmode_incoming_handshake) {
                log("unexpected M_PWD");
                return -1;
            }
            if (!bstate->address_established) {
                log("M_PWD before M_ADR");
                return -1;
            }

	    strnxcpy(bstate->remote_data->passwd, buf+1, block_length);
	    memcpy(bstate->remote_data->challenge, bstate->local_data->challenge, BINKP_MAXCHALLENGE+1);
	    bstate->remote_data->challenge_length = bstate->local_data->challenge_length;

            /* Do authorization */
	    if( binkp_auth_incoming(bstate->remote_data) ) {
	        log("error: invalid password");
	        if( bstate->extracmd[0] !=-1 ) return 0; // suspend if extra is occupied
	        bstate->extracmd[0] = BPMSG_ERR;
	        strcpy(bstate->extracmd+1, "Security violation");
	        bstate->extraislast = true;
	        return 1;
	    }
	    // lock addresses
	    if( session_addrs_lock(bstate->remote_data->addrs, bstate->remote_data->anum) ) {
	        log("error locking addresses of the remote");
	        if( bstate->extracmd[0] !=-1 ) return 0; // suspend if extra is occupied
	        bstate->extracmd[0] = BPMSG_BSY;
	        strcpy(bstate->extracmd+1, "All addresses are busy");
	        bstate->extraislast = true;
	        return 1;
	    }
	    else {
	        log("flag password received");
		bstate->password_received = true;
		return 2;
	    }
            break;

case BPMSG_FILE:             /* File information */
            log("M_FILE");
            if (bstate->mode != bmode_transfer) {
                log("unexpected M_FILE");
                return -1;
            }
            s_bpfinfo recvfi;
            if( binkp_parsfinfo(buf+1, &recvfi, true) ) {
		log ("M_FILE parse error: %s", buf + 1);
		return -1;
            }
            bstate->batch_recv_count += 1;
            if (bstate->frs == frs_data) {
                log("overlapping M_FILE received");
                return -1;
            }

	    if (bstate->frs == frs_didget) {
                log("is it what we want?");
                if( bstate->pi->recv && p_compfinfo(bstate->pi->recv, recvfi.fn, recvfi.sz, recvfi.tm) == 0
			 && bstate->pi->recv->bytes_skipped == recvfi.offs && bstate->pi->recv->fp ) {
                    log("resuming %s from offset %d", recvfi.fn, recvfi.offs);
	            bstate->frs = frs_data;
		    return 1;
	        }
	        log("no, skipping (TODO: accept it)");
		if( bstate->extracmd[0] != -1 ) return 0;
		bstate->extracmd[0] = BPMSG_SKIP;
		sprintf(bstate->extracmd+1, "%s %ld %ld %ld", recvfi.fn, recvfi.sz, recvfi.tm);
		bstate->extraislast = false;
		return 1;
	    }

	    if (bstate->frs!=frs_nothing && bstate->frs!=frs_skipping) {
	        log("strange receiving mode %d", bstate->frs);
	        return -1;
	    }

            if( bstate->extracmd[0] != -1 ) return 0;
	    switch(p_rx_fopen(bstate->pi, recvfi.fn, recvfi.sz, recvfi.tm, 0)) {
case 0:
		if (bstate->pi->recv->bytes_skipped == recvfi.offs) {
		        log("accepting file %s from offset %d", recvfi.fn, recvfi.offs);
			bstate->frs = frs_data;
			return 1;
		}
		log("making M_GET to skip downloaded part");
		bstate->extracmd[0] = BPMSG_GET;
		sprintf(bstate->extracmd+1, "%s %ld %ld %ld",
					bstate->pi->recv->net_name, (long)bstate->pi->recv->bytes_total,
					(long)bstate->pi->recv->mod_time,
					(long)bstate->pi->recv->bytes_skipped);
		bstate->extraislast = false;
		bstate->frs = frs_didget;
		return 1;
case 1:
		log("SKIP (non-destructive)");
		bstate->extracmd[0] = BPMSG_SKIP;
		sprintf(bstate->extracmd+1, "%s %ld %ld", bstate->pi->recv->net_name, (long)bstate->pi->recv->bytes_total,
					(long)bstate->pi->recv->mod_time);
		bstate->extraislast = false;
	        bstate->frs = frs_skipping;
		return 1;
case 2:
		log("SKIP (destructive)");
		bstate->extracmd[0] =  BPMSG_GOT;
		sprintf(bstate->extracmd+1, "%s %ld %ld",
					bstate->pi->recv->net_name, (long)bstate->pi->recv->bytes_total,
					(long)bstate->pi->recv->mod_time);
		bstate->extraislast = false;
		bstate->frs = frs_skipping;
		return 1;
default:
		log("p_rx_fopen_error");
		return -1;
            }
            log("never get here");
            return -1;
            
case BPMSG_OK:               /* Password was acknowleged (data ignored) */
            log("M_OK received");
            if (bstate->mode != bmode_outgoing_handshake) {
                log("unexpected M_OK");
                return -1;
            }
            if (session_addrs_lock(bstate->remote_data->addrs, bstate->remote_data->anum)) {
                log("error: unable to lock");
                if (bstate->extracmd[0]!=-1) return 0;
                bstate->extracmd[0] = BPMSG_BSY;
                strcpy(bstate->extracmd+1, "All addresses are busy");
		bstate->extraislast = true;
		return 2;
	    }
	    log("outoing handshake successfully complete");
	    bstate->complete = true;
            return 2;

case BPMSG_EOB:              /* End Of Batch (data ignored) */
            log("M_EOB received");
            if (bstate->mode != bmode_transfer) {
                log("unexpected M_EOB");
                return -1;
            }
            bstate->batchreceivecomplete += 1;
            return 1; // continue receiving as M_GOT may and would arrive

case BPMSG_GOT:              /* File received */
case BPMSG_SKIP:
            log("received GOT/SKIP");
            if (bstate->mode != bmode_transfer) {
                log("unexpected M_GOT/M_SKIP");
                return -1;
            }
            s_bpfinfo fi;
            int i;
	    if (binkp_parsfinfo (buf+1, &fi, false)) {
	        log("error parsing");
	        return -1;
	    }

	    if (strcmp (bstate->pi->send->net_name, fi.fn) == 0 && bstate->pi->send->status != FSTAT_WAITACK) {
	        log("aborting current file");
	        if (bstate->pi->send->netspool) {
	            log("cannot abort netspool in progress");
	            return -1;
	        }
	        p_tx_fclose(bstate->pi);
	        bstate->phase = 0;
	    }
	
	    for(i = 0; i < bstate->pi->n_sentfiles; i++ ) {
	        if (p_compfinfo (&bstate->pi->sentfiles[i], fi.fn, fi.sz, fi.tm) == 0) {
	            s_finfo *tmp = bstate->pi->send;
	            bstate->pi->send = &bstate->pi->sentfiles[i];
	            if (buf[0] == BPMSG_SKIP) {
	                if (bstate->pi->send->netspool) {
	                    log("cannot skip netspool");
	                    return -1; // no reason to continue sending
	                }
	                log("skipped %s", fi.fn);
	                bstate->pi->send->status = FSTAT_REFUSED;
	            } else {
	                if (bstate->pi->send->status == FSTAT_WAITACK) {
	                    log("confirmed %s", fi.fn);
	                    bstate->pi->send->status = FSTAT_SUCCESS;
	                } else {
	                    log("confirmed not sent file - skipped %s", fi.fn);
	                    if (bstate->pi->send->netspool) {
	                        log("cannot skip netspool");
	                        return -1; // no reason to continue sending
	                    }
		            bstate->pi->send->status = FSTAT_SKIPPED;
	                }
	            }
	            log("closing file");
	            p_tx_fclose(bstate->pi);
	            bstate->pi->send = tmp;
	            goto check_that_all_files_are_confirmed;
	        }
	    }
            log("unmatched file name");
            return -1;

check_that_all_files_are_confirmed:
            {
                int i;
                for (i = 0; i < bstate->pi->n_sentfiles; i++) {
                    if (bstate->pi->sentfiles[i].status == FSTAT_WAITACK) {
                        log("sent file %d waits for acknowlede", i);
                        return 1;
                    }
                }
            }
            log("all files are confirmed");
            bstate->waiting_got = false;
            return 1;


case BPMSG_ERR:              /* Misc errors */
            log("remote error: %s", buf+1);
            return 3;
case BPMSG_BSY:              /* All AKAs are busy */
            log("remote busy: %s", buf+1);
            return 3;

case BPMSG_GET:              /* Get a file from offset */
            log("received M_GET: cancel transmitting current file and send requested file if it is in outbound");
            if (bstate->mode != bmode_transfer) {
                log("unexpected M_GET");
                return -1;
            }
            s_bpfinfo getfi;
            if (binkp_parsfinfo(buf+1, &getfi, true) != 0) {
                log("error parsing M_GET %s", buf+1);
                return -1;
            }
            log("M_GET file %s size %d time %d offset %d", getfi.fn, getfi.sz, getfi.tm, getfi.offs);

            if (bstate->extracmd[0] != -1) return 0;

            if (bstate->pi->send) if (p_compfinfo(bstate->pi->send, getfi.fn, getfi.sz, getfi.tm)==0) {
                log("M_GET for currently transmitted file");
                if (getfi.offs==bstate->pi->send->bytes_sent) {
                    log("M_GET offset match current (seems NR mode)");
                    // go to sending M_FILE
                    bstate->phase = 2;
                    bstate->extracmd[0] = BPMSG_FILE;
                    sprintf(bstate->extracmd+1, "%s %ld %ld %ld",
							bstate->pi->send->net_name, (long)bstate->pi->send->bytes_total,
							(long)bstate->pi->send->mod_time, (long)bstate->pi->send->bytes_sent);
		    bstate->extraislast = false;
		    bstate->continuesend = true;
		    return 1;
		
                }
            }

            if (bstate->pi->send) if (bstate->pi->send->netspool) {
                log("ignore differing M_GET for netspool");
                bstate->continuesend = true;
                return 1;
            }

            if (bstate->pi->send) if (p_compfinfo(bstate->pi->send, getfi.fn, getfi.sz, getfi.tm)==0) {
		log("resending \"%s\" from %ld offset", bstate->pi->send->net_name, (long)getfi.offs);
                if( p_tx_rewind(bstate->pi, getfi.offs) != 0 ) {
                    log("failed to rewind");
                    p_tx_fclose(bstate->pi);
                    return -1;
                }
		bstate->pi->send->bytes_skipped = getfi.offs;
		bstate->pi->send->bytes_sent = getfi.offs;
		bstate->extracmd[0] = BPMSG_FILE;
		sprintf(bstate->extracmd+1, "%s %ld %ld %ld", bstate->pi->send->net_name, (long)bstate->pi->send->bytes_total,
							(long)bstate->pi->send->mod_time, (long)getfi.offs);
		bstate->extraislast = false;
		bstate->phase = 2;
		bstate->continuesend = true;
		return 1;
	    }

            if( bstate->pi->send ) {
                log("aborting current file");
                p_tx_fclose(bstate->pi);
            }

            s_filehint hint;
            hint.fn = getfi.fn;
            hint.sz = getfi.sz;
            hint.tm = getfi.tm;
            if( p_tx_fopen(bstate->pi, &hint) != 0 ) {
                log("could not satisfy M_GET");
                return -1;
            }
            if( p_tx_rewind(bstate->pi, getfi.offs) != 0 ) {
                log("failed to rewind");
                p_tx_fclose(bstate->pi);
                return -1;
            }
            bstate->waiting_got = true;
	    log("sending \"%s\" from %ld offset", bstate->pi->send->net_name, (long)getfi.offs);
	    bstate->pi->send->bytes_skipped = getfi.offs;
	    bstate->pi->send->bytes_sent = getfi.offs;
	    bstate->extracmd[0] = BPMSG_FILE;
	    sprintf(bstate->extracmd+1, "%s %ld %ld %ld", bstate->pi->send->net_name, (long)bstate->pi->send->bytes_total,
	                                        (long)bstate->pi->send->mod_time, (long)getfi.offs);
	    bstate->extraislast = false;
	    bstate->phase = 2;
	    bstate->continuesend = true;
	    return 1;
        }
        log("unknown command %d received", buf[0]);
        return -1;

case BINKP_BLK_DATA:
        //if there is file in progress
        log("data block received length=%d", block_length);
        if (block_length==0) {
            log("ignore zero length data block, argus workaround");
            return 1;
        }
        if (bstate->frs == frs_nothing) {
            log("unexpected data block");
            return -1;
        }
        if (bstate->frs == frs_didget || bstate->frs == frs_skipping) {
            log("did M_GET or M_GOT or M_SKIP, skipping data");
            return 1;
        }

        if (bstate->extracmd[0] != -1) return 0;

        long int n;
        n = p_rx_writefile(buf, block_length, bstate->pi);

	if( n < 0 ) {
	    log("error writing file");
	    if( n == -2 ) {
		bstate->extracmd[0] = BPMSG_GOT;
		sprintf(bstate->extracmd+1, "%s %ld %ld", bstate->pi->recv->net_name, (long)bstate->pi->recv->bytes_total,
							(long)bstate->pi->recv->mod_time);
		bstate->extraislast = false;
	    }
	    else {
	        bstate->extracmd[0] = BPMSG_SKIP;
		sprintf(bstate->extracmd+1, "%s %ld %ld", bstate->pi->recv->net_name, (long)bstate->pi->recv->bytes_total,
							(long)bstate->pi->recv->mod_time);
		bstate->extraislast = false;
	    }
	    bstate->frs = frs_skipping;
	    p_rx_fclose(bstate->pi);
	    return 1;
	}
	else {
	    bstate->pi->recv->bytes_received += block_length;

            /* Was it the last data block? */
	    if( bstate->pi->recv->bytes_received > bstate->pi->recv->bytes_total ) {
		log("binkp got too many data (%ld, %ld expected)",
			(long)bstate->pi->recv->bytes_received, (long)bstate->pi->recv->bytes_total);

		bstate->frs = frs_skipping;
		bstate->pi->recv->status = FSTAT_REFUSED;
		p_rx_fclose(bstate->pi);
		return -1;
	    }
	    else if( bstate->pi->recv->bytes_received == bstate->pi->recv->bytes_total ) {
		log("receive completed");
		bstate->frs = frs_nothing;
		bstate->pi->recv->status = FSTAT_SUCCESS;
		if( !p_rx_fclose(bstate->pi) ) {
			bstate->extracmd[0] = BPMSG_GOT;
			sprintf(bstate->extracmd+1, "%s %ld %ld",
					bstate->pi->recv->net_name, (long)bstate->pi->recv->bytes_total,
					(long)bstate->pi->recv->mod_time);
			bstate->extraislast = false;
			return 1;
		}
		else {
		    log("some error committing file");
		    bstate->extracmd[0] = BPMSG_SKIP;
		    sprintf(bstate->extracmd+1, "%s %ld %ld",
								bstate->pi->recv->net_name, (long)bstate->pi->recv->bytes_total,
								(long)bstate->pi->recv->mod_time);
		    bstate->extraislast = false;
		    return 1;
		}
	    } else {
	        log("data block accepted");
	        return 1;
	    }
	}
        log("never should be here");
        return -1;
default:
        log("impossible block_type");
        return -1;
    }
}


// ---- inherent code ----

void binkp_process_NUL(s_binkp_sysinfo *remote_data, char *buffer)
{
	char *p, *q;
	
	if( strncmp(buffer, "SYS ", 4) == 0 )
		strnxcpy(remote_data->systname, buffer+4, sizeof(remote_data->systname));
	else if( strncmp(buffer, "ZYZ ", 4) == 0 )
		strnxcpy(remote_data->sysop, buffer+4, sizeof(remote_data->sysop));
	else if( strncmp(buffer, "LOC ", 4) == 0 )
		strnxcpy(remote_data->location, buffer+4, sizeof(remote_data->location));
	else if( strncmp(buffer, "PHN ", 4) == 0 )
		strnxcpy(remote_data->phone, buffer+4, sizeof(remote_data->phone));
	else if( strncmp(buffer, "NDL ", 4) == 0 )
		strnxcpy(remote_data->flags, buffer+4, sizeof(remote_data->flags));
	else if( strncmp(buffer, "TIME ", 5) == 0 )
		strnxcpy(remote_data->timestr, buffer+5, sizeof(remote_data->timestr));
	else if( strncmp(buffer, "OPT ", 4) == 0 )
	{
		if( *remote_data->opt )
		{
			strnxcat(remote_data->opt, " ", sizeof(remote_data->opt));
			strnxcat(remote_data->opt, buffer+4, sizeof(remote_data->opt));
		}
		else
			strnxcpy(remote_data->opt, buffer+4, sizeof(remote_data->opt));
		
		binkp_parse_options(remote_data, buffer+4);
	}
	else if( strncmp(buffer, "VER ", 4) == 0 )
	{
		/* <mailer> [<protocol>/<vermaj>.<vermin>] */
		if( (p = strchr(buffer+4, ' ')) )
		{
			strnxcpy(remote_data->progname, buffer+4,
					MIN(sizeof(remote_data->progname), p - (buffer+4) + 1));
			++p;
			if( (q = strchr(p, '/')) )
			{
				strnxcpy(remote_data->protname, p,
						MIN(sizeof(remote_data->protname), q - p + 1));
				sscanf(q+1, "%d.%d",
						&remote_data->majorver,
						&remote_data->minorver);
			}
			if ((remote_data->majorver * 100 + 
			     remote_data->minorver)> 100)
			     remote_data->options |= BINKP_OPT_MB;
		}
		else
			strnxcpy(remote_data->progname, buffer+4, sizeof(remote_data->progname));
	}
	else
		log("BinkP NUL: \"%s\"", string_printable(buffer)); // NUL cannot be invalid as it is optional info
}

void binkp_process_ADR(s_binkp_sysinfo *remote_data, char *buffer)
{
	s_faddr addr;
	char *p, *q;
	
	for( p = string_token(buffer, &q, NULL, 0); p; p = string_token(NULL, &q, NULL, 0) )
	{
		if( ftn_addrparse(&addr, p, FALSE) )
			log("BinkP got unparsable address \"%s\"", string_printable(p));
		else
			session_addrs_add(&remote_data->addrs, &remote_data->anum, addr);
	}
}

int binkp_auth_incoming(s_binkp_sysinfo *remote_data)
{
	if( remote_data->challenge_length > 0
	 && strncmp(remote_data->passwd, "CRAM-MD5-", 9) == 0 )
	{
	        log("md5 auth addrs %s", remote_data->addrs);
	        log("md5 auth anum %d", remote_data->anum);
	        log("md5 auth passwd %s", remote_data->passwd + 9);
	        log("md5 auth challenge %s", remote_data->challenge);
	        log("md5 auth challenge len %d", remote_data->challenge_length);
		return session_addrs_check(remote_data->addrs,
		                           remote_data->anum,
		                           remote_data->passwd + 9,
		                           remote_data->challenge,
		                           remote_data->challenge_length);
	}
	
	log("plain-text auth");
	return session_addrs_check(remote_data->addrs, remote_data->anum,
	                           remote_data->passwd, NULL, 0);
}

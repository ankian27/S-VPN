#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>
#include <netinet/in.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/time.h>
#include <time.h>
#include "svpn_client.h"
#include "crypt.h"
#include "svpn_fd.h"

//#define MY_BUFFER_LEN 2
//#define BUFFER_LEN	4096
#define PRINT 0
#define BUFFER_LEN    64400//102200
#define TIMEOUT_USEC  100
#define ACC_TIME      10000000 //nanoseconds
#define ENCRYPT 0
#define BUFFSIZE   42
#define DYN 1

static void svpn_sig_handler(int sig) {
	char buffer[] = "Signal?\n";
	write(1, buffer, strlen(buffer));
}

float diff2float(struct timespec *start, struct timespec *end)
{
	float f;

	struct timespec temp;

	if ((end->tv_nsec - start->tv_nsec)<0) {
		temp.tv_sec = end->tv_sec - start->tv_sec-1;
		temp.tv_nsec = 1000000000 + end->tv_nsec - start->tv_nsec;
	} else {
		temp.tv_sec = end->tv_sec - start->tv_sec;
		temp.tv_nsec = end->tv_nsec - start->tv_nsec;
	}

	f = temp.tv_sec + (temp.tv_nsec / 1000000000.0);

	return f;
}


void pad_buf(char *buff,int len ){
	int si = BUFFER_LEN - len ;
	
	if(si<6){
		//int i;
		//for(int i=0;i<si;i++){
		//	buff[i]='0';
	return;	
	
	}
	
	else{
		uint32_t *lenmemloc=(uint32_t *)&(buff[0]);
		*lenmemloc=si-4;
		buff[4]='P';
		//int i;
		//for(i=0;i<si-5;i++){
		//	buff[5+i]='0';
		//}

	}
}
//tcp connect call

int svpn_handle_thread(struct svpn_client* pvoid) {
	struct svpn_client *psc = pvoid;
	int MY_BUFFER_LEN = 60;
	struct sockaddr_in addr;
	socklen_t alen = sizeof(addr);
	unsigned char buffer[BUFFER_LEN], tmp_buffer[BUFFER_LEN], s_tmp_buffer[BUFFER_LEN], n_tmp_buffer[BUFFER_LEN];
	struct timeval timeout;
	long long acc;
	fd_set fd_list, fd_list2, fd_list3;
	int maxfd = (psc->sock_fd > psc->tun_fd) ? psc->sock_fd : psc->tun_fd;
	int te, ret;
	uint32_t len;
	int recvc = 0, sendc = 0;
	int seed = time(NULL); /* get current time for seed */
	struct timespec start, end1, end2, end3, end4;
	float elapsed;
	maxfd++;
	int j=0, dyn_len;
	int maxfd2 = psc->tun_fd + 1;
	uint32_t *lenmemloc;
	int rem_offset, pack_flag, rem_len, valid_length;
	uint32_t *total_len, *pad_len;
	int len_buf[MY_BUFFER_LEN];

//	tv.tv_sec = 1;
//	tv.tv_usec = 0;
	connect (psc->sock_fd, (struct sockaddr*)&(psc->server_addr), sizeof(psc->server_addr));

	while(1) {
        
		FD_ZERO(&fd_list);
		FD_SET(psc->tun_fd, &fd_list);
		FD_SET(psc->sock_fd, &fd_list);
		
                timeout.tv_sec=0;
                timeout.tv_usec=TIMEOUT_USEC;
		//printf("Came to line number %d \n", __LINE__);

		ret = select(maxfd, &fd_list, NULL, NULL, &timeout);
		if(ret < 0) {
			if(errno == EINTR)
				return 0;
			continue;
		}
		//printf("Came to line number %d \n", __LINE__);

		if(FD_ISSET(psc->tun_fd, &fd_list)) {
            
			int bc=0;
			uint32_t tlen=0;
 			int ind=0;
			memset(tmp_buffer,'0',BUFFER_LEN );
			memset(buffer,'0',BUFFER_LEN );
			clock_gettime(CLOCK_REALTIME, &start);
			acc = 0;

		//while(tlen <= BUFFER_LEN-12){
			printf("Came to line number %d \n", __LINE__);
			for(bc=1;bc<=BUFFSIZE;bc++){
 	
				//printf("Came to line number %d \n", __LINE__);
				if(acc<=ACC_TIME){
					FD_ZERO(&fd_list2);
					FD_SET(psc->tun_fd, &fd_list2);
					timeout.tv_sec=0;
					timeout.tv_usec=TIMEOUT_USEC;
					ret = select(maxfd2, &fd_list2, NULL, NULL, &timeout);
					if(ret < 0) {
						if(errno == EINTR)
							return 0;
 							continue;
						//break;
					}
			
					if(ret==0){
						//printf("\nselect continue\n");
						clock_gettime(CLOCK_REALTIME, &end1);
						elapsed = diff2float(&start, &end1);
						acc =  1000000000*elapsed;
						printf("\naccumulated time = %lld\n",acc);
						fflush(stdout);

						if(!DYN)
							continue;
						else
							break;
					}
					
					printf("Then HERE\n");
					fflush(stdout);
					printf("Came to line number %d \n", __LINE__);
					//clock_gettime(CLOCK_REALTIME, &start);
					
					
					if(FD_ISSET(psc->tun_fd, &fd_list2)) {
					printf("Came to line number %d \n", __LINE__);
					printf("\nJUST BEFORE READ\n")	;
					

					len = read(psc->tun_fd, &tmp_buffer[8] + tlen + 4, BUFFER_LEN-tlen-12); //12 because 8 bytes from initial and 4 additional for keeping the length
					//acc = acc + timeout.tv_usec;
					printf("\naccumulated time = %lld\n",acc);
					fflush(stdout);
					//}
					//else continue;
			
					clock_gettime(CLOCK_REALTIME, &end1);
					elapsed = diff2float(&start, &end1);
					printf("elapsed time for buffer iteration %d : %f\n ", bc, elapsed);
					fflush(stdout);
					printf("timeout %ld : %ld ",timeout.tv_sec,timeout.tv_usec);
					fflush(stdout);
					printf("\nlength-%d--\n",len);
					
					lenmemloc = (uint32_t *)&(tmp_buffer[8+tlen]);
					*lenmemloc = len;
					tlen = tlen + len + 4; 
					printf("\nREACHES HERE!!!!!!!!!!!!!\n");
					fflush(stdout);
					//acc = acc + timeout.tv_usec;
					acc =  1000000000*elapsed;
					printf("HHHHHHHHHH\n");
					fflush(stdout);
					}
					//breaking from the if FD_ISSET()
					else 
						break;
					//}
					//else break;
                }
					
				else{
					printf("\nacc = %lld\n",acc );
					fflush(stdout);
					break;

				}
					
			//}
		}
		printf("\ntlen = %d\n",tlen);
		fflush(stdout);
			//Adding the total length before the buffer contents
		
		uint32_t *totallen= (uint32_t *)&(tmp_buffer[0]);
		//*totallen=tlen;
		//len=tlen+4;
		uint32_t *pad= (uint32_t *)&(tmp_buffer[4]);
		*totallen = tlen;

		
		if(DYN){
			*pad = (acc/1000) - tlen;
		}
		else
			*pad = BUFFER_LEN - tlen;
		//len=tlen+8;
		//pad_buf(tmp_buffer,len );
		
		//memcpy(buffer,tmp_buffer,BUFFER_LEN);
		//printf("outoutoutout");
		//fflush(stdout);
	             //len = read(psc->tun_fd, tmp_buffer+tlen, BUFFER_LEN);
                      //printf("\nlength-%d--\n",len);
                      //tlen=tlen+len;
                      //tmp_buffer[ind]=len;
                      //ind=tlen;
                       // }
                      //len=tlen;
		if(DYN){
			len = 8 + tlen + *pad;
			dyn_len = len;
		}
		else
			len = BUFFER_LEN;

		printf("\nlength-%d--\n",len);

		if (len < 0 || len > BUFFER_LEN)
				continue;

		sendc += len;

//			printf("send : %d total:%d\n", len, sendc);
 		if(ENCRYPT)
			Encrypt(&(psc->table), tmp_buffer + 8, buffer + 8, len - 8);
                     // printf("qwe--%s--\n",buffer);
		else
			memcpy(buffer,tmp_buffer,len);



			clock_gettime(CLOCK_REALTIME, &end2);
			elapsed = diff2float(&end1, &end2);
			printf("elapsed time after memcpy tmp_buf to buf : %f\n", elapsed);
			fflush(stdout);

            if (PRINT){                 
			int ccc;
 			for(ccc=0;ccc<len;ccc++) {
				printf("%c ",buffer[ccc]);
				if (ccc % 48 == 0) printf("\n");
			}
			}
			clock_gettime(CLOCK_REALTIME, &end3);
			elapsed = diff2float(&end2, &end3);
			printf("elapsed time after printing : %f\n", elapsed);
			fflush(stdout);


//			len = sendto(psc->sock_fd, buffer, len, 0,
//					(struct sockaddr*)&(psc->server_addr), sizeof(psc->server_addr));

			//len = sendto(psc->sock_fd, buffer, len, 0,
			//	(struct sockaddr*)&(psc->server_addr), sizeof(psc->server_addr));
			signal(SIGPIPE, SIG_IGN);
			int sent_len=0,readlen=1400;
			printf("Came to line number %d \n", __LINE__);

			if(!DYN){
				while(sent_len<BUFFER_LEN){
				printf("Came to line number %d \n", __LINE__);
				len=send(psc->sock_fd, buffer + sent_len , BUFFER_LEN - sent_len, MSG_NOSIGNAL);

					if(PRINT){
						int ccc;
						for(ccc=0;ccc<len;ccc++) {
							printf("%c ",buffer[sent_len+ccc]);
							if (ccc % 28 == 0) printf("\n");
						}
						printf("\n\n");
						fflush(stdout);
					}
						
						sent_len = sent_len + len;
						//printf("\nmsg sent successfully");
						//fflush(stdout);
				
				//else
				//	{printf("Error sending msg: %s\n", strerror(errno));}
				}
			}	
			else{
				while(sent_len<dyn_len){
				printf("Came to line number %d \n", __LINE__);
				len=send(psc->sock_fd, buffer + sent_len , dyn_len - sent_len, MSG_NOSIGNAL);

					if(PRINT){
						int ccc;
						for(ccc=0;ccc<len;ccc++) {
							printf("%c ",buffer[sent_len+ccc]);
							if (ccc % 28 == 0) printf("\n");
						}
						printf("\n\n");
						fflush(stdout);
					}
						
						sent_len = sent_len + len;
						//printf("\nmsg sent successfully");
						//fflush(stdout);
				
				//else
				//	{printf("Error sending msg: %s\n", strerror(errno));}
				}
			}
		printf("Came to line number %d \n", __LINE__);


			clock_gettime(CLOCK_REALTIME, &end4);
			elapsed = diff2float(&end3, &end4);
			printf("elapsed time after sending : %f\n", elapsed);
			fflush(stdout);
			printf("\nlength after sendto %d\n", sent_len);
			printf("\n-------------------------------------------------\n");
			fflush(stdout);
			
			if(len <= 0) {
				printf("non-blocked, drop the packet\n");
			}
		
	
		}

		memset(s_tmp_buffer,'0', BUFFER_LEN);


		if(FD_ISSET(psc->sock_fd, &fd_list)) {
			printf("Came to line number %d \n", __LINE__);
			rem_len = BUFFER_LEN;
			rem_offset=0;
			pack_flag = 0;
			while(rem_len > 0){

				if(pack_flag == 0)
					len = recv(psc->sock_fd, tmp_buffer, 8, 0);
				else
					len = recv(psc->sock_fd, tmp_buffer, rem_len, 0);


				if(pack_flag == 0){
					total_len =   (uint32_t *)&tmp_buffer[0];
					pad_len   =   (uint32_t *)&tmp_buffer[4];
					//int valid_length = *total_len + 4;
					valid_length = *total_len + 8;

					rem_len = *total_len + *pad_len;

					 if(valid_length <= len){
						//printf("Came to line number %d \n", __LINE__);
						memcpy(s_tmp_buffer, tmp_buffer + 8, (*total_len));
						//Processing it now---------------------------------------------------------------
						//done processing-----------------------------------------------------------------
						rem_offset  = rem_offset  + len;
						//++++++++++++++++++++++++++++++change according to pad len
						rem_len = rem_len - len;
						//printf("Came to line number %d \n", __LINE__);
					}

					else{
						valid_length = valid_length - len;	
						rem_offset  = rem_offset  + len;
						//++++++++++++++++++++++++++++++change according to pad len
						rem_len = rem_len - len;
						//printf("Came to line number %d \n", __LINE__);
						
						while(valid_length > 0){
							//len= recvfrom(psc->sock_fd, tmp_buffer+rem_offset, valid_length, 0, (struct sockaddr*)&addr, &alen);
							//if(FD_ISSET(psc->sock_fd, &fd_list)){
						
							len = recv(psc->sock_fd, tmp_buffer+rem_offset, pack_len, 0);
						
							valid_length = valid_length - len;
							rem_offset   = rem_offset   + len;
							//++++++++++++++++++++++++++++++change according to pad len
							rem_len = rem_len - len;
							//printf("Came to line number %d \n", __LINE__);
						}
						//memcpy(t_tmp_buffer,tmp_buffer+4,(*total_len));
						memcpy(s_tmp_buffer,tmp_buffer + 8,(*total_len));
					}

					pack_flag=1;

					int tlen = 0;
					int ind = 0;       			                                                				                 				 
					
					//Removing the length info from the buffer-------------------------
					for(te = 0 ; te <= MY_BUFFER_LEN - 1 ; te++){
						
						if(s_tmp_buffer[ind]=='0'){
							break;
						}
						uint32_t *c_len  = (uint32_t *) &(s_tmp_buffer[ind]);
						uint32_t cur_len = *c_len;	
						memcpy(n_tmp_buffer + tlen, s_tmp_buffer + ind + 4, cur_len);
						//*c_len=(uint32_t *) &( t_tmp_buffer[ind] );
						len_buf[te] = *c_len;
							
						//DEBUG_PRINT____________________________________________________
						printf("\n The %d packet length is %d\n  ", te , len_buf[te]);
						fflush(stdout);
						//_______________________________________________________________
										
						ind = ind + len_buf[te] + 4;
						tlen = tlen + len_buf[te];
											 		
					}

					len = tlen;
					
					if (len < 0 || len > BUFFER_LEN)
						continue;

					recvc += len;

					//printf("recv : %d total:%d\n", len, recvc);
            		if(ENCRYPT)  
			    		Decrypt(&(psc->table), tmp_buffer, buffer, len);
            		else
                		memcpy(buffer,n_tmp_buffer,len);

            		printf("----received----\n");
			
					//if (buffer[0] >> 4 != 4)
					//	continue;
					int var, incr=0, ller;
					
					for(var = 0; var < te; var++){
						printf("Came to line number %d \n", __LINE__);
						ller = write(psc->tun_fd, buffer+incr, len_buf[var]);
						incr = incr + ller;
					}




				}

				else{
					printf("Came to line number %d \n", __LINE__);
					rem_offset  = rem_offset  + len;
					//++++++++++++++++++++++++++++++change according to pad len
					rem_len     = rem_len   - len;
					continue;
				}
			
			//len = write(psc->tun_fd, buffer, len);

			//printf("Came to line number %d \n", __LINE__);
			
			}

			if (buffer[0] >> 4 != 4)
				continue;

		}
		//printf("Came to line number %d \n", __LINE__);

	}
	return 0;
}

struct svpn_client *svpn_init(char *addr, unsigned short port,
		unsigned char *md5, long long timestamp) {

	struct svpn_client *psc = (struct svpn_client*) malloc(sizeof(struct svpn_client));
	struct sigaction sact;
	memset(&sact, 0, sizeof(struct sigaction));
	memset(psc, 0, sizeof(struct svpn_client));

	BuildTable(&(psc->table), md5, timestamp);


	// init socket
	if(svpn_sock_create(psc, addr, port) < 0) {
		free(psc);
		psc = NULL;
		goto out;
	}


	// init tunnel
	psc->tun_fd = svpn_tun_create(psc->dev_name);
	if(psc->tun_fd < 0) {
		close(psc->sock_fd);
		free(psc);
		psc = NULL;
		goto out;
	}


	//signal(SIGUSR1, svpn_sig_handler);
	sact.sa_handler = svpn_sig_handler;
	sact.sa_flags &= ~SA_RESTART;
	sigaction(SIGUSR1, &sact, &(psc->old_act));

out:
	return psc;
}

/*
int svpn_release(struct svpn_client *psc) {
	if(!psc) {
		return -1;
	}

	if(psc->recv_thread_on) {
		svpn_stop_recv_thread(psc);
	}
	if(psc->send_thread_on) {
		svpn_stop_send_thread(psc);
	}

	close(psc->tun_fd);
	close(psc->sock_fd);

	sigaction(SIGUSR1, &(psc->old_act), NULL);

	free(psc);
	
	return 0;
}
*/


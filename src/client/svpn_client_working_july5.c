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
#include "minicomp.h"

//#define MY_BUFFER_LEN 2
//#define BUFFER_LEN	4096
#define PRINT 0
#define DEBUG_PRINT 0
#define BUFFER_LEN    63712//64400//102200
#define TIMEOUT_USEC  100
#define ACC_TIME      10000000 //nanoseconds
#define ENCRYPT 0
#define COMPRESS 1
#define BUFFSIZE   42
#define DYN 0
#define EMPTY 1

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
	int MY_BUFFER_LEN = COMPRESS ? BUFFSIZE *3.2 : 85 ;
	struct sockaddr_in addr;
	socklen_t alen = sizeof(addr);
	unsigned char buffer[BUFFER_LEN], tmp_buffer[BUFFER_LEN], s_tmp_buffer[BUFFER_LEN ], n_tmp_buffer[BUFFER_LEN];
	struct timeval timeout;
	long long int acc = 0, acc2 = 0, comp_time, acc_avg = 0, n_iter=0, total_acc=0,total_acc2=0,total_acc3=0,acc_avg2=0,acc_avg3=0;
	fd_set fd_list, fd_list2, fd_list3;
	int maxfd = (psc->sock_fd > psc->tun_fd) ? psc->sock_fd : psc->tun_fd;
	int te, ret;
	uint32_t len;
	int recvc = 0, sendc = 0;
	int seed = time(NULL); /* get current time for seed */
	struct timespec start, end1, end2, end3, end4, comp1, comp2,sleep1,sleep2;
	float elapsed;
	maxfd++;
	int j=0, dyn_len;
	int limit = COMPRESS? 3 : 1;
	int maxfd2 = psc->tun_fd + 1;
	uint32_t *lenmemloc;
	int rem_offset, pack_flag, rem_len, valid_length, empty_flag;
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
			uint32_t tlen, main_tlen = 0;
 			int ind=0, i;
 			int comp_len;
 			uint32_t *totallen, *pad, *totallen2, *pad2;
			
			memset(buffer,'0',BUFFER_LEN );
			
			acc = 0;

		//while(tlen <= BUFFER_LEN-12){
			if(DEBUG_PRINT)
				printf("Came to line number %d \n", __LINE__);
			//loop shuru+__________________+_________+_+_+_+_+_+_+_+_+_+_+__+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+__+_+_+_+_+_+_+_++_+_+__+_+_+_+_+_+_+_+
			clock_gettime(CLOCK_REALTIME, &start);

			for(i=0; i< limit && acc <= ACC_TIME && main_tlen <= BUFFER_LEN-3712; i++){

			memset(tmp_buffer,'0',BUFFER_LEN );
			tlen = 0;
			for(bc=1; bc <= BUFFSIZE; bc++){
 	
				//printf("Came to line number %d \n", __LINE__);
				if(acc <= ACC_TIME && main_tlen <= BUFFER_LEN-3712){
					FD_ZERO(&fd_list2);
					FD_SET(psc->tun_fd, &fd_list2);
					timeout.tv_sec=0;
					timeout.tv_usec=TIMEOUT_USEC;
					ret = select(maxfd2, &fd_list2, NULL, NULL, &timeout);
					if(ret < 0) {
						if(errno == EINTR)
							return 0;
 							continue;
					}
			
					if(ret==0){
						//printf("\nselect continue\n");
						clock_gettime(CLOCK_REALTIME, &end1);
						elapsed = diff2float(&start, &end1);
						acc =  1000000000*elapsed; //convert to nanoseconds
						if(DEBUG_PRINT)
							printf("\naccumulated time = %lld\n",acc);
						fflush(stdout);
						break;
					}
										
					if(FD_ISSET(psc->tun_fd, &fd_list2)) {
						if(DEBUG_PRINT){
							printf("Came to line number %d \n", __LINE__);
							printf("\nJUST BEFORE READ\n")	;
						}	
						len = read(psc->tun_fd, &tmp_buffer[8] + tlen + 4, BUFFER_LEN-(main_tlen+12+3712)); //12 because 8 bytes from initial & 4 the length 3712 because margin
						if(DEBUG_PRINT){
							printf("\naccumulated time = %lld\n",acc);
						}
						fflush(stdout);
						
						
						if(PRINT){
							printf("elapsed time for buffer iteration %d : %f\n ", bc, elapsed);
							fflush(stdout);
							printf("timeout %ld : %ld ",timeout.tv_sec,timeout.tv_usec);
							fflush(stdout);
							printf("\nlength-%d--\n",len);
						}
						
						lenmemloc = (uint32_t *)&(tmp_buffer[8+tlen]);
						*lenmemloc = len;
						tlen = tlen + len + 4;
						main_tlen = main_tlen + len + 4; 
						//printf("\nREACHES HERE!!!!!!!!!!!!!\n");
						//fflush(stdout);
						clock_gettime(CLOCK_REALTIME, &end1);
						elapsed = diff2float(&start, &end1);
						acc =  1000000000*elapsed; //convert to nanoseconds
						//printf("HHHHHHHHHH\n");
						//fflush(stdout);
					}
					//breaking from the if FD_ISSET()
					else 
						break;
                }
					
				else{
					if(DEBUG_PRINT)
					printf("\nacc = %lld\n",acc );
					fflush(stdout);
					break;
				}
			}
		if(DEBUG_PRINT)
		printf("\ntlen = %d\n",tlen);
		fflush(stdout);
			//Adding the total length before the buffer contents
		
		totallen= (uint32_t *)&(tmp_buffer[0]);
		pad= (uint32_t *)&(tmp_buffer[4]);
		*totallen = tlen;
		*pad = BUFFER_LEN-(tlen+8);
		len = BUFFER_LEN;
		if(DEBUG_PRINT)
		printf("\nlength-%d--\n",len);

		if (len < 0 || len > BUFFER_LEN)
				continue;

		sendc += len;
		//new compressed length of the valid data
		
		if(ENCRYPT)
			Encrypt(&(psc->table), tmp_buffer + 8, buffer + 8, len - 8);
                    
		else{
			if(COMPRESS && tlen >0){
				//clock_gettime(CLOCK_REALTIME, &comp1);
				//printf("Came to line number %d \n", __LINE__);
				comp_len = minicomp(buffer + (main_tlen - tlen) + 8, tmp_buffer + 8, tlen, BUFFER_LEN-(main_tlen - tlen + 8));
				//printf("Came to line number %d \n", __LINE__);

				totallen2 = (uint32_t *)&(buffer[0]);
				pad2 = (uint32_t *)&(buffer[4]);
				*totallen2 = (main_tlen - tlen) + comp_len;
				*pad2 = BUFFER_LEN - ((main_tlen - tlen) + comp_len + 8);
			}
			else if(tlen > 0)
				memcpy(buffer,tmp_buffer,len);

			
		}
		clock_gettime(CLOCK_REALTIME, &end2);
		elapsed = diff2float(&start, &end2);
		acc =  1000000000*elapsed;
		main_tlen = COMPRESS ? *totallen2 : tlen ; 
						
	}		
		//loop yahan tak+_+_+_+__+_+_+_+_+_+_+_+_+_+_+_+__+_+_+_+_+_+_+___+_+_+_+_+_+_+_+_+_+__+_+_+_+_+_+___+_+++_+__+++_+_+___+++_+_+_+_+_+__+_+____+_+_+_+_+_+_+_+_+_
			//sending data out

			//clock_gettime(CLOCK_REALTIME, &end2);
			//elapsed = diff2float(&end1, &end2);
			//printf("elapsed time after memcpy tmp_buf to buf : %f\n", elapsed);
			//fflush(stdout);

            if (PRINT){                 
			int ccc;
 			for(ccc=0;ccc<tlen;ccc++) {
				printf("%c ",buffer[ccc]);
				if (ccc % 48 == 0) printf("\n");
			}
			}
			
			//clock_gettime(CLOCK_REALTIME, &end3);
			//elapsed = diff2float(&end2, &end3);
			signal(SIGPIPE, SIG_IGN);
			int sent_len=0,readlen=1400;

			while(sent_len<BUFFER_LEN){
			if(DEBUG_PRINT)	
			printf("Came to line number %d \n", __LINE__);
			len=send(psc->sock_fd, buffer + sent_len , BUFFER_LEN - sent_len, MSG_NOSIGNAL);
			if(DEBUG_PRINT)
			printf("Came to line number %d \n", __LINE__);

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
				
			}
		if(DEBUG_PRINT)		
		printf("Came to line number %d \n", __LINE__);


			clock_gettime(CLOCK_REALTIME, &end4);
			elapsed = diff2float(&end3, &end4);
			if(PRINT){
				printf("elapsed time after sending : %f\n", elapsed);
				fflush(stdout);
				printf("\nlength after sendto %d\n", sent_len);
				printf("\n-------------------------------------------------\n");
				fflush(stdout);
			}
			if(len <= 0) {
				printf("non-blocked, drop the packet\n");
			}
		
	
		}

		memset(s_tmp_buffer,'0', BUFFER_LEN );




		if(FD_ISSET(psc->sock_fd, &fd_list)) {
			if(DEBUG_PRINT)
			printf("Came to line number %d \n", __LINE__);
			rem_len = BUFFER_LEN;
			rem_offset=0;
			pack_flag = 0;
			empty_flag=0;
			//acc2 = 0;
			while(rem_len > 0){

				if(pack_flag == 0)
					len = recv(psc->sock_fd, tmp_buffer, 8, 0);
				else
					len = recv(psc->sock_fd, tmp_buffer, rem_len, 0);
				//printf("\n length read for padding = %d\n", len);
				//fflush(stdout);
				//printf("Came to line number %d \n", __LINE__);
				//fflush(stdout);
				if(pack_flag == 0){
					total_len =   (uint32_t *)&tmp_buffer[0];
					pad_len   =   (uint32_t *)&tmp_buffer[4];
					if(*total_len==0){
						empty_flag=1;
						if(DEBUG_PRINT)
						printf("\nEMPTY FLAG SET\n");
						fflush(stdout);
					}
					//int valid_length = *total_len + 4;
					valid_length = *total_len + 8;

					 if(valid_length <= len){
					 	if(DEBUG_PRINT)
						printf("Came to line number %d \n", __LINE__);
						memcpy(s_tmp_buffer, tmp_buffer + 8, (*total_len));
						//Processing it now---------------------------------------------------------------
						//done processing-----------------------------------------------------------------
						rem_offset  = rem_offset  + len;
						//++++++++++++++++++++++++++++++change according to pad len
						rem_len = rem_len - len;
						//printf("Came to line number %d \n", __LINE__);
						/*if(acc_avg2==0){
							//acc_avg = 500000;
							//sleep1.tv_sec=0;
							//sleep1.tv_nsec=2100000;
							//nanosleep(&sleep1,&sleep2);
						}
						else{
							//sleep1.tv_sec=0;
							//sleep1.tv_nsec=acc_avg2;
							//nanosleep(&sleep1,&sleep2);
						}*/
					}

					else{
						clock_gettime(CLOCK_REALTIME, &start);
						valid_length = valid_length - len;	
						rem_offset  = rem_offset  + len;
						//++++++++++++++++++++++++++++++change according to pad len
						rem_len = rem_len - len;
						//printf("Came to line number %d \n", __LINE__);
						
						while(valid_length > 0){
							//len= recvfrom(psc->sock_fd, tmp_buffer+rem_offset, valid_length, 0, (struct sockaddr*)&addr, &alen);
							//if(FD_ISSET(psc->sock_fd, &fd_list)){
							if(DEBUG_PRINT)
							printf("Came to line number %d \n", __LINE__);
							len = recv(psc->sock_fd, tmp_buffer+rem_offset, valid_length, 0);
							if(DEBUG_PRINT)
							printf("Came to line number %d \n", __LINE__);
						
							valid_length = valid_length - len;
							rem_offset   = rem_offset   + len;
							//++++++++++++++++++++++++++++++change according to pad len
							rem_len = rem_len - len;
							//printf("Came to line number %d \n", __LINE__);
						}
						if(COMPRESS){
							int start_offset=0,rem_total_len = *total_len, comp_chunk_size = 0 ;
							while(rem_total_len>0){
								memset(s_tmp_buffer,'0', BUFFER_LEN);
								memset(buffer,'0', BUFFER_LEN);
								comp_chunk_size = (int)(get_complen(tmp_buffer+8+start_offset))+sizeof(struct mcheader);
								minidecomp(s_tmp_buffer,tmp_buffer + 8 + start_offset, comp_chunk_size, BUFFER_LEN);
								int tlen = 0, ind = 0;
								 
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
									//printf("\n The %d packet length is %d\n  ", te , len_buf[te]);
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
			                	if(DEBUG_PRINT)
			            		printf("----received----\n");
					
								int var, incr=0, ller;
								
								for(var = 0; var < te; var++){
									if(DEBUG_PRINT)
									printf("Came to line number %d \n", __LINE__);
									ller = write(psc->tun_fd, buffer+incr, len_buf[var]);
									if(DEBUG_PRINT)
									printf("Came to line number %d \n", __LINE__);
									incr = incr + ller;
								}
								start_offset = start_offset + comp_chunk_size;
								rem_total_len = rem_total_len - comp_chunk_size;
							}
						}
						else
							memcpy(s_tmp_buffer,tmp_buffer + 8,(*total_len));

						clock_gettime(CLOCK_REALTIME, &end1);
						elapsed = diff2float(&start, &end1);
						acc2 =  1000000000*elapsed;

					}

					pack_flag=1;     			                                                				                 				 
					
					if(!empty_flag && !COMPRESS){
					//Removing the length info from the buffer-------------------------
						int tlen = 0,ind = 0;  
						clock_gettime(CLOCK_REALTIME, &start);
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
							if(DEBUG_PRINT){
								printf("\n The %d packet length is %d\n  ", te , len_buf[te]);
								fflush(stdout);
							}	
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
	                	if(DEBUG_PRINT)
	            		printf("----received----\n");
				
						//if (buffer[0] >> 4 != 4)
						//	continue;
						int var, incr=0, ller;
						
						for(var = 0; var < te; var++){
							if(DEBUG_PRINT)
							printf("Came to line number %d \n", __LINE__);
							ller = write(psc->tun_fd, buffer+incr, len_buf[var]);
							if(DEBUG_PRINT)
							printf("Came to line number %d \n", __LINE__);
							incr = incr + ller;
						}
						clock_gettime(CLOCK_REALTIME, &end1);
						elapsed = diff2float(&start, &end1);
						acc2 =  1000000000*elapsed;

					}

					/*else if(!(FD_ISSET(psc->tun_fd, &fd_list)) && empty_flag) {
						memset(buffer,'0',BUFFER_LEN );
						uint32_t *totallen = (uint32_t *)&(buffer[0]);	

						uint32_t *pad = (uint32_t *)&(buffer[4]);
						*totallen=0;
						*pad=BUFFER_LEN-8;
						int sent=0;
						
						if (PRINT){                 
							int ccc;
 							for(ccc=0;ccc<BUFFER_LEN;ccc++) {
								printf("%c ",buffer[ccc]);
								if (ccc % 48 == 0) printf("\n");
							}
						}
	
						
						while(sent<BUFFER_LEN){
							printf("Came to line number %d \n", __LINE__);
							len=send(psc->sock_fd, buffer + sent , BUFFER_LEN - sent, MSG_NOSIGNAL);
							printf("\n------sending------\n");
							fflush(stdout);
							
							if (PRINT){                 
								int ccc;
 								for(ccc=0;ccc<len;ccc++) {
									printf("%c ",buffer[sent+ccc]);
									if (ccc % 48 == 0) printf("\n");
								}
							}
							
							printf("\nLength Sent = %d\n", len );
							fflush(stdout);
							sent = sent + len;
						}	
					}*/


				}

				else{
					//printf("Came to line number %d \n", __LINE__);
					//fflush(stdout);
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
		if(n_iter==20){
			n_iter=n_iter/5;
			total_acc = total_acc/5;
			total_acc2 = total_acc2/5;
			total_acc3 = total_acc3/5;
			
		}
		else{
			n_iter++;
			total_acc = total_acc + acc + acc2;
			total_acc2 = total_acc2 + acc;
			total_acc3 = total_acc3 + acc2;
			acc_avg = total_acc/n_iter;
			acc_avg2 = total_acc2/n_iter;
			acc_avg3 = total_acc3/n_iter; 
			if(DEBUG_PRINT)
			printf("\nAVG_ACC = %lld\n",acc_avg);
			fflush(stdout);
		}
		//if(!(FD_ISSET(psc->sock_fd, &fd_list)) && !(FD_ISSET(psc->tun_fd, &fd_list)) && EMPTY){
		if(!(FD_ISSET(psc->tun_fd, &fd_list)) && EMPTY && !(FD_ISSET(psc->sock_fd, &fd_list))){
			memset(buffer,'0',BUFFER_LEN );
			uint32_t *totallen = (uint32_t *)&(buffer[0]);	
			uint32_t *pad = (uint32_t *)&(buffer[4]);
			*totallen=0;
			*pad=BUFFER_LEN-8;
			int sent=0;
			if(FD_ISSET(psc->sock_fd, &fd_list)){
				if(DEBUG_PRINT)
				printf("\nSocket is set\n");
			}
			else{
				if(DEBUG_PRINT)
				printf("\nSocket is not set\n");
			}
			int j,k=0;
			for (j=0;j<10000;j++){
				k=(((25) * 1000)-25000)/100000 + 0;
			}
			/*if(acc_avg2==0){
				//acc_avg = 2100000;
				//sleep1.tv_sec=0;
				//sleep1.tv_nsec=2100000;
				//nanosleep(&sleep1,&sleep2);
			}
			else{
				//sleep1.tv_sec=0;
				//sleep1.tv_nsec=acc_avg2;
				//nanosleep(&sleep1,&sleep2);
			}*/

			while(sent<BUFFER_LEN){
				if(DEBUG_PRINT)
				printf("Came to line number %d \n", __LINE__);
				len=send(psc->sock_fd, buffer + sent , BUFFER_LEN - sent, MSG_NOSIGNAL);
				if(DEBUG_PRINT)
				printf("Came to line number %d \n", __LINE__);

				sent = sent + len;
			}
		}


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


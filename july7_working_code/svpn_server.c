#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>
#include <netinet/in.h>
#include <signal.h>
#include <errno.h>
#include <sys/time.h>
#include <time.h>
#include "svpn_server.h"
#include "crypt.h"
#include "svpn_fd.h"
#include "svpn_net.h"
#include "util.h"
#include "minicomp.h"

//#define MY_BUFFER_LEN 4
#define BUFFER_LEN	63712 //64400  //102200
//#define BUFFER_LEN 4096
#define TIMEOUT_USEC  100
#define ACC_TIME      10000000 //nanoseconds
#define DYN 0
#define RATE_LIM 0
#define COMP_PRINT 0
#define COMPRESS 1
#define BUFFSIZE 42
#define ENCRYPT 0
#define PRINT 0
#define DEBUG_PRINT 0
#define EMPTY 1 

static void svpn_sig_handler(int sig) {
	char buffer[] = "Signal?\n";
	int tmp = write(1, buffer, strlen(buffer));
}

struct tstat
{
	long long total_send;
	long long total_recv;
	long long last_send, last_recv;
	int total_pkgsend, total_pkgrecv;
	int last_pkgsend, last_pkgrecv;
	int speed_send, speed_recv;
	long long ts_last;
	long long ts_current;
};

long long tl_last = 0;
FILE* frec = NULL;


struct tstat stat2;

int conn_fd,so_fd;

#define OPT_TH 1024 * 1024
#define TIM_TH 1000000 * 2

char fmtstr[128];

inline void fillnum(char* s, int len, int num)
{
	int lenb = len;
	s += lenb -1;
	while (lenb > 0)
	{
		if (num == 0)
			*s = ' ';
		else if ((len - lenb) % 4 == 3)
			*s = ',';
		else
		{
			*s = '0' + num % 10;
			num /= 10;
		}

		s--;
		lenb--;
	}
}

inline void fillnum64(char* s, int len, long long num)
{
        int lenb = len;
        s += lenb -1;
        while (lenb > 0)
        {
                if (num == 0)
                        *s = ' ';
                else if ((len - lenb) % 4 == 3)
                        *s = ',';
                else
                {
                        *s = '0' + num % 10;
                        num /= 10;
                }

                s--;
                lenb--;
        }
}


inline void output_info()
{
	int tval;

	if (stat2.total_send - stat2.last_send < OPT_TH &&
		stat2.total_recv - stat2.last_recv < OPT_TH &&
		stat2.ts_current - stat2.ts_last < TIM_TH)
			return;

	tval = stat2.ts_current - stat2.ts_last;
	stat2.speed_send = (stat2.total_send - stat2.last_send) * 1000000 / tval;
	stat2.speed_recv = (stat2.total_recv - stat2.last_recv) * 1000000 / tval;

	fillnum64(fmtstr + 5, 14, stat2.total_send);
	fillnum(fmtstr + 22, 9, stat2.speed_send);
	fillnum64(fmtstr + 42, 14, stat2.total_recv);
	fillnum(fmtstr + 59, 9, stat2.speed_recv);
	fprintf(stderr, "%s", fmtstr);

	stat2.ts_last = stat2.ts_current;
	stat2.last_send = stat2.total_send;
	stat2.last_recv = stat2.total_recv;
	stat2.last_pkgsend = stat2.total_pkgsend;
	stat2.last_pkgrecv = stat2.total_pkgrecv;

	if ((stat2.ts_current / 1000000) % 2 == 0)
	{
		long secc = stat2.ts_current / 1000000;
		struct tm tmp = *localtime(&secc);
		if (tl_last != 0)
		{
			if (frec == NULL)
				frec = fopen("svpn.log", "a");
			printf("Rec File %08x \n\n", frec);
			fprintf(frec, "%d.%02d.%02d %02d:%02d:%02d  %9lld\n",
							tmp.tm_year, tmp.tm_mon, tmp.tm_mday,
							tmp.tm_hour, tmp.tm_min, tmp.tm_sec,
							stat2.last_recv - tl_last);
			fflush(frec);
		}
		tl_last = stat2.last_recv;
	}

}

float diff2float(struct timespec *start, struct timespec *end)
{
		float f;

		struct timespec temp;

		if ((end->tv_nsec - start->tv_nsec)<0) {
     		temp.tv_sec = end->tv_sec - start->tv_sec-1;
	    	temp.tv_nsec = 1000000000 + end->tv_nsec - start->tv_nsec;
			} 
		else {
				temp.tv_sec = end->tv_sec - start->tv_sec;
				temp.tv_nsec = end->tv_nsec - start->tv_nsec;
				}

			f = temp.tv_sec + (temp.tv_nsec / 1000000000.0);

		return f;
}

void my_function(int sig){ // can be called asynchronously
  //flag = 1; // set flag
	close(conn_fd);
	close(so_fd);
}

int svpn_server_handle_thread(struct svpn_server* pvoid)
{
	struct svpn_server *psc = pvoid;

	struct sockaddr_in6 addr;
	socklen_t alen = sizeof(addr);
	int seed = time(NULL); /* get current time for seed */
	struct timespec start,start1, end1, end2, end3, end4, comp1, comp2,sleep1,sleep2,sleep3,sleep4;
	float elapsed;
	long long int acc = 0, acc2 = 0, comp_time, acc_avg = 0, n_iter=0, total_acc=0;
	struct timeval tv;
	struct timeval timeout;
	int MY_BUFFER_LEN = COMPRESS ? BUFFSIZE *3.2 : 85 ;
	char tmpstr[32];
	int te, init_recv_length, rem_len, dyn_len;
	char mytmp;
	int len_buf[MY_BUFFER_LEN];
	len_buf[0]=0;
	//len_buf[1]=0;
	so_fd = psc-> sock_fd;
	struct svpn_net_ipv4_header* pheader = NULL;
	unsigned char buffer[BUFFER_LEN + 1400], tmp_buffer[BUFFER_LEN+1400], n_tmp_buffer[BUFFER_LEN+1400],t_tmp_buffer[BUFFER_LEN+1400], s_tmp_buffer[BUFFER_LEN+1400];
	int ret, uid, len;
	fd_set fd_list, fd_list2;
	int limit = COMPRESS ? 3 : 1;
	uint32_t *lenmemloc;
	uint32_t *total_len, *pad_len;
	int rem_offset, pack_flag, pack_len,fd_flag,empty_flag;
	printf("buff==%s==\n",buffer);
	printf("tmpbuf==%s==\n",tmp_buffer);

	strcpy(fmtstr, "Send:99,000,000,000B [9,000,000B/s], Recv:99,000,000,000B [9,000,000B/s]\r");

	memset(&stat2, 0, sizeof(stat2));
	gettimeofday(&tv, NULL);
	stat2.ts_last = tv.tv_sec * 1000000LL + tv.tv_usec;
	printf("Came to line number %d \n", __LINE__);

	signal(SIGINT, my_function); 

	if(listen(psc->sock_fd, 10)== -1){
	printf("an error: %s\n", strerror(errno));}

	//int maxfd = (psc->sock_fd > psc->tun_fd) ? psc->sock_fd : psc->tun_fd;
	//maxfd++;
	//struct timespec start, end1, end2, end3, end4;
	conn_fd = accept (psc->sock_fd,NULL,NULL);
	printf("\nconn_fd = %d", conn_fd);
	int maxfd = (conn_fd > psc->tun_fd) ? conn_fd : psc->tun_fd;
	maxfd++;

	int maxfd2 = psc->tun_fd +1;
	while(1)
	{	
		FD_ZERO(&fd_list);
		FD_SET(psc->tun_fd, &fd_list);
		FD_SET(conn_fd, &fd_list);
		//printf("Came to line number %d \n", __LINE__);
				timeout.tv_sec=0;
                timeout.tv_usec=TIMEOUT_USEC;
	
		ret = select(maxfd, &fd_list, NULL, NULL, &timeout);
		//printf("Came to line number %d \n", __LINE__);	
		if(ret < 0)
		{
			if(errno == EINTR)
				return 0;
				continue;
		}
		//printf("Came to line number %d \n", __LINE__);
		// update statistics data----------------------------------------------
		gettimeofday(&tv, NULL);
		stat2.ts_current = tv.tv_sec * 1000000LL + tv.tv_usec;
		//--------------------------------------------------------------------
		

		init_recv_length=0;
        //memset(tmp_buffer,'0',BUFFER_LEN + 1400);
		memset(t_tmp_buffer,'0',BUFFER_LEN + 1400);
		memset(s_tmp_buffer,'0',BUFFER_LEN + 1400);
		memset(buffer,'0',BUFFER_LEN + 1400);

		rem_len = BUFFER_LEN;
		pack_len = 1400;
		fd_flag=0;
		//fflush(stdout);
		//printf("Came to line number %d \n", __LINE__);
		//fflush(stdout);
		if(FD_ISSET(conn_fd, &fd_list))
		{
			rem_offset=0;
			//clock_gettime(CLOCK_REALTIME, &start);
			pack_flag=0;
			empty_flag=0;
			if(DEBUG_PRINT)
				printf("Came to line number %d \n", __LINE__);
			while(rem_len > 0){
						if(DEBUG_PRINT){
							printf("Came to line number %d \n", __LINE__);
							//if(FD_ISSET(psc->sock_fd, &fd_list))
							printf("\nBefore the first recvfrom\n");
							fflush(stdout);
							printf("Came to line number %d \n", __LINE__);
							printf("\nREM_LEN = %d \n",rem_len);
							fflush(stdout);
						}	
						
						if(pack_flag==0)	
							len = recv(conn_fd, tmp_buffer + rem_offset, 8 , 0);
						else{
							len = recv(conn_fd, tmp_buffer + rem_offset, rem_len , 0);	
							if(DEBUG_PRINT)
								printf("\nPADDING_LENGTH_READ = %d\n", len);
						}
						if(len < 0)
							printf("an error: %s\n", strerror(errno));	
						if(DEBUG_PRINT)
						printf("\nAfter the first recvfrom\n");
						
						fflush(stdout);
			      
						//if it is reading for the first time (we want to extract all useful data here)---------------------------
							if(pack_flag==0){
								if(DEBUG_PRINT)
									printf("Came to line number %d \n", __LINE__);
							    total_len =   (uint32_t *)&tmp_buffer[0];
							    pad_len   =   (uint32_t *)&tmp_buffer[4];
								//int valid_length = *total_len + 4;
								if(*total_len == 0)
									empty_flag=1;
								int valid_length = *total_len + 8;
								if(DEBUG_PRINT)
									printf("\nvalid_length = %d\n",valid_length);								
								//if it has read the entire data and maybe some padding------------------------------------------- 
								if(valid_length <= len){
											if(DEBUG_PRINT)
												printf("Came to line number %d \n", __LINE__);
											
											memcpy(t_tmp_buffer,tmp_buffer + 8,(*total_len));
											//Processing it now---------------------------------------------------------------
										    //done processing-----------------------------------------------------------------
											rem_offset  = rem_offset  + len;
											//++++++++++++++++++++++++++++++change according to pad len
											rem_len = rem_len - len;			
											if(DEBUG_PRINT)
												printf("Came to line number %d \n", __LINE__);
								}
								//-------------------------------------------------------------------------------------------------


								//if the entire valid data has not been read-------------------------------------------------------
							//	printf("Came to line number %d \n", __LINE__);
								else{
											clock_gettime(CLOCK_REALTIME, &start);
											valid_length = valid_length - len;	
											rem_offset  = rem_offset  + len;
											//++++++++++++++++++++++++++++++change according to pad len
											rem_len = rem_len - len;
											if(DEBUG_PRINT)
												printf("Came to line number %d \n", __LINE__);
											while(valid_length > 0){
													//len= recvfrom(psc->sock_fd, tmp_buffer+rem_offset, valid_length, 0, (struct sockaddr*)&addr, &alen);
													//if(FD_ISSET(psc->sock_fd, &fd_list)){
													//printf("\nBefore the second recvfrom\n");	
													len = recv(conn_fd, tmp_buffer+rem_offset, valid_length, 0);
													//printf("\nAfter the second recvfrom\n");
													valid_length = valid_length - len;
													rem_offset   = rem_offset   + len;
													//++++++++++++++++++++++++++++++change according to pad len
													rem_len = rem_len - len;
													//}
													//printf("Came to line number %d \n", __LINE__);
											}
											if(COMPRESS){
												int start_offset=0,rem_total_len = *total_len, comp_chunk_size = 0;
												while(rem_total_len>0){
													memset(t_tmp_buffer,'0', BUFFER_LEN);
													memset(buffer,'0', BUFFER_LEN);
													clock_gettime(CLOCK_REALTIME, &start1);
													
													comp_chunk_size =  (int)(get_complen(tmp_buffer+8+start_offset))+sizeof(struct mcheader);
													minidecomp(t_tmp_buffer,tmp_buffer + 8 + start_offset, comp_chunk_size, BUFFER_LEN);
													
													clock_gettime(CLOCK_REALTIME, &end3);
													elapsed = diff2float(&start1, &end3);
													if(COMP_PRINT){
														printf("elapsed time for minidecomp %f\n ",  elapsed);
														fflush(stdout);
													}
													if(ENCRYPT)
											    		uid = t_tmp_buffer[0];
													else{ 
										    			int tlen = 0, ind = 0;
							                                            				                 				 
											    		//Removing the length info from the buffer-------------------------
						                                for(te = 0 ; te <= MY_BUFFER_LEN - 1 ; te++){
														  	if(t_tmp_buffer[ind]=='0'){
														     	break;
														   	 }
															uint32_t *c_len  = (uint32_t *) &(t_tmp_buffer[ind]);
														 	uint32_t cur_len = *c_len;	
														 	memcpy(n_tmp_buffer + tlen, t_tmp_buffer + ind + 4, cur_len);
														 	//*c_len=(uint32_t *) &( t_tmp_buffer[ind] );
														 	len_buf[te] = *c_len;
														
														 	//DEBUG_PRINT____________________________________________________
														 	if(DEBUG_PRINT)
															printf("\n The %d packet length is %d\n  ", te , len_buf[te]);
														 	fflush(stdout);
														 	//_______________________________________________________________
														
															ind = ind + len_buf[te] + 4;
														 	tlen = tlen + len_buf[te];
														 		
														}
														//Done Removing it---------------------------------------------------
												         		
														len = tlen;
														mytmp=n_tmp_buffer[0];
														uid=188;
														n_tmp_buffer[0]=(char)uid;
													}
													
													if (ENCRYPT) 
	                                               		Decrypt(&(psc->clients[uid]->table), n_tmp_buffer, buffer, len);
													else 
					                   					memcpy(buffer, n_tmp_buffer, len);
				               						
				               						buffer[0]=mytmp;

				               						int var, incr=0,ller;
													for(var=0;var<te;var++){
														if(DEBUG_PRINT)
															printf("Came to line number %d \n", __LINE__);
											    		ller = write(psc->tun_fd, buffer+incr, len_buf[var]);
											   			if(DEBUG_PRINT)
															printf("Came to line number %d \n", __LINE__);
														incr=incr+ller;
													}
													start_offset = start_offset + comp_chunk_size;
													rem_total_len = rem_total_len - comp_chunk_size;
												}
											}
											else{
												clock_gettime(CLOCK_REALTIME, &start1);
												memcpy(t_tmp_buffer,tmp_buffer + 8,(*total_len));		
												clock_gettime(CLOCK_REALTIME, &end3);
												elapsed = diff2float(&start1, &end3);
												if(COMP_PRINT){
													printf("elapsed time for memcpy/d %f\n ",  elapsed);
													fflush(stdout);
												}
											}	
											clock_gettime(CLOCK_REALTIME, &end1);
											elapsed = diff2float(&start, &end1);
											acc2 =  1000000000*elapsed;
								}
								//--------------------------------------------------------------------------------------------------
								//if
								pack_flag=1;
							    //CODE TO WRITE IT TO THE TUN
								if(DEBUG_PRINT)	
								printf("Came to line number %d \n", __LINE__);

								             //ignore-------------------------------------------------------
											 //printf("\n the src_addr is %s \n",addr.sa_data);
											 //clock_gettime(CLOCK_REALTIME, &end1);
											 //elapsed = diff2float(&start, &end1);
											 if(DEBUG_PRINT){
												 printf("\nThe elapsed time after receiving : %f \n", elapsed);
												 fflush(stdout);
												 printf("\nlen=%d\n", len);
												 fflush(stdout);
											  }	 
											 if(len <= 0 || len > BUFFER_LEN)
											 continue;
											 //endignore-----------------------------------------------------
											                                                                                                                                        
										if(!(empty_flag) && !COMPRESS){                                                                                                                              
											 //manually overwriting the tmp_buffer[0] value 
											if(ENCRYPT)
											    uid = t_tmp_buffer[0];
											else{ 
										    int tlen = 0, ind = 0;
							                clock_gettime(CLOCK_REALTIME, &start);                            				                 				 
								    		//Removing the length info from the buffer-------------------------
			                                for(te = 0 ; te <= MY_BUFFER_LEN - 1 ; te++){
											  	if(t_tmp_buffer[ind]=='0'){
											     	break;
											   	 }
												uint32_t *c_len  = (uint32_t *) &(t_tmp_buffer[ind]);
											 	uint32_t cur_len = *c_len;	
											 	memcpy(n_tmp_buffer + tlen, t_tmp_buffer + ind + 4, cur_len);
											 	//*c_len=(uint32_t *) &( t_tmp_buffer[ind] );
											 	len_buf[te] = *c_len;
											
											 	//DEBUG_PRINT____________________________________________________
											 	if(DEBUG_PRINT)
												printf("\n The %d packet length is %d\n  ", te , len_buf[te]);
											 	fflush(stdout);
											 	//_______________________________________________________________
											
												ind = ind + len_buf[te] + 4;
											 	tlen = tlen + len_buf[te];
											 		
											}
											//Done Removing it---------------------------------------------------
									         		
											len = tlen;
											mytmp=n_tmp_buffer[0];
											uid=188;
											n_tmp_buffer[0]=(char)uid;
										}
											 
											  
											 //DEBUG_PRINT__________________________________________________________
											if(DEBUG_PRINT) 
												printf("\nbefore--tempbuf--%c--uid--%d--beforeend\n",n_tmp_buffer[0],uid);
											fflush(stdout);
											 //_____________________________________________________________________

											 //DEBUG_DEFAULT________________________________________________________
											 if (psc->clients[uid] == NULL)
											    {
											     printf("tempbuf--%c--uid--%d--\n",n_tmp_buffer[0],uid);
											     mprintf(LWARN, "Unknown user #%d\n", uid);
											     continue;
											     }
											 //______________________________________________________________________
											
											if (ENCRYPT) {
	                                               Decrypt(&(psc->clients[uid]->table), n_tmp_buffer, buffer, len);
	              							}
						    
						              		else {
					                   				memcpy(buffer, n_tmp_buffer, len);
				               					}
											
						 					//DEBUG_PRINT________________________________________________________________
											if(DEBUG_PRINT)
											printf("\nbuffer before typecast\n");
						              		fflush(stdout);					
											//___________________________________________________________________________
											
											//update the first index with the original value
											buffer[0]=mytmp;
											
											//DEBUG_PRINT________________________________________________________________
											if(PRINT){
											 	int ccd;
									         	for(ccd=0;ccd<len;ccd++){
													printf("%c ",buffer[ccd]);
											  		if(ccd % 16 == 0) printf("\n");
								             			fflush(stdout);
												}
											}			 
											//___________________________________________________________________________
									
									   
								            pheader = (struct svpn_net_ipv4_header*)buffer;
								           
										   	if (pheader->src_ip[0] != psc->local_addr[0] ||
								                pheader->src_ip[1] != psc->local_addr[1] ||
								                pheader->src_ip[2] != psc->local_addr[2] ||
								                pheader->src_ip[3] != uid)
								             {
								                 printf("pheader->src_ip[0] = %d != psc->local_addr[0] = %d\n", pheader->src_ip[0], psc->local_addr[0]);
								                 printf("pheader->src_ip[1] = %d != psc->local_addr[1] = %d\n", pheader->src_ip[1], psc->local_addr[1]);
								                 printf("pheader->src_ip[2] = %d != psc->local_addr[2] = %d\n", pheader->src_ip[2], psc->local_addr[2]);
								                 printf("pheader->src_ip[3] = %d != uid = %d\n", pheader->src_ip[3], uid);
								                 printf("psc->local_addr[3] = %d\n", psc->local_addr[3]);
								
												 mprintf(LWARN, "Invalid password");
								             //  mprintf(LWARN, "Invalid password : %s", inet_ntop(PF_INET6, (char*)&addr + 8, tmpstr, sizeof(tmpstr)));
								                 printf("about to continue...\n");
								                 continue;
								             }
								 
								             if (memcmp(&addr, &psc->clients[uid]->addr, sizeof(addr)) != 0)
								             {
								                memcpy(&psc->clients[uid]->addr, &addr, sizeof(addr));
								                mprintf(LINFO, "Client #%d move to ???", uid);
								                //mprintf(LINFO, "Client #%d move to %s", uid, inet_ntop(PF_INET6, (char*)&addr + 8, tmpstr, sizeof(tmpstr)));
								             }
								
											 stat2.total_send += len;
								             stat2.total_pkgsend++;
								             //output_info();
								             

											int var, incr=0,ller;
											for(var=0;var<te;var++){
												if(DEBUG_PRINT)
													printf("Came to line number %d \n", __LINE__);
											    ller = write(psc->tun_fd, buffer+incr, len_buf[var]);
											    if(DEBUG_PRINT)
													printf("Came to line number %d \n", __LINE__);
												incr=incr+ller;
											}
	                                          
											//TIMING_CODE_____________________________________________________________________________
											clock_gettime(CLOCK_REALTIME, &end1);
											elapsed = diff2float(&start, &end1);
											acc2 =  1000000000*elapsed;
											//_______________________________________________________________________________________
										}
										else if(!(FD_ISSET(psc->tun_fd, &fd_list)) && empty_flag){
												memset(buffer,'0',BUFFER_LEN );
											    uint32_t *totallen = (uint32_t *)&(buffer[0]);
											    uint32_t *pad = (uint32_t *)&(buffer[4]);
											    *totallen=0;
											 	*pad=BUFFER_LEN-8;
												int sent=0;
												if(FD_ISSET(conn_fd, &fd_list)){
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

												/*if(acc_avg==0){
													//sleep1.tv_sec = 0;
													//sleep1.tv_nsec = ;
													//nanosleep(&sleep1,&sleep2);
												}*/
												//else{
													sleep1.tv_sec = 0;
													sleep1.tv_nsec = acc_avg;
													nanosleep(&sleep1,&sleep2); 
												//}
												if(DEBUG_PRINT)
												printf("Came to line number %d \n", __LINE__);
												while(sent < BUFFER_LEN){
													if(DEBUG_PRINT)
														printf("Came to line number %d \n", __LINE__);
													len=send(conn_fd, buffer + sent , BUFFER_LEN - sent, MSG_NOSIGNAL);
													if(DEBUG_PRINT)
														printf("Came to line number %d \n", __LINE__);
													sent = sent + len;
												}
										}
											 
						
							}
						else{
							if(DEBUG_PRINT)
								printf("Came to line number %d \n", __LINE__);
							 rem_offset  = rem_offset  + len;
							 //++++++++++++++++++++++++++++++change according to pad len
							 rem_len     = rem_len     - len;
							 
							 continue;
						}
					//----------------------------------------------------------------------------------------------------------------------
				
				//	else 
				//	   break;
				if(DEBUG_PRINT)	
					printf("Came to line number %d \n", __LINE__);
			}
			//init_recv_length=BUFFER_LEN;
			if(DEBUG_PRINT) 
			printf("Came to line number %d \n", __LINE__);
		
		}//The main if ends here "if(FD_ISSET(psc->sock_fd, &fd_list))"


		if(FD_ISSET(psc->tun_fd, &fd_list))
		{	if(DEBUG_PRINT)
			printf("BEFORE READ\n");

			int bc=0;
			uint32_t tlen, main_tlen = 0;
			int comp_len;
 			int ind = 0, i;
 			uint32_t *totallen, *pad, *totallen2, *pad2;
			memset(t_tmp_buffer,'0',BUFFER_LEN + 1400);
			memset(s_tmp_buffer,'0',BUFFER_LEN + 1400);
			memset(buffer,'0',BUFFER_LEN + 1400);
			
			acc = 0;
			clock_gettime(CLOCK_REALTIME, &start);
			//LOOP SHURU+_+_+_+_+_+_+_+_+__+_+_+_+__++_+_+_+_++_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+__+_+_+_+_+_+_+_+_
			for(i=0; i<limit && acc <= ACC_TIME && main_tlen <= BUFFER_LEN-3712 ; i++){
				
				memset(tmp_buffer,'0',BUFFER_LEN );
				tlen = 0;

				for(bc=1;bc<=BUFFSIZE;bc++){
					
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
							//break;
						}
				
						if(ret==0){
							//printf("\nselect continue\n");
							clock_gettime(CLOCK_REALTIME, &end1);
							elapsed = diff2float(&start, &end1);
							acc =  1000000000*elapsed;
							if(DEBUG_PRINT)
							printf("\naccumulated time = %lld\n",acc);
							fflush(stdout);
							break;
							//continue;

						}

						if(FD_ISSET(psc->tun_fd, &fd_list2)) {
							if(DEBUG_PRINT){
								printf("Came to line number %d \n", __LINE__);
								printf("\nJUST BEFORE READ\n")	;
							}
								len = read(psc->tun_fd, &tmp_buffer[8] + tlen + 4, BUFFER_LEN-tlen-12); //12 because 8 bytes from initial and 4 additional for keeping the length
							//acc = acc + timeout.tv_usec;
							//printf("\nlength read from the tunnel %d\n",len);
							if(DEBUG_PRINT)
							printf("\naccumulated time = %lld\n",acc);
							fflush(stdout);
			
							//else continue;
					
							//clock_gettime(CLOCK_REALTIME, &end1);
							//elapsed = diff2float(&start, &end1);
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
						   	if(DEBUG_PRINT)	
							printf("\nREACHES HERE!!!!!!!!!!!!!\n");
							fflush(stdout);
							//acc = acc + timeout.tv_usec;
							clock_gettime(CLOCK_REALTIME, &end1);
							elapsed = diff2float(&start, &end1);
							acc =  1000000000*elapsed;
							//printf("HHHHHHHHHH\n");
							//fflush(stdout);
						}

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
				if(!COMPRESS){
					totallen = (uint32_t *)&(tmp_buffer[0]);
					pad = (uint32_t *)&(tmp_buffer[4]);
					*totallen = tlen;
					*pad = BUFFER_LEN - (tlen+8);
					len = BUFFER_LEN;				
				}

	    		//len = read(psc->tun_fd, tmp_buffer, BUFFER_LEN);
				
				if (len <= 0 || len > BUFFER_LEN)
					continue;
				if(DEBUG_PRINT)
				printf("Came to line number %d \n", __LINE__);
				/*pheader = (struct svpn_net_ipv4_header*)(&tmp_buffer[12]);

				uid = pheader->dst_ip[3];
				//uid = 188;
				if (psc->clients[uid] == NULL)
				{
					printf("The ip first part is %d\n",pheader->dst_ip[0]);
					 printf("The ip second part is %d\n",pheader->dst_ip[1]);
					  printf("The ip third part is %d\n",pheader->dst_ip[2]);
					   printf("The ip fourth part is %d\n",pheader->dst_ip[3]);

					mprintf(LWARN, "User #%d not exist", uid);
					continue;
				}*/
				if(DEBUG_PRINT){
					printf("Came to line number %d \n", __LINE__);
					printf("\ntlen = %d\n",tlen);
				}
				if (ENCRYPT) 
					Encrypt(&(psc->clients[uid]->table), tmp_buffer, buffer, len);
				
				else {
					
					if(COMPRESS && tlen>0){
						if(DEBUG_PRINT)
							printf("Came to line number %d \n", __LINE__);
						clock_gettime(CLOCK_REALTIME, &start1);
						comp_len = minicomp(buffer + (main_tlen - tlen) + 8, tmp_buffer + 8, tlen, BUFFER_LEN-(main_tlen - tlen + 8));
						clock_gettime(CLOCK_REALTIME, &end3);
						elapsed = diff2float(&start1, &end3);
						if(COMP_PRINT){
							printf("elapsed time for minicomp %f\n ",  elapsed);
							fflush(stdout);
						}
						if(DEBUG_PRINT)	
							printf("Came to line number %d \n", __LINE__);
						totallen2 = (uint32_t *)&(buffer[0]);
						pad2 = (uint32_t *)&(buffer[4]);
						*totallen2 = (main_tlen - tlen) + comp_len;
						*pad2 = BUFFER_LEN - ((main_tlen - tlen) + comp_len + 8);
															
						
					}
					//printf("Came to line number %d \n", __LINE__);
					else if(tlen>0){	
						clock_gettime(CLOCK_REALTIME, &start1);
						memcpy(buffer, tmp_buffer, len);
						clock_gettime(CLOCK_REALTIME, &end3);
					    elapsed = diff2float(&start1, &end3);
					    if(COMP_PRINT){
							printf("elapsed time for memcpy %f\n ",  elapsed);
							fflush(stdout);
						}
					}
				}

				clock_gettime(CLOCK_REALTIME, &end2);
				elapsed = diff2float(&start, &end2);
				acc =  1000000000*elapsed;
				main_tlen = COMPRESS ? *totallen2 : tlen ; 
			}	
			//LOOP YAHAN TAK_++_++_++_++_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+__+_+_+_+_+_+__+_+_+_+_+__+_+__+_+__+__+__+_+_+_+_+_+_+_++_+_+_+_+_+_
			int sent_len=0,readlen=1400;
			if(DEBUG_PRINT)
			printf("Came to line number %d \n", __LINE__);
			
			if(RATE_LIM){
			long long int diff = 100000000 - acc ;
			if(diff<0)diff=0;
			printf("\ndiff = %lld\n",diff);
			
			sleep3.tv_sec = 0;
			sleep3.tv_nsec = diff;
			nanosleep(&sleep3,&sleep4);
			
			}
			
			
			while(sent_len < BUFFER_LEN){
				if(DEBUG_PRINT){
					printf("Came to line number %d \n", __LINE__);
					printf("\nsent_len = %d\n",sent_len);
				}
				len = send(conn_fd, buffer + sent_len , BUFFER_LEN - sent_len, MSG_NOSIGNAL);
				if(len < 0)
				     printf("an error: %s\n", strerror(errno));
						
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
			//len = send(conn_fd, buffer, BUFFER_LEN, 0);
					
			if (len <= 0)
			{
				mprintf(LERROR, "Client #%d disconnected", uid);
				continue;
			}

			// update statistics data
			stat2.total_recv += len;
			stat2.total_pkgrecv++;
		//	output_info();
		}
		//printf("Came to line number %d \n", __LINE__);
		if(n_iter==20){
			n_iter=n_iter/5;
			total_acc = total_acc/5;
			//acc_avg = total_acc;
		}
		else{
			n_iter++;
			total_acc = total_acc + acc + acc2;
			acc_avg = total_acc/n_iter;
		   	if(DEBUG_PRINT)
			printf("\nAVG_ACC = %lld\n", acc_avg);	
		}

	}
	if(DEBUG_PRINT)
	printf("Came to line number %d \n", __LINE__);
	return 0;
}

struct svpn_server *svpn_server_init(const char* configfile)
{
	struct svpn_server *psc;
	struct sigaction sact;
	FILE* fin;
	char tmpstr[128];
	unsigned short port;
	int laddr;

	if ((fin = fopen(configfile, "r")) == NULL)
		return NULL;

	port = 0;
	laddr = 0;
	while (1)
	{
		if (fscanf(fin, "%s", tmpstr) <= 0)
			break;
		if (strcasecmp(tmpstr, "port") == 0)
			fscanf(fin, "%hd", &port);
		else if (strcasecmp(tmpstr, "local") == 0)
		{
			fscanf(fin, "%s", tmpstr);
			laddr = inet_addr(tmpstr);
//		printf("lelelelele%d\nleelelele",laddr);
		}

skipline:
		fgets(tmpstr, sizeof(tmpstr), fin);
	}
	fclose(fin);

	if (port == 0)
	{
		mprintf(LFATAL, "No port specified");
		return NULL;
	}
	if (laddr == 0)
	{
		mprintf(LFATAL, "No local address specified");
		return NULL;
	}

	psc = (struct svpn_server*)malloc(sizeof(struct svpn_server));
	memset(&sact, 0, sizeof(struct sigaction));
	memset(psc, 0, sizeof(struct svpn_server));
   // printf("lelelelele%d\nleelelele",laddr);
	memcpy(psc->local_addr, &laddr, sizeof(laddr));
	mprintf(LINFO, "Assigned local address %d.%d.%d.%d",
				psc->local_addr[0], psc->local_addr[1],
				psc->local_addr[2], psc->local_addr[3]);

	// init socket
	if(svpn_sock_create(psc, port) < 0)
	{
		free(psc);
		return NULL;
	}

	mprintf(LINFO, "Bind Server Port %d", port);

	// init tunnel
	psc->tun_fd = svpn_tun_create(psc->dev_name, laddr);
	if(psc->tun_fd < 0)
	{
		close(psc->sock_fd);
		free(psc);
		return NULL;
	}

	sact.sa_handler = svpn_sig_handler;
	sact.sa_flags &= ~SA_RESTART;
	sigaction(SIGUSR1, &sact, &(psc->old_act));

	return psc;
}

int svpn_server_init_client(struct svpn_server *psc, const char* userlist)
{
	FILE* fin;
	char tmpstr[128];
	unsigned char pmd5[16];
	int tmpint, id;

	if((fin = fopen(userlist, "r")) == NULL)
		return -1;

	memset(psc->clients, 0, sizeof(psc->clients));

	while (1)
	{
		if (fscanf(fin, "%s", tmpstr) <= 0)
			break;
	//	 printf("tmpstr--%s--",tmpstr);
		if (strcasecmp(tmpstr, "user") != 0)
			goto skipline;

		// read id
		if (fscanf(fin, "%d", &id) <= 0)
			goto skipline;
		if (psc->clients[id] != NULL)
		{
			mprintf(LERROR, "User ID duplicated");
			goto skipline;
		}
		// read password
		if (fscanf(fin, "%s", tmpstr) <= 0)
			goto skipline;
		// OK
		psc->clients[id] = 
			(struct svpn_client_node*)malloc(sizeof(struct svpn_client_node));
		MD5Fast(tmpstr, strlen(tmpstr), pmd5);
		BuildTable(&(psc->clients[id]->table), pmd5, id);

		mprintf(LINFO, "User #%d added", id);

skipline:
		fgets(tmpstr, sizeof(tmpstr), fin);
	//printf("tmpstr--%s--",tmpstr);
	}

	fclose(fin);
	return 0;
}

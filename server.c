#include "unpifiplus.h"
#include <math.h>
#include <setjmp.h>

#define RTT_RXTMIN		1000 			
#define RTT_RXTMAX		3000			
#define RTT_MAXNREXMT		12 			
#define HEADER_SIZE 20

extern struct ifi_info *Get_ifi_info_plus(int family, int doaliases);
extern        void      free_ifi_info_plus(struct ifi_info *ifihead);

static int nextPos;
static sigjmp_buf 			jmpbuf;	
static int 				conn_flag;

struct header
{
	int  			seq_num;		
	int 			ack;			
	int 			fin;			
	int 			receive_window;		
	int			body_size;		
};

struct record 
{
	int 			sockfd;
	char 			ipaddr[20],nwaddr[20],sbaddr[20];
	
};

struct child_info
{
	char client_ip[50];
	int pipeid,client_port;

};

struct packet{
	struct packet 			*next;			
	struct header 			pheader;
	char 				body[512];
	int 				p_ack;			
	int 				pcount;		
	unsigned long long int timestamp; 	
};

struct rtt_info{
	int 				rtt;			
	int 				srtt;			
	int 				rttvar; 		
	int 				rto;			
	int 				windowPing;		
};

void error(char *msg)
{
	fprintf(stderr,"%s:  %s\n",msg,strerror(errno));
	exit(1);
}


void error_wo_exit(char *msg)
{
	fprintf(stderr, "%s: %s\n", msg, strerror(errno));
}

void string_tokenizer(char *ipadd, int *arr)
{
    int i=0, j, k;
    char temp[10];
	
    for(j=0;j<4;j++)
    {   
    	k=0;
   	 while (ipadd[i]!='.' && i<strlen(ipadd) )
   	 {
       	 temp[k]=ipadd[i];
       	 i++;k++;
    	}
	    i++;
	    arr[j]=atoi(temp);
	 // printf("%i",arr[j]);
	    bzero(temp,sizeof(temp));
    }
}

void bitwise_and(int *a,int *b, char *final)
{
	int i;
	char temp[255];
	sprintf(final,"%d", a[0] & b[0]);
	for(i=1;i<4;i++)
	{
		sprintf(temp,".%d",a[i] & b[i]);
		strcat(final,temp);
	}

	
	
}

int compareIP(char *ip1, char *ip2, char *subnet)
{
	int temp1[255], temp2[255], temp3[255];
	char result1[255], result2[255];
	// int i=0;
	string_tokenizer(ip1,temp1);
	string_tokenizer(ip2,temp2);
	string_tokenizer(subnet,temp3);
	bitwise_and(temp1,temp3,result1);
	bitwise_and(temp2,temp3,result2);
	if(strcmp(result1,result2)==0)
		return 1;
	else return 0;
}


char* checkchild(char *ip,int port, struct child_info *childinfo)
{
	int i;
	char pos[10], nodata[]="noentry";
	for(i=0;i<nextPos;i++)
		{
			if(strcmp(ip,childinfo[i].client_ip)==0 && port==childinfo[i].client_port)
				return sprintf(pos,"%d",i);
		}
	return nodata;
}



unsigned long long int get_time(){
    struct timeval tv;
	gettimeofday(&tv, NULL);
	unsigned long long int timestamp = (unsigned long long int)((unsigned long long int)tv.tv_usec)/1000;
	timestamp += (unsigned long long int) ((unsigned long long int)tv.tv_sec * 1000);
	return timestamp;
} 


void do_rtt(unsigned long dif, struct rtt_info **rtt){
	int delta = (dif - ((*rtt)->srtt >> 3));
	(*rtt)->srtt = (*rtt)->srtt + delta;
	(*rtt)->rttvar = (*rtt)->rttvar + (abs(delta)-((*rtt)->rttvar));
	(*rtt)->rto = rtt_minmax(((*rtt)->srtt >> 3) + (*rtt)->rttvar);
	printf("RTT info in msec: rto=%i \trtvar=%i \tsrtt=%i\n", (*rtt)->rto, ((*rtt)->rttvar >> 2), ((*rtt)->srtt >> 3));	
	return;
}


int rtt_minmax(int rtt){
	if(rtt<RTT_RXTMIN){
		return RTT_RXTMIN;
	}else if(rtt>RTT_RXTMAX){
		return RTT_RXTMAX;
	}else{
		return rtt;
	}
}

void sending(struct packet *dgpacket, int sockfd_child, struct packet **point, struct sockaddr_in *cliaddr, struct itimerval *counter, struct rtt_info *rtt)
	{	//printf("oyee"); fflush(stdout);

		char datagram[MAXLINE];
		struct packet *current = *point;
		dgpacket->pcount = dgpacket->pcount + 1;
		//*point=dgpacket;
		//printf("dg %i",dgpacket->pcount); fflush(stdout);
		
		printf("Window statistics: Start packet number: %i ",(int) ntohl((*point)->pheader.seq_num)); fflush(stdout);
		//printf("oyee"); fflush(stdout);
		
		while(current->next!=NULL)
			current = current->next;
		
		printf("End packet number: %i.\n", ntohl(current->pheader.seq_num));

		
		printf("Sending packet details:\n Packet number: %i\t No of times sent: %i\n", ntohl(dgpacket->pheader.seq_num), dgpacket->pcount);

		memcpy(&datagram, &(dgpacket->pheader), sizeof(dgpacket->pheader));
		memcpy(&datagram[HEADER_SIZE], &dgpacket->body, ntohl(dgpacket->pheader.body_size));

		if(sendto(sockfd_child, datagram, HEADER_SIZE+512, 0, NULL, sizeof(*cliaddr)) < 0 ){
			printf("Error in sendto %s\n", strerror(errno));
		}
		return;
	}


void removep(int seq_num, int *flight, struct packet **point, struct itimerval **counter, struct rtt_info **rtt)
{
	struct packet *current = *point;
	*flight = *flight - 1;
	if(*point!=NULL)
		if(ntohl((*point)->pheader.seq_num) == seq_num)
		{
			*point = (*point)->next;
			return;
		}
		else
			{
				
				while(current->next != NULL)
				{
					if(ntohl(current->next->pheader.seq_num) == seq_num)
					{
						current->next=current->next->next;
						break;
					}
					else	{
							current=current->next;	
						}
				}
			return;
			}
	else return;


}


void handling(int seq_num, int *cwin, int *sthold, int *ackcount, int *r_size,int last, struct packet **point, struct itimerval **counter, struct rtt_info **rtt, unsigned int *flight, int sockfd_child, struct sockaddr_in *cliaddr){
	struct packet *current = *point;
	while(current != NULL)
	{
	
		if(ntohl(current->pheader.seq_num) < seq_num || last==1)
		{
			if(*cwin >= *sthold)
			{	if(*(ackcount)%(*cwin) == 0 && *ackcount != 0)
				{
					*ackcount = 0;	
					*cwin = min(*r_size, *cwin + 1);
					printf("Packets acknowledged after slow start - New CWIN= %i\n", *cwin);
				}
				else	*ackcount = *ackcount + 1;
				
				
			}
			else
			{
			 	*cwin = min(*r_size,*cwin + 1);
				printf("Packets Acknowledged during Slow Start - New CWIN= %i\n", *cwin);	
			}


			if(current->p_ack < 4)
			{			
				unsigned long long int ts = get_time();
				ts = ts - (current->timestamp);
				do_rtt((unsigned long) ts, rtt);
			}
			
			printf("\nPacket number %i removed from sending window\n",ntohl(current->pheader.seq_num));
			
			removep(ntohl(current->pheader.seq_num), flight, point, counter, rtt);

			current = *point;
			//printf("wait\n");
		}
		else if(ntohl(current->pheader.seq_num) == seq_num)
			{
				if( (current->p_ack = (current->p_ack+1)) == 4)
				{
				
					*sthold = ((*cwin/2.0)-(int)(*cwin/2.0))>=0.5?((int)(*cwin/2.0)+1):(int)(*cwin/2.0);
					*cwin = *sthold;
					*ackcount = 0;
					printf("Fast Retransmit for packet number %i with CWIN= %i\n", ntohl(current->pheader.seq_num), *sthold);
					sending(current, sockfd_child, point, cliaddr, counter, rtt);
				}
				current = current->next;
			}
			else	break;
			
	}
	return;
}


void packetizing(struct packet *dgpacket, int *flight, struct sent_packet **point)
{
	struct packet *current = *point;
	*flight = *flight + 1;
	if(*point == NULL){
		*point = dgpacket;
		current = *point;
	}else{
		while(current->next != NULL){
			current = current->next;
		}
		current->next = dgpacket;
	}
	printf("Adding packet number %i to sending window\n", ntohl(dgpacket->pheader.seq_num));
	printf("Packets in flight= %i\n", *flight);
	return;
}

static void signal_alarm(int signo)
{
	siglongjmp(jmpbuf, 1);
}

void filetransfer(int sockfd_child, struct sockaddr_in *cliaddr, char *file_name, int r_size)
{	
	int 			flight=0, cwin=1, ack=0, numofpack=0, last=0, ackcount=0,dataread=0;
	sigset_t 		mask;
	struct header		cheader;
	int sthold = r_size; 	
	char			buf[MAXLINE];
	struct packet *point=NULL;
	struct itimerval timerv, counter1;
	struct itimerval *counter = &timerv;
	sigemptyset(&mask);
	sigaddset(&mask, SIGALRM);
	struct rtt_info	*rtt = (struct rtt_info*) malloc( sizeof(struct rtt_info) );
	
	FILE *filesend;
	filesend = fopen(file_name, "r");
	if(filesend == NULL)
	{
		printf("Error reading file!\n"); 
		exit(1);
	}
	signal(SIGALRM, signal_alarm);
	bzero(&counter1,sizeof(counter1));
	//printf("oyee");
	//fflush(stdout);
	rtt->rto = rtt_minmax(3000);	
	rtt->srtt = 0;
	rtt->windowPing = rtt_minmax(1500);	
	rtt->rttvar = 3000;

	while(1)
	{
	if(last == 1 && flight == 0)
		{
			printf("*****Transfer completed successfully - Terminating child process*****\n");
			//close(sockfd_child);
			break;
		}
	if(sigsetjmp(jmpbuf, 1) != 0)
	{
			printf("\nTimer Expired!\n");
			sthold = ((cwin/2.0)-(int)(cwin/2.0))>=0.5?((int)(cwin/2.0)+1):(int)(cwin/2.0);
			cwin = 1;
			ackcount = 0;

			//check if already transmitted the maximum number of times
			if(point != NULL && point->pcount > RTT_MAXNREXMT)
			{
				printf("*****Maximum attempts for sending reached. Terminating the connection*****\n");fflush(stdout);
				close(sockfd_child);
				conn_flag=1;	
				return;
			}

			printf("Time out for packet number %i\n", ntohl(point->pheader.seq_num));
			printf("Resending...\sthreshold= %i\n",sthold);			
			rtt->rto = rtt_minmax(rtt->rto*2);
			sending(point, sockfd_child, &point, cliaddr, &counter, &rtt);
	}
	do
	{	
		int dataread = 0;
		sigprocmask(SIG_BLOCK, &mask, NULL);
		if(r_size>0 && flight<cwin)	
		{	
			
			if((dataread = fread(buf, sizeof(filesend),512, filesend)) < 512 && last==0)
			{
				printf("\nThis is the last byte!");
				last = 1;
			}
		}	
		if(dataread > 0)
		{
			struct packet *dgpacket = malloc( sizeof(struct packet) );
			bzero(dgpacket, sizeof(*dgpacket));
			printf("\Bytes read from the file: %i\n", dataread);
			//first packet of file will be seq # 1
			numofpack++;
			//printf("numofpack %i",numofpack);
	
			struct header sheader;
			
			sheader.seq_num = htonl(numofpack);
			sheader.ack = htonl(1);			
			sheader.receive_window = htonl(0);
			sheader.body_size = htonl(dataread);
			sheader.fin = htonl(last);

			dgpacket->next = NULL;
			dgpacket->pheader = sheader;
			memcpy(&(dgpacket->body), &buf, 512);
			dgpacket->p_ack = 0;
			dgpacket->pcount = 0;
			dgpacket->timestamp = get_time();
			//printf("numofpack %i",dgpacket->pheader.seq_num);
			//printf("amt of data %i",strlen(dgpacket->body));
			//fflush(stdout);
			packetizing(dgpacket, &flight, &point);
			sending(dgpacket, sockfd_child, &point, cliaddr, counter, rtt);
		}
			
		sigprocmask(SIG_UNBLOCK, &mask, NULL);
	}
		while(flight<cwin && dataread>0 ); 
		//int len=sizeof(cliaddr);
		printf("waiting for reply...\n"); fflush(stdout);
		counter1.it_value.tv_usec=rtt->rto;
		counter1.it_value.tv_sec=0;
		if(setitimer(ITIMER_REAL, &counter1, NULL) < 0)
		{
			printf("set timer error: %s\n", strerror(errno));
			exit(1);
		}

		if(recvfrom(sockfd_child, buf, MAXLINE, 0, NULL,NULL) < 0)
			printf("Error in recvfrom: %s\n", strerror(errno));
		else
			{
				counter1.it_value.tv_sec=0;
				counter1.it_value.tv_usec=0;	
				if(setitimer(ITIMER_REAL, &counter1, NULL) < 0)
				{
				printf("set timer error: %s\n", strerror(errno));
				exit(1);
				}
			printf("\nReceived a packet from the client\n"); fflush(stdout);
			memcpy(&cheader, &buf, sizeof(cheader));
			if(ntohl(cheader.ack) == 1)
				{
					r_size = ntohl(cheader.receive_window);
					if(r_size == 0)
						printf("Receiver window is full!\n");
					

				
				printf("ACK received = %i\n", ntohl(cheader.seq_num));
				handling(ntohl(cheader.seq_num), &cwin, &sthold, &ackcount, &r_size,last, &point, &counter, &rtt, &flight, sockfd_child, cliaddr);
				//printf("handled!");
				//fflush(stdout);
				counter->it_value.tv_usec = 0;
			}
			}

}
return;	
}	


int main(int argc,char** argv)
{
	int 			port,s_size,n,sockfd,size,sockfd_child,flag=0,lhost_flag=0, r_size;
	const int 		on=1;
	pid_t			pid;
	struct sockaddr_in 	*sa,client, servaddr,cliaddr,servaddr_child;
	struct ifi_info 	*ifi,*ifihead;
	FILE 			*ifp;
	//char*			soc;
	struct record		re[10];
	int 			c=0,mysock,pfd[2];
	int 			ipadd[6], netmsk[6],i,childport;
	char 			final[255],temp[255],*cli_ipaddr,*ip_server,ip_serv[20],ipchild[MAXLINE], temp1[10], *check,body[MAXLINE];
								
	char 			mesg[MAXLINE],message[MAXLINE],datagram[MAXLINE],recvline[MAXLINE], file_name[100];
	fd_set 			rset;
	char			input[20];
	struct child_info	childinfo[10];
	struct header		sheader,cheader;
	struct timeval		select_timer;
	struct itimerval		*counter;
	bzero(&counter,sizeof(counter));
		
	printf("***************Details read from the file*****************\n\n");
	ifp=fopen("server.in","r");
	if(ifp==NULL)
		{
			error("Error in opening the file!");
		 }
	
	if(fscanf(ifp,"%s",input)!=NULL)
	{	
		port=atoi(input);
		printf("Port no.:  %i\n",port);
			
	}
	else 	{
			printf("Port not defined in the file\n");
			exit(1);
		}

	

	if(fscanf(ifp,"%s",&input)!=NULL)
	{	
		s_size=atoi(input);
		printf("Window size: %i\n\n",s_size);
	}
	else 	{
			printf("Window size not defined in the file\n");
			exit(1);
		}
	printf("*********INTERFACE DETAILS********\n");
	while(1)	
	{
	 	c=0;
		for (ifihead=ifi=Get_ifi_info_plus(AF_INET,1); ifi!=NULL; (ifi=ifi->ifi_next))
		{	
			if((re[c].sockfd=socket(AF_INET,SOCK_DGRAM,0))<0)
			error("Cannot create socket!");
			if((setsockopt(re[c].sockfd,SOL_SOCKET,SO_REUSEADDR,&on,sizeof(on)))<0)
			error("Error in setting the reuse option!");
		
			bzero(&servaddr,sizeof(servaddr));
			sa=(struct sockaddr_in*)ifi->ifi_addr;
			sa->sin_family=AF_INET;
			
			sa->sin_port=htons(port);
			if((bind(re[c].sockfd,(SA*) sa,sizeof(*sa)))<0)
			error("Error in bind!");	
		
			if(flag!=1)
			{
		
				
				strcpy(re[c].ipaddr,sock_ntop((SA*) sa,sizeof(*sa)));
				printf("\n\nBound %s\n",re[c].ipaddr);
				string_tokenizer(re[c].ipaddr,ipadd);

				if ( (sa = ifi->ifi_ntmaddr) != NULL)
				strcpy(re[c].nwaddr,sock_ntop((SA*)sa,sizeof(*sa)));
			 	printf("Network mask: %s\n",re[c].nwaddr);
				string_tokenizer(re[c].nwaddr,netmsk);
		
				sprintf(final,"%d", ipadd[0] & netmsk[0]);
				for(i=1;i<4;i++)
				{
					sprintf(temp,".%d",ipadd[i] & netmsk[i]);
					strcat(final,temp);
				}
				strcpy(re[c].sbaddr,final);
				printf("Subnet mask: %s\n\n\n",re[c].sbaddr);
			}
			
			c++;
		
		}
			printf("Waiting for client request...\n"); fflush(stdout);
			flag=0,lhost_flag=0;			
			FD_ZERO(&rset);
			int max = re[0].sockfd;
			for(i=0;i<c;i++)
			{
				if(re[i].sockfd>max)
					max = re[i].sockfd;
				FD_SET(re[i].sockfd,&rset);
				
			}
			
			//printf("before");
			//fflush(stdout);
			int ready = select(max+1,&rset,NULL,NULL,NULL);
			if(ready==-1 && errno==EINTR)
				continue;
			//printf("after");
			//fflush(stdout);

			for(i=0;i<c;i++)
			{	
				if(FD_ISSET(re[i].sockfd,&rset))
				{	int length=sizeof(cliaddr);
					
					if((n=recvfrom(re[i].sockfd, mesg, MAXLINE, 0, (SA*)&cliaddr, &length))<0)
						error_wo_exit("Error in reading!");
					printf("Connection request received from the client\n\n");

					memcpy(&cheader, &mesg, sizeof(cheader));
					memcpy(&message, &mesg[HEADER_SIZE], ntohl(cheader.body_size));
					r_size=ntohl(cheader.receive_window);
					r_size=min(s_size,r_size);
					//printf("min size %i",r_size);
					message[strlen(message)] = '\0';
					strcpy(file_name,message);
					printf("Request for the file to be transferred is %s\n",message);
					
					if(nextPos)
					{
						printf("checking");
						check=checkchild(inet_ntoa( cliaddr.sin_addr),(int)ntohs( cliaddr.sin_port),&childinfo);
						if(strcmp(check,"noentry"))
						{
							printf("Child already created\nSending child server details again to the client\n");
							write(childinfo[atoi(check)].pipeid,"Retransmit",11);
							//goto LABEL;
							break;
						}
					}
					strcpy(childinfo[nextPos].client_ip,inet_ntoa( cliaddr.sin_addr));
					childinfo[nextPos].client_port=(int)ntohs( cliaddr.sin_port);
					
					
					if (pipe(pfd) == -1)
					    error("Pipe failed!");
					childinfo[nextPos].pipeid=pfd[1];
					nextPos++;
					

					if ((pid = fork()) < 0)
					   error("Forking failed!");
					break;		
				}
			}
			int slen=sizeof(servaddr);
			if((getsockname(re[i].sockfd, (SA *) &servaddr, &slen))<0);
			ip_server=inet_ntoa( servaddr.sin_addr);
			mysock=re[i].sockfd;
			if(!pid)
			{	
				close(pfd[1]);
				for(i=0;i<c;i++)
				{
					if(re[i].sockfd!=mysock)
					close(re[i].sockfd);
				}
				printf("Server Child created for the client...\n");
				strcpy(ip_serv,ip_server);
				//dg_echo(mysock, (SA *) &cliaddr, sizeof(cliaddr));
				printf("IPclient: %s",inet_ntoa( cliaddr.sin_addr));
				cli_ipaddr=inet_ntoa( cliaddr.sin_addr);
				printf(":%d\n", (int) ntohs(cliaddr.sin_port));
				
				if(!(strcmp(cli_ipaddr,"127.0.0.1")))
				{
					
					cliaddr.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
					printf("*************Client on Localhost -> DONTROUTE OPTION SET************\n");
					lhost_flag=1;
				}
				else {
					for(i=0;i<c;i++)
					{	
							if(compareIP(re[i].ipaddr,cli_ipaddr,re[i].sbaddr))
							if(flag==0)
								flag=1;
					} 
				     }
				if(flag==1)
					printf("*************Client is local -> DONTROUTE OPTION SET************\n");
				else if(lhost_flag==0)
					printf("Client is not local\n");
				fflush(stdout);
				bzero(&servaddr_child, sizeof(servaddr_child));
				servaddr_child.sin_family = AF_INET;
				servaddr_child.sin_port = htons(0);
				if (inet_pton(AF_INET,ip_serv , &servaddr_child.sin_addr) < 0)
				error("inet_pton error!");
				int clen=sizeof(servaddr_child);
				
				if((sockfd_child=socket(AF_INET,SOCK_DGRAM,0))<0)
				error("Cannot create socket");
			
		
				if((setsockopt(sockfd_child,SOL_SOCKET,SO_REUSEADDR,&on,sizeof(on)))<0)
				error("Error in setting reuse option");
				if(flag==1)
					{
						if((setsockopt(sockfd_child,SOL_SOCKET,SO_DONTROUTE,&on,sizeof(on)))<0)
						error("Error in setting DONTROUTE option");
			
					}
				if((bind(sockfd_child,(SA*) &servaddr_child,sizeof(servaddr_child)))<0)
				error("Error in bind!");
				
				if((connect(sockfd_child,(SA*) &cliaddr,sizeof(cliaddr)))<0)
					error("Error in connecct!");
				if((getsockname(sockfd_child, (SA *) &servaddr_child, &clen))<0);
				
				childport=(int) htons(servaddr_child.sin_port);
				sprintf(ipchild,"%d", childport );
				

				printf("Server Child Port No.: %s\n",ipchild);
				fflush(stdout);	
				
				strcpy(body,ipchild);
				sheader.ack = htonl(1);
				sheader.receive_window = htonl(0); 	//set the advertised window size
				sheader.seq_num = htonl(0); 		//the next packet number we are expecting
				sheader.body_size = htonl(strlen(body)); 			//empty body
				memcpy(&datagram, &sheader, sizeof(sheader));

				memcpy(&datagram, &sheader, sizeof(sheader));
				memcpy(&datagram[20], &body, strlen(body));		
				
				if((sendto(mysock,datagram,(20+strlen(body)),0,(SA*) &cliaddr,sizeof(cliaddr)))<0)
				error("error in send to");

				

				//if((sendto(sockfd_child,datagram,(20+strlen(body)),0,NULL,sizeof(cliaddr)))<0)
				//error("error in send to");
				
				fd_set fset;
				FD_ZERO(&fset);
				FD_SET(sockfd_child,&fset);
				FD_SET(pfd[0],&fset);
				select_timer.tv_sec=5;
				select_timer.tv_usec=0;
				
				if((ready=select((sockfd_child)+1,&fset,NULL,NULL,&select_timer))<0)
					error("error in select");
				if(ready==-1 && errno==EINTR)
				continue; 
			
				
				if(FD_ISSET(sockfd_child,&fset))				
				{
					close(mysock);
					recvfrom(sockfd_child,recvline,sizeof(recvline),0,NULL,NULL);
					memcpy(&cheader, &recvline, sizeof(cheader));
					printf("Acknowledge received..\n******************Starting file transfer*******************\n\n");
					filetransfer(sockfd_child,&cliaddr, file_name, r_size);
					//printf("after file transfer"); fflush(stdout);
					//sheader.ack=htonl(1);
					//sheader.fin=htonl(1);
					//memcpy(&datagram, &sheader, sizeof(sheader));
					//if(conn_flag!=1)
					//{	//printf("after conn_flag"); fflush(stdout);
						//if((n=sendto(sockfd_child,datagram,20,0,NULL,sizeof(cliaddr)))<0)
						//	error("Error in sendto");
							
						//counter->it_value.tv_sec=3l;
						//if(setitimer(ITIMER_REAL, &counter, NULL) < 0)
						//{
						//	printf("set timer error 2:%s\n", strerror(errno));
						//	exit(1);
						//}
						//if((n=recvfrom(sockfd_child,datagram,20,0,NULL,NULL))<0)
						//	error("Error in readfrom");
						//printf("after recv"); fflush(stdout);
				 		//memcpy(&cheader,&datagram,sizeof(cheader));
						//if((ntohl(cheader.ack==1)) && (ntohl(cheader.fin)==1))
						//{
						//	printf("Received FIN from the client\nClosing connection...\n\n");
						//	fflush(stdout);
							
						//}
					//}
					

					
				}
				if(FD_ISSET(pfd[0],&fset))
				{
					printf("Retransmitting child server port no.\n");
					
					if((sendto(sockfd,datagram,(20+strlen(body)),0,(SA*)&cliaddr,sizeof(cliaddr)))<0)
					error("error in send to");	

				}	
				
			}	
			else		//in parent
				{
					close(pfd[0]);
					
					
				}
					close(sockfd_child);
					printf("Child terminated\n"); fflush(stdout);				
             		flag=1;			
			
	}	
	exit(0);
	
}




#include "unpifiplus.h"
#include "unprtt.h"
#include <setjmp.h>

#define HEADER_SIZE 20

pthread_mutex_t		mutex_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t   	cond_lock = PTHREAD_COND_INITIALIZER;

//ssize_t Dg_send_recv(int, const void *, size_t, void *, size_t,const SA *, socklen_t);
static sigjmp_buf jmpbuf;
static int retrans_count;
int nextseq=1;

struct header
{
	int  				seq_num;		
	int 				ack;			
	int 				fin;			
	int 				receive_window;		
	int				body_size;		
};

struct packet
{
	struct packet			  	*next;
	struct header  				pheader;
	char 					body[MAXLINE];	
};

struct thread_arg
{
	int 					sockfd,port;
	double 					probability;
	int 					r_size;
	struct sockaddr_in			serveraddr;
	char 					addr[50];
};
struct 	packet 	*point = NULL;

void error(char *msg)
{
	fprintf(stderr,"%s:  %s\n",msg,strerror(errno));
	exit(1);
}


void error_wo_exit(char *msg)
{
	fprintf(stderr, "%s: %s\n", msg, strerror(errno));
}

static void reset(int signo)
{	
	siglongjmp(jmpbuf,1);
	
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
	//int flag=0;
	string_tokenizer(ip1,temp1);
	string_tokenizer(ip2,temp2);
	string_tokenizer(subnet,temp3);
	bitwise_and(temp1,temp3,result1);
	bitwise_and(temp2,temp3,result2);
	if(strcmp(result1,result2)==0)
		return 1;
	else return 0;
}
/*void
dg_cli(FILE *fp, int sockfd, const SA *pservaddr, socklen_t servlen)
{
	int	n;
	char	sendline[MAXLINE], recvline[MAXLINE + 1];

	 fgets(sendline, MAXLINE, fp); 
	
		//n=Dg_send_recv(sockfd,sendline,sizeof(sendline),recvline,sizeof(recvline),pservaddr,servlen);
	
		recvline[n] = 0;	/* null terminate */
	
	/*	printf("%s\n",recvline);
	
}*/


void* producer_work(void* var)
	{	
		struct thread_arg * list = (struct thread_arg *) var;
		printf("Producer thread created\n");
		char recvline[MAXLINE],body[MAXLINE],datagram[MAXLINE];
		int n,wcount=0;
		int sockfd=list->sockfd;
		int wsize=list->r_size;
		int advert_win=wsize;
		float probability=list->probability;
		struct header sheader,cheader;
		struct packet rpacket,*current;
		struct sockaddr_in servaddr;
		int port=list->port;
		
		char addr[50];
		strcpy(addr,list->addr);
		//printf("port %i ip: %s\n",port, addr);
		printf("probability %f",probability);
		
	while(1)
	{
		//servaddr=list.serveraddr;
	
		printf("Waiting for server response...\n"); fflush(stdout);
		if((n=recvfrom(sockfd, recvline, MAXLINE, 0,NULL,NULL)<0))
				//error("Error in read!");
				if(errno==EINTR)
				{
					continue;
				}
				else error("Error in read!");
			recvline[n] = 0;	/* null terminate */
			memcpy(&sheader, &recvline, sizeof(sheader));
			memcpy(&(rpacket.body), &recvline[HEADER_SIZE], 512);
		

		if(loss(probability)==0)
		{		
			printf("Packet dropped");
			continue;	
		}
		
			//printf("%s",(rrpacket->rpacket.body));
			rpacket.pheader=sheader;
			printf("\nPacket Received from server\n Seq_num= %i\t ack= %i)\n",ntohl(sheader.seq_num),ntohl(sheader.ack));
		
		if(ntohl(sheader.fin) == 1){
			printf("*This is the last packet from the server!\n");
			
		}

		
		if(point == NULL) 
			point=&rpacket;
		current=point;

		if( advert_win== 0)
		printf("*The window is full now!!!\n");
		
		printf("Adding sequence number %i to the window.\n", ntohl(rpacket.pheader.seq_num));
		printf("Printing contents of packet no. %i \n %s",ntohl(rpacket.pheader.seq_num),rpacket.body);
			
			
		

		
	
		if(ntohl(rpacket.pheader.seq_num) >= nextseq)
		{
			wcount = wcount + 1; 
			advert_win = wsize - wcount; 

						if(ntohl(rpacket.pheader.seq_num) < ntohl(point->pheader.seq_num))
			{
				rpacket.next = point;
				point =&rpacket;
				
			}else {
						while(current->next != NULL)
					{
						if(ntohl(rpacket.pheader.seq_num) > ntohl(current->next->pheader.seq_num))
							current = current->next;
						else
							break;
					
					}
					
					rpacket.next = current->next;
					current->next =&rpacket;
					
				
				}
		}
		if(ntohl(sheader.fin) == 1)
		{	
			printf("\n\n*******************Transfer Completed successfully! Closing Client*******************\n");
			cheader.ack=htonl(1);
			cheader.fin=htonl(1);
			memcpy(&datagram,&cheader,sizeof(cheader));
			if((n=sendto(sockfd,datagram,(20),0,NULL,sizeof(servaddr)))<0)
				error("Error in sendto!");
							
			close(sockfd);
			exit(1);
			
		}

	
		if(nextseq == ntohl(rpacket.pheader.seq_num))
		{
			while(current->next != NULL && ntohl(current->next->pheader.seq_num) == (ntohl(current->pheader.seq_num)+1))
			{
				current = current->next;
			}

			
			nextseq = ntohl(current->pheader.seq_num) + 1;
			
		}

		if(loss(probability)==0)
		{
			printf("Acknowledge to be sent is dropped\n");
			continue;
		}
		printf("Next expected Sequence Number: %i\n", nextseq);

		cheader.ack = htonl(1);
		cheader.receive_window = htonl(advert_win); 	
		cheader.seq_num = htonl(nextseq); 		
		cheader.body_size = htonl(0); 			
		memcpy(&datagram, &cheader, sizeof(cheader));

		//memcpy(&datagram, &cheader, sizeof(cheader));

		
		
		bzero(&servaddr, sizeof(servaddr));
		servaddr.sin_family = AF_INET;
		servaddr.sin_port = htons(port);
		inet_aton(addr,&servaddr.sin_addr);
		//printf("ip: %s\n", inet_ntoa(servaddr.sin_addr));
		//printf("port: %d\n",port);
		//inet_pton(AF_INET,addr, &servaddr.sin_addr);
		if((n=sendto(sockfd,datagram,(20),0,NULL,sizeof(servaddr)))<0)
			if(errno==111)
			{
				printf("Lossy Environment!\nClosing connection...\n");
				exit(1);		
			}
			else error("Error in sendto!");
		
		//printf("datagram %s",datagram); fflush(stdout);
	}
}

int loss(double probability)
{
	float			random;
	random = (double)rand() / (double)RAND_MAX;
	//printf("\nrand=%f, loss prob=%f\n",actual_drop_prob, lost_prob);
	if(random < probability){
		return 0;
	}else{
		return 1;
	}
}


int main(int argc,char** argv)
{
	FILE 			*ifp;
	char 			ser_ipaddr[10], inp[MAXLINE];
	int 			port,wsize,sockfd,n,count,len,sock_getname, flag=0,lhost_flag=0, pot,u,seed_value,child_port,connfd;
	float 			probability;
	const int 		on=1;
	pid_t			pid;
	struct sockaddr_in 	*sa,servaddr,cliaddr,newservaddr,newcliaddr;
	struct ifi_info 	*ifi,*ifihead;
	char			sendline[MAXLINE],recvline[MAXLINE],file_name[50],datagram[MAXLINE],dghead[20],body[MAXLINE],childport[50];
	char*			soc;
	
	char			*ipaddr,*nwaddr,*sbaddr;
	int 			ipadd[6], netmsk[6],i;
	char 			final[255],temp[255],ipclient[255];
	struct header		cheader,sheader;
	struct itimerval	counter;
	

	ifp=fopen("client.in","r");
	if(ifp==NULL)
	{	error("Error in opening the file!");
		//exit(1);
	}
	printf("****************Input File Details***************\n\n");
	fflush(stdout);
	if(fscanf(ifp,"%s",ser_ipaddr)!=NULL)
	{
		 printf("IP address of server: %s\n",ser_ipaddr);
	}
	else {
		printf("Server IP address not defined in the file!\n");
		exit(1);
		}

	if(fscanf(ifp,"%s",inp)!=NULL)
	{
		 
		port=atoi(inp);
		printf("Port no.: %i\n",port);
		     
	}
	else {
		printf("Server Port No. not defined in the file!\n");
		exit(1);
		}	

	if(fscanf(ifp,"%s",file_name)!=NULL)
	{	
		printf("File name:%s\n",file_name);
	}
	else 	{
			printf("File is not mentioned'\n");
			exit(1);
		}


	if(fscanf(ifp,"%s",inp)!=NULL)
	{
		wsize=atoi(inp);
		printf("Window size: %i\n",wsize);
	}
	else 	{
			printf("Window size not defined in the file!\n");
			exit(1);
		}
	
	if(fscanf(ifp,"%s",inp)!=NULL)
	{
		seed_value=atoi(inp);
		srand(seed_value);
		printf("Seed value is: %i\n",seed_value);
	}
	else 	{
			printf("Seed value not defined in the file!\n");
			exit(1);
		}

	if(fscanf(ifp,"%s",inp)!=NULL)
	{
		probability=atof(inp);
		printf("Probability of datagram lost: %f\n",probability);
	}
	else 	{
			printf("Probability not mentioned\n");
			exit(1);
		}

	if(fscanf(ifp,"%s",inp)!=NULL)
	{
		u=atoi(inp);
		printf("Value of u in miliseconds: %i\n\n",u);
	}
	else 	{
			printf("Value of u in miliseconds not mentioned\n");
			exit(1);
		}


	for (ifihead=ifi=Get_ifi_info_plus(AF_INET,1); ifi!=NULL; (ifi=ifi->ifi_next))
	{
		
		printf("*****************Interface Details*****************\n\n");
		fflush(stdout);
		sa=(struct sockaddr_in*)ifi->ifi_addr;
		sa->sin_family=AF_INET;
		sa->sin_port=htons(port);
		
				
		
		ipaddr=sock_ntop((SA*)sa,sizeof(*sa));
		printf("IP address: %s\n",ipaddr);
		strcpy(ipclient,ipaddr);
		string_tokenizer(ipaddr,ipadd);	

		if ( (sa = ifi->ifi_ntmaddr) != NULL)
		nwaddr=sock_ntop((SA*)sa,sizeof(*sa));
	 	printf("Network mask: %s\n",nwaddr);

		string_tokenizer(nwaddr,netmsk);
		bitwise_and(ipadd,netmsk, final);
		printf("Subnet mask: %s\n\n\n",final);
		
		if((strcmp(ser_ipaddr,"127.0.0.1")))
		if(compareIP(ipaddr,ser_ipaddr,final))
			if(flag==0)
				flag=1;
		
		
	}
		bzero(&cliaddr, sizeof(cliaddr));
		cliaddr.sin_family = AF_INET;
		
		if(!(strcmp(ser_ipaddr,"127.0.0.1")))
		{
			cliaddr.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
			printf("Server is on Localhost\n");
			lhost_flag=1;
		}
		
		cliaddr.sin_port = htons(0);
		len=sizeof(cliaddr);

		bzero(&servaddr, sizeof(servaddr));
		servaddr.sin_family = AF_INET;
		servaddr.sin_port = htons(port);
		if (inet_pton(AF_INET, ser_ipaddr, &servaddr.sin_addr) < 0)
		error("inet_pton error!");
		
		if((sockfd=socket(AF_INET,SOCK_DGRAM,0))<0)
		error("Cannot create socket!");
		
	
		if((setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,&on,sizeof(on)))<0)
		error("Error in setting reuse option!");
		if(flag==1)
			{
				if((setsockopt(sockfd,SOL_SOCKET,SO_DONTROUTE,&on,sizeof(on)))<0)
					error("Error in setting the DONTROUTE option!");
				printf("Server is local\n");
			}
		else if(lhost_flag=0)
			printf("Server is not local\n");
		if((bind(sockfd,(SA*) &cliaddr,sizeof(cliaddr)))<0)
		error("Error in bind!");
		if((sock_getname=socket(AF_INET,SOCK_DGRAM,0))<0)
			error("Cannot create socket!");
		if((connect(sock_getname,(SA*) &servaddr,sizeof(servaddr)))<0)
			error("Error in connecct!");

		if((getpeername(sock_getname,(SA*)&servaddr,sizeof(servaddr)))<0);
		printf("IPserver: %s",inet_ntoa(servaddr.sin_addr));
		printf(":%d\n", (int) ntohs(servaddr.sin_port));				
		
		if((getsockname(sock_getname, (SA *) &cliaddr, &len))<0);
			
	
		printf("IPclient:%s", inet_ntoa( cliaddr.sin_addr));
		printf(":%d\n", ((int) ntohs(cliaddr.sin_port))-1);
		close(sock_getname);
		
		strcpy(body,file_name);

		signal(SIGALRM,reset);
		if((sigsetjmp(jmpbuf,1))!=0 || 1)
		{
			if(retrans_count)
			{
				
				if(retrans_count==3){
							printf("Unable to connect to the server...giving up!");
							exit (0);
						    }
				printf("Retransmitting file request...\n");
			}
			printf("Connecting...\n");			
			
			cheader.ack = htonl(0);
			cheader.receive_window = htonl(wsize); 	
			cheader.seq_num = htonl(0); 		
			cheader.body_size = htonl(strlen(body));
			memcpy(&datagram, &cheader, sizeof(cheader));

			memcpy(&datagram, &cheader, sizeof(cheader));
			memcpy(&datagram[20], &body, strlen(body));
		
			sendto(sockfd,datagram,(20+strlen(file_name)),0,(SA*)&servaddr,sizeof(servaddr));
			retrans_count++;
		}
		counter.it_value.tv_sec = 5;
		counter.it_value.tv_usec = 0;
		if(setitimer(ITIMER_REAL, &counter, NULL)==-1)
		{
		printf("set timer error :%s\n", strerror(errno));
		exit(1);
		}
		
		if((n=recvfrom(sockfd, recvline, MAXLINE, 0,NULL,NULL)<0))
			//error("Error in read!");
			if(n==-1 && errno==EINTR);
		recvline[n] = 0;	/* null terminate */
		memcpy(&sheader, &recvline, sizeof(sheader));
		memcpy(&childport, &recvline[HEADER_SIZE], ntohl(sheader.body_size));
		childport[strlen(childport)] = '\0';
		//printf("File to be transferred is %s\n",message);
		printf("Server Child Port No.: %s\n",childport);
		fflush(stdout);
		
		//child_port=atoi(childport);
		bzero(&newservaddr, sizeof(newservaddr));
		newservaddr.sin_family = AF_INET;
		newservaddr.sin_port = htons(atoi(childport));
		newservaddr.sin_addr.s_addr=servaddr.sin_addr.s_addr;
		if((connect(sockfd,(SA*) &newservaddr,sizeof(newservaddr)))<0)
		error("Cannot connect!");

		cheader.ack = htonl(1);
		cheader.receive_window = htonl(0); 
		cheader.seq_num = htonl(1); 
		cheader.body_size = htonl(0);
		memcpy(&datagram, &cheader, sizeof(cheader));
					
		if((sendto(sockfd, datagram, sizeof(datagram),0,NULL,sizeof(newservaddr)))<0)
			error("error in sendto");
	
		counter.it_value.tv_sec = 0;
		counter.it_value.tv_usec = 0;
		if(setitimer(ITIMER_REAL, &counter, NULL)==-1)
		{
		printf("set timer error :%s\n", strerror(errno));
		exit(1);
		}

		struct thread_arg var;
		var.sockfd=sockfd;
		var.probability=probability;
		var.r_size=wsize;
		var.serveraddr=servaddr;
		var.port=(atoi(childport));
		strcpy(var.addr,inet_ntoa(newservaddr.sin_addr));
		
		pthread_t producer, consumer;

		
		if(pthread_create (&producer,NULL,producer_work,&var)<0)
		error("Cannot create thread");
		void* result;

		if(pthread_join(producer,&result)<0)
		error("Cannot join thread");	
		

	exit(0);
}

	

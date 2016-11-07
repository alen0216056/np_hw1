#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>

#define MAXLINE 1024

int max(int a, int b);
void sys_error(char* str);
void initialize(int* sockfd, struct sockaddr_in* servaddr, char* ip, char* port);
void str_cli(FILE *fp, int sockfd);
int check_exit(char* command);

int main(int argc, char** argv)
{
	int sockfd;
	struct sockaddr_in servaddr;
	
	if(argc!=3)
		sys_error("usage:./client <SERVER IP> <SERVER PORT>");
	
	initialize(&sockfd, &servaddr, argv[1], argv[2]);

	str_cli(stdin, sockfd);
	
	if(close(sockfd)<0)
		sys_error("close socket error");
	
	return 0;
}

int max(int a, int b)
{
	return (a>b)? a: b;
}

void sys_error(char* str)
{
	printf("%s\n", str);
	exit(1);
}

void initialize(int* sockfd, struct sockaddr_in* servaddr, char* ip, char* port)
{
	*sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(*sockfd<0)
		sys_error("create socket error");

	bzero(servaddr, sizeof(*servaddr));
	servaddr->sin_family = AF_INET;
	servaddr->sin_port = htons(atoi(port));
	if( inet_pton(AF_INET, ip, &servaddr->sin_addr)<=0 )
		sys_error("inet_pton error");
	
	if( connect(*sockfd, (struct sockaddr*)servaddr, sizeof(*servaddr))<0 )
		sys_error("connect error");
}

void str_cli(FILE *fp, int sockfd)
{
	int maxfd, n;
	char sendline[MAXLINE], recvline[MAXLINE];
	char* ptr;
	fd_set rset;
	
	FD_ZERO(&rset);
	while(1)
	{
		FD_SET(fileno(fp), &rset);
		FD_SET(sockfd, &rset);
		maxfd = max(fileno(fp), sockfd) + 1;
		n = select(maxfd, &rset, NULL, NULL, NULL);
		
		if(FD_ISSET(sockfd, &rset))
		{
			if((n=read(sockfd, recvline, MAXLINE))==0)
				sys_error("server terminate");
			
			recvline[n] = '\0';
			fputs(recvline, stdout);
		}

		if(FD_ISSET(fileno(fp), &rset))
		{
			if(fgets(sendline, MAXLINE, fp)==NULL)
				return;
			if(sendline[0]=='\n')
				continue;
			if(check_exit(sendline))
				return;
			
			write(sockfd, sendline, strlen(sendline));
		}
	}
}

int check_exit(char* command)
{	
	char tmp[MAXLINE];
	char* ptr;
	strcpy(tmp, command);
	ptr = strtok(tmp, " \n");
	if(strcmp(ptr, "exit")==0)
	{
		ptr = strtok(NULL, " ");
		if(ptr==NULL)
			return 1;
		return 0;
	}
	return 0;
}
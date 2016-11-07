#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <time.h>

#define MAXLINE 1024
#define LISTENQ 1024
#define SERV_PORT 9999

struct client
{
	int sockfd;
	char ip[16];
	int port;
	char name[13];
};

void sys_error(char* str);
void initialize(int* listenfd, struct sockaddr_in* server_addr, struct client* clients);

void broadcast_message(char* message, struct client* clients, int maxi);
void who(int self, struct client* clients, int maxi);
void name(int self, struct client* clients, int maxi, char* recvline);
void tell(int self, struct client* clients, int maxi, char* recvline, char* receiver);
void yell(int self, struct client* clients, int maxi, char* recvline);
void server(int listenfd, struct client* clients);

int main(int argc, char** argv)
{
	int listenfd;
	struct sockaddr_in server_addr;
	struct client clients[FD_SETSIZE];
	
	initialize(&listenfd, &server_addr, clients);
	
	server(listenfd, clients);
	
	if(close(listenfd)<0)
		sys_error("close listen socket error");
	
	return 0;
}

void sys_error(char* str)
{
	printf("%s\n", str);
	exit(1);
}

void initialize(int* listenfd, struct sockaddr_in* server_addr, struct client* clients)
{
	*listenfd = socket(AF_INET, SOCK_STREAM, 0);
	if(*listenfd<0)
		sys_error("create listen socket error");
	
	bzero(server_addr, sizeof(*server_addr));
	server_addr->sin_family = AF_INET;
	server_addr->sin_addr.s_addr = htonl(INADDR_ANY);
	server_addr->sin_port = htons(SERV_PORT);
	
	if(bind(*listenfd, (struct sockaddr *)server_addr, sizeof(*server_addr))<0)
		sys_error("bind error");
	
	if(listen(*listenfd, LISTENQ)<0)
		sys_error("listen error");
	
	int i;	
	for(i=0; i<FD_SETSIZE; i++)
	{
		clients[i].sockfd = -1;
		strcpy(clients[i].name, "anonymous");
	}
}

void broadcast_message(char* message, struct client* clients, int maxi)
{
	int i;
	for(i=0; i<=maxi; i++)
	{
		if(clients[i].sockfd<0)
			continue;
		write(clients[i].sockfd, message, strlen(message));
	}
}

void name(int self, struct client* clients, int maxi, char* recvline)
{
	int i;
	char new_name[MAXLINE], sendline[MAXLINE];
	char* ptr = strstr(recvline, "name") + 4;
	for(; ptr[0]!='\n'; ptr++)
	{
		if(ptr[0]==' ')
			continue;
		break;
	}
	strncpy(new_name, ptr, strlen(ptr)-1);
	new_name[strlen(ptr)-1] = '\0';
	
	if(strcmp(new_name, "anonymous")==0)
	{
		snprintf(sendline, MAXLINE, "[Server] ERROR: Username cannot be anonymous.\n");
		write(clients[self].sockfd, sendline, strlen(sendline));
		return;
	}
	
	if(2<=strlen(new_name) && strlen(new_name)<=12)
	{
		for(i=0; i<strlen(new_name); i++)
			if(!(('a'<=new_name[i] && new_name[i]<='z') || ('A'<=new_name[i] && new_name[i]<='Z')))
			{
				snprintf(sendline, MAXLINE, "[Server] ERROR: Username can only consists of 2~12 English letters.\n");
				write(clients[self].sockfd, sendline, strlen(sendline));
				return;
			}
	}
	else
	{
		snprintf(sendline, MAXLINE, "[Server] ERROR: Username can only consists of 2~12 English letters.\n");
		write(clients[self].sockfd, sendline, strlen(sendline));
		return;
	}
		
	for(i=0; i<=maxi; i++)
	{
		if(i==self || clients[i].sockfd<0)
			continue;
		if(strcmp(new_name, clients[i].name)==0)
		{
			snprintf(sendline, MAXLINE, "[Server] ERROR: %s has been used by others.\n", new_name);
			write(clients[self].sockfd, sendline, strlen(sendline));
			return;
		}
	}
		
	for(i=0; i<=maxi; i++)
	{
		if(clients[i].sockfd<0)
			continue;
		if(i==self)
			snprintf(sendline, MAXLINE, "[Server] You're now known as %s.\n", new_name);
		else
			snprintf(sendline, MAXLINE, "[Server] %s is now known as %s.\n", clients[self].name, new_name);
		write(clients[i].sockfd, sendline, strlen(sendline));
	}
	strcpy(clients[self].name, new_name);	
}

void who(int self, struct client* clients, int maxi)
{
	int i;
	char sendline[MAXLINE];
	for(i=0; i<=maxi; i++)
	{
		if(clients[i].sockfd<0)
			continue;
		if(i==self)
			snprintf(sendline, MAXLINE, "[Server] %s %s/%d ->me\n", clients[i].name, clients[i].ip, clients[i].port);
		else
			snprintf(sendline, MAXLINE, "[Server] %s %s/%d\n", clients[i].name, clients[i].ip, clients[i].port);
		write(clients[self].sockfd, sendline, strlen(sendline));
	}
}

void tell(int self, struct client* clients, int maxi, char* recvline, char* receiver)
{
	int i;
	char sendline[MAXLINE];
	char* ptr = strstr(recvline, receiver) + strlen(receiver);
	for(; ptr[0]!='\n'; ptr++)
	{
		if(ptr[0]==' ')
			continue;
		break;
	}
	recvline[strlen(recvline)-1] = '\0';
		
	if(strcmp(clients[self].name, "anonymous")==0)
	{
		snprintf(sendline, MAXLINE, "[Server] ERROR: You are anonymous.\n");
		write(clients[self].sockfd, sendline, strlen(sendline));
	}
	else if(strcmp(receiver, "anonymous")==0)
	{
		snprintf(sendline, MAXLINE, "[Server] ERROR: The client to which you sent is anonymous.\n");
		write(clients[self].sockfd, sendline, strlen(sendline));
	}
	else
	{
		for(i=0; i<=maxi; i++)
		{
			if(clients[i].sockfd<0)
				continue;
			if(strcmp(clients[i].name, receiver)==0)
			{
				snprintf(sendline, MAXLINE, "[Server] SUCCESS: Your message has been sent.\n");
				write(clients[self].sockfd, sendline, strlen(sendline));
				snprintf(sendline, MAXLINE, "[Server] %s tell you %s\n", clients[self].name, ptr);
				write(clients[i].sockfd, sendline, strlen(sendline));
				return;
			}
		}
		snprintf(sendline, MAXLINE, "[Server] ERROR: The receiver doesn't exist.\n");
		write(clients[self].sockfd, sendline, strlen(sendline));
	}
}

void yell(int self, struct client* clients, int maxi, char* recvline)
{
	int i;
	char sendline[MAXLINE];
	char* ptr = strstr(recvline, "yell") + 4;
	for(; ptr[0]!='\n'; ptr++)
	{
		if(ptr[0]==' ')
			continue;
		break;
	}
	recvline[strlen(recvline)-1] = '\0';
	snprintf(sendline, MAXLINE, "[Server] %s yell %s\n", clients[self].name, ptr);
	for(i=0; i<=maxi; i++)
	{
		if(clients[i].sockfd<0 || i==self)
			continue;
		write(clients[i].sockfd, sendline, strlen(sendline));
	}
}

void server(int listenfd, struct client* clients)
{
	int i, maxi = -1, maxfd = listenfd, connfd, nready;
	char* ptr;
	char recvline[MAXLINE], sendline[MAXLINE], tmp[MAXLINE];
	ssize_t n;
	fd_set rset, allset;
	struct sockaddr_in client_addr;
	socklen_t client_len = sizeof(client_addr);
	
	
	FD_ZERO(&allset);
	FD_SET(listenfd, &allset);
	while(1)
	{
		rset = allset;
		nready = select(maxfd+1, &rset, NULL, NULL, NULL);
		if(FD_ISSET(listenfd, &rset))
		{
			connfd = accept(listenfd, (struct sockaddr *)&client_addr, &client_len);
			if(connfd<0)
				sys_error("accept error");
			
			broadcast_message("[Server] Someone is coming!\n", clients, maxi);
			
			for(i=0; i<FD_SETSIZE; i++)
				if(clients[i].sockfd<0)
				{
					clients[i].sockfd = connfd;
					inet_ntop(AF_INET, &client_addr.sin_addr, clients[i].ip, sizeof(client_addr));
					clients[i].port = ntohs(client_addr.sin_port);
					break;
				}
			if(i==FD_SETSIZE)
				sys_error("too many clients");
			
			printf("new client %s, port %d\n", clients[i].ip, clients[i].port);
			snprintf(sendline, MAXLINE, "[Server] Hello, anonymous! From: %s/%d\n", clients[i].ip, clients[i].port);
			write(clients[i].sockfd, sendline, strlen(sendline));
			
			FD_SET(connfd, &allset);
			
			if(connfd>maxfd)
				maxfd = connfd;
			if(i>maxi)
				maxi = i;
			if(--nready<=0)
				continue;
		}	
		
		for(i=0; i<=maxi; i++)
		{
			if(clients[i].sockfd<0)
				continue;
			if(FD_ISSET(clients[i].sockfd, &rset))
			{
				if((n=read(clients[i].sockfd, recvline, MAXLINE))==0)
				{
					if(close(clients[i].sockfd)<0)
						sys_error("close client error");
					FD_CLR(clients[i].sockfd, &allset);
					
					snprintf(sendline, MAXLINE, "[Server] %s is offline.\n", clients[i].name);
					broadcast_message(sendline, clients, maxi);
					
					clients[i].sockfd = -1;
					strcpy(clients[i].name, "anonymous");
				}
				else
				{
					recvline[n] = '\0';
					strcpy(tmp, recvline);
					ptr = strtok(tmp, " \n");
					if(strcmp(ptr, "who")==0)
					{
						who(i, clients, maxi);
					}
					else if(strcmp(ptr, "name")==0)
					{
						name(i, clients, maxi, recvline);
					}
					else if(strcmp(ptr, "tell")==0)
					{
						ptr = strtok(NULL, " \n");
						tell(i, clients, maxi, recvline, ptr);
					}
					else if(strcmp(ptr, "yell")==0)
					{
						yell(i, clients, maxi, recvline);
					}
					else
					{
						snprintf(sendline, MAXLINE, "[Server] ERROR: Error command.\n");
						write(clients[i].sockfd, sendline, strlen(sendline));
					}
				}
				if(--nready<=0)
					break;
			}
		}
	}
}
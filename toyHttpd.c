#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

#define PORTNUM 8080
#define MAXEVENT 1000
#define MAXBUFSIZE 2048

#define HANDLE_ERROR(error_string) do{perror(error_string); exit(-1);}while(0);

int epfd;
struct epoll_event ev, *events;
int listen_sock;
struct sockaddr_in server_addr;
char buf[MAXBUFSIZE];

void setnonblocking(int sock)
{
    int opts;
    opts=fcntl(sock,F_GETFL);
    if(opts<0)
    {
        HANDLE_ERROR("fcntl(sock,GETFL)");
    }
    opts = opts|O_NONBLOCK;
    if(fcntl(sock,F_SETFL,opts)<0)
    {
        HANDLE_ERROR("fcntl(sock,SETFL,opts)");
    }
}

void createAndBind()
{
	listen_sock = socket(AF_INET, SOCK_STREAM, 0);
	if(-1 == listen_sock)
	{
		HANDLE_ERROR("can not create sock");
	}

	memset(&server_addr, 0, sizeof(server_addr));

	server_addr.sin_port = htons(PORTNUM);
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	int ret;
	ret = bind(listen_sock, (struct sockaddr *)&server_addr, sizeof(server_addr));
	if(-1 == ret)
	{
		HANDLE_ERROR("can not bind");
	}
}

void accept_sock(int sock)
{
	struct sockaddr_in client_sock;
	socklen_t len;
	int conn_fd;
	while((conn_fd = accept(sock, (struct sockaddr *)&client_sock, &len)) > 0)
	{
		setnonblocking(conn_fd);
		ev.data.fd = conn_fd;
		ev.events = EPOLLIN | EPOLLET;

		if(-1 == epoll_ctl(epfd, EPOLL_CTL_ADD, conn_fd, &ev))
		{
			HANDLE_ERROR("epoll add when accept");
		}
	}
	if(-1 == conn_fd)
	{
		if(errno != EAGAIN && errno != ECONNABORTED   
            && errno != EPROTO && errno != EINTR)//排除accpet到队列完这种返回，这只是读完了，并不是错误
            HANDLE_ERROR("accept error");
	}
}
void read_sock(int sock)
{
	int n = 0;
	int nread = 0;
	while((nread = read(sock, buf+n, MAXBUFSIZE)) >0)
	{
		n+=nread;
	}
	if (nread == -1 && errno != EAGAIN) 
	{  
    	HANDLE_ERROR("read");  
	}
	ev.data.fd = sock;
	ev.events = EPOLLOUT | EPOLLET;

	if(-1 == epoll_ctl(epfd, EPOLL_CTL_MOD, sock, &ev))
	{
		HANDLE_ERROR("epoll read");
	}
}
void write_sock(int sock)
{
	sprintf(buf, "HTTP/1.1 200 OK\r\nContent-Length: %d\r\n\r\nHello World", 11);
	int nwrite, data_size = strlen(buf);  
    int n = data_size;    
    while (n > 0)
    {  
        nwrite = write(sock, buf + data_size - n, n);  
        if (nwrite < n) 
        {  
	        if (nwrite == -1 && errno != EAGAIN) 
	        {  
	            HANDLE_ERROR("write");  
	        }  
            break;  
        }  
         n -= nwrite;  
    }  
    close(sock);  
}
void handle_request(struct epoll_event event)
{
	if(event.data.fd == listen_sock)
	{
		accept_sock(event.data.fd);
	}
	else if(event.events & EPOLLIN)
	{
		read_sock(event.data.fd);
	}
	else if(event.events & EPOLLOUT)
	{
		write_sock(event.data.fd);
	}
}

int main()
{
	int num;
	createAndBind();
	setnonblocking(listen_sock);

	epfd = epoll_create1(0);
	if(-1 == epfd)
	{
		HANDLE_ERROR("can not create epoll");
	}

	ev.data.fd = listen_sock;
	ev.events = EPOLLIN | EPOLLET;

	if(-1 == listen(listen_sock, SOMAXCONN))
	{
		HANDLE_ERROR("can not listen");
	}

	if(-1 == epoll_ctl(epfd, EPOLL_CTL_ADD, listen_sock, &ev))
	{
		HANDLE_ERROR("add listen sock");
	}

	events = (struct epoll_event *)malloc(MAXEVENT*sizeof(struct epoll_event));

	while(1)
	{
		num = epoll_wait(epfd, events, MAXEVENT, -1);
		if(-1 == num)
		{
			HANDLE_ERROR("epoll wait");
		}
		int i;
		for(i=0; i<num; i++)
		{
			handle_request(events[i]);
		}
	}



	return 0;
}
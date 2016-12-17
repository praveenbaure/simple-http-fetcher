#include <sys/epoll.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#define MAXCLI 10
#define FAIL -1

void error_hdl(char *err_str) {
	fprintf(stderr,"error in %s\n",err_str);
}


int make_nonblocking(int sock) {
	int current_flag, status;
	current_flag = fcntl(sock,F_GETFL,NULL);
	status = fcntl(sock,F_SETFL,current_flag | O_NONBLOCK);
	if(status == 0){
		return sock;
	}else{
		return FAIL;
	}
}


int create_bind(){
	struct addrinfo hints, *result;
	int status;
	int sockfd;
	int yes = 1;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	
	if((status = getaddrinfo(NULL,"32225",&hints,&result)) != 0){
		fprintf(stderr,"getaddinfo%s\n",gai_strerror(status));
		return FAIL;
	}
	
	//if((sockfd = socket(result->ai_family,result->ai_socktype | SOCK_NONBLOCK,result->ai_protocol)) < 0) {
	if((sockfd = socket(result->ai_family,result->ai_socktype, result->ai_protocol)) < 0) {
			perror("socket");
			return FAIL;
	}

	if((status = bind(sockfd,result->ai_addr,result->ai_addrlen)) < 0){
			perror("bind");
			return FAIL;
	}
	if((sockfd = make_nonblocking(sockfd)) < 0){
			error_hdl("bind");
			close(sockfd);
			return FAIL;
	}
	setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes));
	freeaddrinfo(result);
	return sockfd;
}


int main() {
		int status, sockfd,livefd,errno;

		//epoll fd for monitoring
		int efd,ready;
		int i;

		//structure for receiving client address
		struct sockaddr cliaddr;
		socklen_t cli_size = sizeof(struct sockaddr);

		//buffer to read from client
		char buf[1024];
		int readed, flag;
		char send_buf[] = "I am praveen";

		//epoll structures for specifying the action you are waiting for
		struct epoll_event event, *events;
		if((sockfd = create_bind()) < 0){
				error_hdl("create_bind");
		}
		printf("sockfd:-%d\n",sockfd);
		if((status = listen(sockfd,MAXCLI)) < 0){
			perror("listen");
			exit(2);
		}
		if((efd = epoll_create1(0)) < 0) {
				perror("epoll_create1");
				error_hdl("epoll_create1");
		}
		event.data.fd = sockfd;
		event.events = EPOLLIN | EPOLLET;

		//add above structure made to polling list

		if((status = epoll_ctl(efd,EPOLL_CTL_ADD,sockfd,&event)) < 0){
				perror("epoll_ctl");
				error_hdl("epoll_ctl");
				exit(2);
		}

		events = calloc(MAXCLI, sizeof(struct epoll_event));

		while(1){
				
				if((ready = epoll_wait(efd, events, MAXCLI, -1)) < 0) {
						perror("epoll_wait");
						error_hdl("epoll_wait");
				}
				printf("ready = %d\n",ready);
				for(i = 0; i < ready; i++){
				
						//it means new connection has been arrived
						flag = 0;
						if(events[i].data.fd == sockfd){
								while(1){
										memset(&cliaddr,0,cli_size);
										livefd = accept(sockfd, &cliaddr, &cli_size);
										if(livefd < 0){
												if((errno == EAGAIN) || (errno == EWOULDBLOCK)){
														break;

												} else {
														fprintf(stdout,"processing of all incomming request is done\n");
														perror("accept");
														error_hdl("accept");
														break;
												}
										} else {
												//create epoll event structure for newly arrivied fd
												memset(&event,0,sizeof(event));
												livefd = make_nonblocking(livefd);
												event.data.fd = livefd;
												event.events = EPOLLIN | EPOLLET;

												if((status = epoll_ctl(efd,EPOLL_CTL_ADD,livefd,&event)) < 0){
														perror("epoll_ctl");
														error_hdl("epoll_ctl");
												}
										}
								}
						} else {
								//else data has been arrivied at one of the fd
								while(1){ // to read all data in one swap as we are using epoll as edge trigger it will not notify again if data is left
										readed = read(events[i].data.fd,&buf,2);
										if(readed < 0){
												if((errno != EAGAIN) || (errno != EWOULDBLOCK)){
														perror("readed");
														flag = 1;
														error_hdl("readed");
														break;
												} else {
														fprintf(stdout,"all the data is readed from fd\n");
														break;
												}
										}else if(readed == 0) {
											fprintf(stdout,"connection id closed from other end by %d fd\n",events[i].data.fd);
											flag = 1;
											break;
										}
										
										if((write(1,buf,readed)) < 0) {
											perror("write");
											error_hdl("write");
										}
										printf("\n");
										write(events[i].data.fd, send_buf, sizeof(send_buf));
								}

						}
						if(flag) {
							close(events[i].data.fd);
						}
				}

		}
		close(efd);
		close(sockfd);
		free(events);

		return 1;
}

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>

#define BUFSIZE 4096
#define MAXEVENTS 32

enum state {
	UNKNOWN,

	CONNECTING,
	CONNECTED,
	WROTE,
	DONE,
};

typedef struct fd {
	enum state state;
	int        fd;
} fd_t;

int
fd_init(fd_t* fd)
{
	struct sockaddr_in server_addr = { 0 };

	fd->state = CONNECTING;

	if (!~(fd->fd =
	         socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_IP))) {
		perror("fd: socket");
		return -1;
	}

	server_addr.sin_family = AF_INET;
	server_addr.sin_port   = htons(1337);
	inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

	if (!~connect(
	      fd->fd, (struct sockaddr*)&server_addr, sizeof(server_addr))) {

		if (errno != EINPROGRESS) {
			perror("fd: connect");
			return -1;
		}
	}

	return 0;
}

int
do_read(int sock_fd)
{
	char buf[BUFSIZE] = { 0 };

	if (!~read(sock_fd, buf, BUFSIZE)) {
		perror("read");
		return -1;
	}

	printf("read: '%s'\n", buf);
	return 0;
}

int
do_write(int sock_fd)
{
	const char* out_msg = "GET / HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n";

	if (!~write(sock_fd, out_msg, strlen(out_msg))) {
		perror("write");
		return -1;
	}

	return 0;
}

int
sock_errcheck(int sock_fd)
{
	int       result;
	socklen_t result_len = sizeof(result);

	// get and clear any pending socket error
	//
	if (!~getsockopt(sock_fd, SOL_SOCKET, SO_ERROR, &result, &result_len)) {
		perror("getsockopt");
		return -1;
	}

	if (result != 0) {
		fprintf(stderr, "connection failed: %s", strerror(result));
		return -1;
	}

	return 0;
}

int
run(fd_t* fd)
{
	struct epoll_event event;
	struct epoll_event events[MAXEVENTS] = { 0 };
	int                epoll_fd;

	if (!~(epoll_fd = epoll_create1(0))) {
		perror("run: epoll_create1");
		return 1;
	}

	event.data.fd = fd->fd;
	event.events  = EPOLLIN | EPOLLOUT;

	if (!~epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd->fd, &event)) {
		perror("run: epoll_ctl");
		return -1;
	}

	while (1) {
		int n = 0;
		int i = 0;

		if (!~(n = epoll_wait(epoll_fd, events, MAXEVENTS, -1))) {
			perror("epoll_wait");
			return -1;
		}

		for (; i < n; i++) {
			int ev    = events[i].events;
			int ev_fd = events[i].data.fd;

			if (ev & EPOLLERR) {
				sock_errcheck(fd->fd);
				return -1;
			}

 			if (ev & EPOLLHUP) {
				// close the conn
				if (!~close(fd->fd)) {
					perror("run: close");
					return -1;
				}

				return 0;
			}

			if (ev_fd != fd->fd) {
				fprintf(stderr, "unknown fd");
			}

			switch (fd->state) {
				case CONNECTING:
					if (!~sock_errcheck(fd->fd)) {
						return -1;
					}

					fd->state = CONNECTED;
					break;

				case CONNECTED:
					if (!~do_write(fd->fd)) {
						return -1;
					}

					fd->state = WROTE;
					break;

				case WROTE:
					if (!(ev & EPOLLIN)) {
						printf("not readable yet\n");
						continue;
					}

					if (!~do_read(fd->fd)) {
						return -1;
					}

					fd->state = DONE;
					break;

				case DONE:
					return 0;

				default:
					fprintf(stderr,
					        "fd in unknown state\n");
					break;
			}
		}
	}

	return 0;
}

int
main(int argc, char** argv)
{
	fd_t fd = { 0 };

	if (!~fd_init(&fd)) {
		return 1;
	}

	if (!~run(&fd)) {
		return 1;
	}

	// ...

	/* if (!~communicate(sock_fd)) { */
	/* 	return 1; */
	/* } */

	/* if (!~close(sock_fd)) { */
	/* 	perror("close"); */
	/* 	return 1; */
	/* } */

	/* free(events); */

	return 0;
}

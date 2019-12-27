#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <unistd.h>

#define BUFSIZE 4096
#define MAXEVENTS 32

#define HOST "127.0.0.1"
#define PORT 1337

enum state {
	UNKNOWN,

	CONNECTING,
	CONNECTED,
	WROTE,
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
	server_addr.sin_port   = htons(PORT);
	inet_pton(AF_INET, HOST, &server_addr.sin_addr);

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
add_to_epoll(int epoll_fd, int fd, int flags)
{
	struct epoll_event event;
	event.data.fd = fd;
	event.events  = flags;

	if (!~epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event)) {
		if (errno != EEXIST) {
			perror("run: epoll_ctl ADD");
			return -1;
		}

		if (!~epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, 0)) {
			perror("run: epoll_ctl DEL_AFTER_ADD");
			return -1;
		}

		if (!~epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event)) {
			perror("run: epoll_ctl ADD_AFTER_RETRY");
			return -1;
		}
	}

	return 0;
}

int
add_timer_to_epoll(int epoll_fd)
{
	int timer_fd;
	struct itimerspec duration = {
		.it_value = {
			.tv_sec = 2, // 2 seconds since now
		},
	};

	if (!~(timer_fd = timerfd_create(CLOCK_REALTIME, 0))) {
		perror("run: timerfd_create");
		return -1;
	}

	if (!~timerfd_settime(timer_fd, 0, &duration, NULL)) {
		perror("run: timerfd_settime");
		return -1;
	}

	if (!~add_to_epoll(epoll_fd, timer_fd, EPOLLIN)) {
		return -1;
	}

	return timer_fd;
}

int
run(fd_t* fd)
{
	struct epoll_event events[MAXEVENTS] = { 0 };
	int                epoll_fd, timer_fd;

	if (!~(epoll_fd = epoll_create1(0))) {
		perror("run: epoll_create1");
		return 1;
	}

	if (!~add_to_epoll(epoll_fd, fd->fd, EPOLLOUT)) {
		return -1;
	}

	if (!~(timer_fd = add_timer_to_epoll(epoll_fd))) {
		return -1;
	}

	while (1) {
		int n = 0;
		if (!~(n = epoll_wait(epoll_fd, events, MAXEVENTS, -1))) {
			perror("epoll_wait");
			return -1;
		}

		for (int i = 0; i < n; i++) {
			int ev    = events[i].events;
			int ev_fd = events[i].data.fd;

			if (ev & EPOLLERR) {
				sock_errcheck(fd->fd);
				return -1;
			}

			if (ev & EPOLLHUP) {
				if (!~close(fd->fd)) {
					perror("run: close");
					return -1;
				}

				return 0;
			}

			if (ev_fd == timer_fd) {
				close(timer_fd);
				close(fd->fd);
				continue;
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

					if (!~add_to_epoll(
					      epoll_fd, fd->fd, EPOLLIN)) {
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

	return 0;
}

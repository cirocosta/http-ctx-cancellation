WHAT'S THIS

	this repo is intended to play around w/ context cancellation in Go in
	the "context" of http requests.
	

	it contains:

		- http client in C that w/ a timeout of `n` seconds 
		  - plain use of epoll and timerfd
		- http client in Go w/ a timeout of `n` seconds 
		  - using ctx cancellation
		- http server in Go that sleeps for N seconds 

	by making the server not respond on a given amount of time, we can
	simulate cases where you'd want to timeout the request.

	using `go`, one can do so via `http.NewRequestWithContext()`, giving it
	a context that gets cancelled after a given deadline is reached.

	under the hood, such cancellation is done through a socket shutdown
	(plain `close(2)`) on a nonblocking socket that's part of an epoll
	facility

		strace -f -e 'trace=!futex,nanosleep' ./http


		1. non-blocking socket gets created
			[pid 17751] socket(AF_INET, SOCK_STREAM|SOCK_CLOEXEC|SOCK_NONBLOCK, IPPROTO_IP) = 3
			[pid 17751] setsockopt(3, SOL_SOCKET, SO_BROADCAST, [1], 4) = 0
			[pid 17751] connect(3, {sa_family=AF_INET, sin_port=htons(1337), sin_addr=inet_addr("127.0.0.1")}, 16) = -1 
				EINPROGRESS (Operation now in progress)

		2. socket gets added to epoll facility

			[pid 17751] epoll_ctl(4, EPOLL_CTL_ADD, 3, {
				EPOLLIN|EPOLLOUT|EPOLLRDHUP|EPOLLET, {u32=812239112, u64=139655968835848}}) = 0

		3. wait on the fds that were added - once `connect` finishes, we
		   should get an event for that fd

			[pid 17751] epoll_pwait(4, [{
				EPOLLOUT, {u32=812239112, u64=139655968835848}}], 128, 0, NULL, 824634156712) = 1

		4. check if there were any errors in the conn

			[pid 17751] getsockopt(3, SOL_SOCKET, SO_ERROR,  <unfinished ...>

		5. write to the socket

			[pid 17751] write(3, "GET / HTTP/1.1\r\nHost: localhost:"..., 95

		6. try to read from it

			[pid 17757] read(3,  <unfinished ...>
			[pid 17757] <... read resumed> 0xc00011e000, 4096) 
				-1 EAGAIN (Resource temporarily unavailable)
				// ...

		7. would block, so lets continue waiting ..

		8. deadline reached, all we gotta do is remove from the set and
		   close it

			[pid 17755] epoll_ctl(4, EPOLL_CTL_DEL, 3, 0xc000132984) = 0
			[pid 17755] close(3)                    = 0


	naturally, we can implement that in C - see ./http-client.c


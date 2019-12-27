build:
	go build -v -o http.out ./http.go
	go build -v -o tcp.out ./tcp.go
	gcc -O2 -static -o client.out ./http-client.c

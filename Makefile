run: build
	./http-ctx-cancellation

build:
	go build -v -o http ./http.go
	go build -v -o tcp ./tcp.go

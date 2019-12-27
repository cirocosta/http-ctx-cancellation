package main

import (
	"context"
	"flag"
	"net"
	"os"
	"runtime"
	"runtime/pprof"
	"time"
)

var isServer = flag.Bool("server", false, "whether it should act as a server")

func mustNot(err error) {
	if err == nil {
		return
	}

	panic(err)
}

func server() {
	ln, err := net.Listen("tcp", ":1337")
	mustNot(err)

	defer ln.Close()

	time.Sleep(60 * time.Minute)
}

func client() {
	var (
		d      net.Dialer
		ctx, _ = context.WithTimeout(context.Background(), 1*time.Second)
	)

	conn, err := d.DialContext(ctx, "tcp", "localhost:1337")
	mustNot(err)

	defer conn.Close()
}

func writeProfile() {
	f, err := os.Create("heap.pprof")
	mustNot(err)

	defer f.Close()
	runtime.GC()

	err = pprof.WriteHeapProfile(f)
	mustNot(err)
}

func init() {
	runtime.MemProfileRate = 1
}

func main() {
	// defer writeProfile()

	flag.Parse()

	if *isServer {
		server()
		return
	}

	client()
	return
}

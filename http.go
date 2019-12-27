package main

import (
	"context"
	"flag"
	"net/http"
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
	http.HandleFunc("/", func(w http.ResponseWriter, r *http.Request) {
		time.Sleep(3 * time.Second)
	})

	http.ListenAndServe(":1337", nil)
}

func client() {
	var (
		ctx, _ = context.WithTimeout(context.Background(), 1*time.Second)
		client = http.DefaultClient
	)

	req, err := http.NewRequestWithContext(ctx, "GET", "http://localhost:1337", nil)
	mustNot(err)

	_, err = client.Do(req)
	mustNot(err)
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

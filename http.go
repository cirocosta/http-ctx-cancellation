package main

import (
	"context"
	"flag"
	"fmt"
	"io/ioutil"
	"net/http"
	"os"
	"os/signal"
	"runtime"
	"runtime/pprof"
	"syscall"
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
		// time.Sleep(1 * time.Second)
		fmt.Fprintln(w, "yoo")
	})

	http.ListenAndServe(":1337", nil)
}

func client() {
	var (
		ctx, cancel = context.WithCancel(context.Background())
		client      = http.DefaultClient
	)

	go func() {
		sigs := make(chan os.Signal, 1)
		signal.Notify(sigs, syscall.SIGUSR1)

		<-sigs
		cancel()
	}()

	req, err := http.NewRequestWithContext(ctx, "GET", "http://localhost:1337", nil)
	mustNot(err)

	resp, err := client.Do(req)
	mustNot(err)

	b, err := ioutil.ReadAll(resp.Body)
	mustNot(err)

	fmt.Printf("STATUS: %d %s\n", resp.StatusCode, resp.Status)
	fmt.Printf("BODY: %s\n", string(b))
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

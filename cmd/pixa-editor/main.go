// Command pixa-editor serves the local PIXA animation editor.
package main

import (
	"embed"
	"errors"
	"flag"
	"io/fs"
	"log"
	"net"
	"net/http"
	"os/exec"
	"runtime"
)

//go:embed web/index.html
var editorFiles embed.FS

func main() {
	address := flag.String("addr", "127.0.0.1:4173", "HTTP listen address")
	noBrowser := flag.Bool("no-browser", false, "do not open the editor in a browser")
	flag.Parse()

	content, err := fs.Sub(editorFiles, "web")
	if err != nil {
		log.Fatal(err)
	}
	listener, err := net.Listen("tcp", *address)
	if err != nil {
		log.Fatal(err)
	}
	url := "http://" + listener.Addr().String()
	if !*noBrowser {
		go func() {
			if err := openBrowser(url); err != nil {
				log.Printf("open PIXA editor: %v", err)
			}
		}()
	}
	log.Printf("PIXA editor listening on %s", url)
	log.Fatal(http.Serve(listener, http.FileServer(http.FS(content))))
}

func openBrowser(url string) error {
	command, err := browserCommand(url)
	if err != nil {
		return err
	}
	return command.Run()
}

func browserCommand(url string) (*exec.Cmd, error) {
	switch runtime.GOOS {
	case "darwin":
		return exec.Command("open", url), nil
	case "linux":
		return exec.Command("xdg-open", url), nil
	case "windows":
		return exec.Command("rundll32", "url.dll,FileProtocolHandler", url), nil
	default:
		return nil, errors.New("opening a browser is not supported on this platform")
	}
}

// Command pixa-editor serves the local PIXA animation editor.
package main

import (
	"embed"
	"flag"
	"io/fs"
	"log"
	"net/http"
)

//go:embed web/index.html
var editorFiles embed.FS

func main() {
	address := flag.String("addr", "127.0.0.1:4173", "HTTP listen address")
	flag.Parse()

	content, err := fs.Sub(editorFiles, "web")
	if err != nil {
		log.Fatal(err)
	}
	log.Printf("PIXA editor listening on http://%s", *address)
	log.Fatal(http.ListenAndServe(*address, http.FileServer(http.FS(content))))
}

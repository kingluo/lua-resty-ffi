package main

import (
	"encoding/binary"
	"log"
	"net"
	"os"
	"syscall"
)

const SockAddr = "/tmp/echo.sock"

func echoServer(c net.Conn) {
	header := make([]byte, 4)
	buf := make([]byte, 1024*1024*10)
	out := make([]byte, 1024*1024*10)
	for {
		n, err := c.Read(header)
		if err != nil {
			break
		}
		length := binary.BigEndian.Uint32(header)
		read := 0
		for uint32(read) < length {
			n, err = c.Read(buf[read:length])
			if err != nil {
				break
			}
			read += n
		}

		binary.BigEndian.PutUint32(out[:4], length)
		copy(out[4:], buf[:length])
		sent := 0
		for uint32(sent) < length+4 {
			n, err = c.Write(out[sent : length+4])
			if err != nil {
				break
			}
			sent += n
		}
	}
	c.Close()
}

func main() {
	if err := os.RemoveAll(SockAddr); err != nil {
		log.Fatal(err)
	}

	syscall.Umask(0)
	l, err := net.Listen("unix", SockAddr)
	if err != nil {
		log.Fatal("listen error:", err)
	}
	defer l.Close()
	for {
		conn, err := l.Accept()
		if err != nil {
			log.Fatal("accept error:", err)
		}

		go echoServer(conn)
	}
}

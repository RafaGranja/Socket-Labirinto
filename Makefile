all: server client

server: src/server.c 
	gcc -o bin/server src/server.c

client: src/client.c 
	gcc -o bin/client src/client.c

clean:
	rm -f bin/server bin/client

run-server:
	bin/server v4 51511 -i input/in.txt
run-client:
	bin/client 127.0.0.1 51511

git-update:
	git stash
	git pull
	make all

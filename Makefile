all: server client

server: src/server2.c 
	gcc -o bin/server src/server2.c

client: src/client2.c 
	gcc -o bin/client src/client2.c

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

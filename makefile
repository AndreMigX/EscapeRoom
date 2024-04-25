all: client server other

client: client.o lib/utils.o lib/game/shared.o lib/game/client.o
	gcc -Wall client.o lib/utils.o lib/game/shared.o lib/game/client.o -o client

server: server.o lib/utils.o lib/game/shared.o lib/game/server.o
	gcc -Wall server.o lib/utils.o lib/game/shared.o lib/game/server.o -o server

other: other.o lib/utils.o lib/game/shared.o lib/game/supervisor.o
	gcc -Wall other.o lib/utils.o lib/game/shared.o lib/game/supervisor.o -o other

clean:
	rm *o lib/*o lib/game/*o
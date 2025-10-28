CC = gcc

all: index_builder search_process ui_process

index_builder: index_builder.c murmurHash.c definitions.h
	$(CC) index_builder.c murmurHash.c -o index_builder

search_process: search_process.c murmurHash.c definitions.h
	$(CC) search_process.c murmurHash.c -o search_process

ui_process: ui_process.c definitions.h
	$(CC) ui_process.c -o ui_process

clean:
	rm -f index_builder search_process ui_process

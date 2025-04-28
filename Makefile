parser: bin/libvcparser.so

bin/libvcparser.so: VCParser.o LinkedListAPI.o | bin
	gcc -shared -o bin/libvcparser.so VCParser.o LinkedListAPI.o

VCParser.o: src/VCParser.c include/VCParser.h include/VCHelpers.h
	gcc -c -fPIC -Iinclude src/VCParser.c -o VCParser.o

LinkedListAPI.o: src/LinkedListAPI.c include/LinkedListAPI.h
	gcc -c -fPIC -Iinclude src/LinkedListAPI.c -o LinkedListAPI.o

clean:
	rm -f *.o bin/*.so

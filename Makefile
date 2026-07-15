INCLUDE=include/
SRC=src/
FLAGS=`llvm-config --cxxflags --ldflags --system-libs --libs all`

all: main.o lexer.o ast.o optimizer.o llvmStructs.o
	clang++ $(FLAGS) -rdynamic -g main.o lexer.o ast.o llvmStructs.o optimizer.o -o main

main.o: main.cpp
	clang++ $(FLAGS) -rdynamic -g -c main.cpp -o main.o

lexer.o: $(INCLUDE)lexer.hpp src/lexer.cpp
	clang++ $(FLAGS) -rdynamic -g src/lexer.cpp -c -o lexer.o

ast.o: $(INCLUDE)ast.hpp src/ast.cpp
	clang++ $(FLAGS) -rdynamic -g -c -o ast.o src/ast.cpp

optimizer.o: $(INCLUDE)optimizer.hpp src/optimizer.cpp
	clang++ $(FLAGS) -rdynamic -g -c -o optimizer.o src/optimizer.cpp

llvmStructs.o: $(INCLUDE)llvmStructs.hpp src/llvmStructs.cpp
	clang++ $(FLAGS) -rdynamic -g -c -o llvmStructs.o src/llvmStructs.cpp

test: test.cpp output.o
	clang++ test.cpp output.o -o test


clean:
	rm -f *.o main test
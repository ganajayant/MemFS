CPP = g++
CPPFLAGS = -std=c++11 -Wall -Wextra

PART1_SRC = src/main.cpp
PART1_EXEC = main
PART2_SRC = src/benchmark.cpp
PART2_EXEC = benchmark

.PHONY: all run benchmark build prune

run:
	$(CPP) $(CPPFLAGS) $(PART1_SRC) -o $(PART1_EXEC)
	./$(PART1_EXEC)

benchmark:
	$(CPP) $(CPPFLAGS) $(PART2_SRC) -o $(PART2_EXEC)
	./$(PART2_EXEC)

prune:
	rm -f $(PART1_EXEC) $(PART2_EXEC)
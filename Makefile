# Compiler
CC = gcc

# Compile Options
CFLAGS = -pthread -Werror -Wall

# Arguments
POLLER_ARG = 5635 8 16 poll-log poll-stats
SWAYER_ARG = linux06.di.uoa.gr 5635 inputFile

# Executables
EXEC_POLLER = poller
EXEC_SWAYER = pollSwayer

# Object files
OBJ_POLLER = poller.o 
OBJ_SWAYER = pollSwayer.o 

# Build both poller and pollSwayer executables
all: $(EXEC_POLLER) $(EXEC_SWAYER)

$(EXEC_POLLER): $(OBJ_POLLER)
	$(CC) $(CFLAGS) $(OBJ_POLLER) -o $(EXEC_POLLER) -lm

$(EXEC_SWAYER): $(OBJ_SWAYER)
	$(CC) $(CFLAGS) $(OBJ_SWAYER) -o $(EXEC_SWAYER)

# Run the poller program with the arguments
run_poller: $(EXEC_POLLER)
	./$(EXEC_POLLER) $(POLLER_ARG)

# Run the pollSwayer program with the arguments
run_swayer: $(EXEC_SWAYER)
	./$(EXEC_SWAYER) $(SWAYER_ARG)

# Run the poller program with Valgrind
valgrind_poller: $(EXEC_POLLER)
	valgrind --leak-check=full ./$(EXEC_POLLER) $(POLLER_ARG)

# Run the pollSwayer program with Valgrind
valgrind_swayer: $(EXEC_SWAYER)
	valgrind --leak-check=full ./$(EXEC_SWAYER) $(SWAYER_ARG)

# Remove object files and executables
clean:
	rm -f $(OBJ_POLLER) $(OBJ_SWAYER) $(EXEC_POLLER) $(EXEC_SWAYER) poll-log poll-stats

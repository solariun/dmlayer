# Makefile

IDIR =.
CC=g++
CPPFLAGS += -fprofile-arcs -ftest-coverage  \
			-I$(CPPUTEST_HOME)/include  \
			-I$(IDIR)					\
			-I"../src"					\
			-g3

ODIR=obj
LDIR =../lib
DMLAYER_DIR=../src
DMLAYER_OBJ=$(ODIR)/DMLayer.o

LIBS=-lm
LD_LIBRARIES = -L$(CPPUTEST_HOME)/lib -lCppUTest -lCppUTestExt

#_DEPS = hellomake.h
_DEPS = 
DEPS = $(patsubst %,$(IDIR)/%,$(_DEPS))

_OBJ = main.o testSystem.o testNumberVariables.o testBinaryVariables.o
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))
#$(info $$OBJ = $(OBJ))

unit_test: $(OBJ) $(DMLAYER_OBJ)
	$(CC) -o $@ $^ $(CPPFLAGS) $(LD_LIBRARIES) $(LIBS)

$(DMLAYER_OBJ): $(DMLAYER_DIR)/DMLayer.c
	$(CC) -c -o $@ $< $(CPPFLAGS) $(LD_LIBRARIES)

#$(ODIR)/%.o: %.c $(DEPS)
$(OBJ): $(ODIR)/%.o: %.cpp $(DEPS)
	mkdir -p obj
	$(CC) -c -o $@ $< $(CPPFLAGS) $(LD_LIBRARIES)

.PHONY: clean
clean:
	rm -rf $(ODIR)/* *~ core unit_test $(INCDIR)/*~

cov:
	# Generate the coverage only for the src files
	cd $(ODIR) && \
		lcov --c --directory . --include "*src*" --output-file main_coverage.info && \
		genhtml main_coverage.info --output-directory out

run test:
	./unit_test -v -c
	$(info testing)

.PHONY: docker
docker:
	docker images | grep "dmlayer-test"
	docker build -t dmlayer-test docker/
	cd .. && pwd && docker run -v /home/djunho/personal-workspace/dmlayer:/dmlayer -it dmlayer-test

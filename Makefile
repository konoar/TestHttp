############################################
#
# HTTP Test
#   Copyright 2019.10.27 konoar
#
############################################

SHELL   := /bin/bash
TESTBIN := ./testbin
LDFLAGS :=-lpthread

.PHONY: clean run

run: $(TESTBIN)
	@-sudo $(TESTBIN)

clean:
	@-rm *.o
	@-rm $(TESTBIN)

$(TESTBIN): ksMain.o ksHttp.o ksBuffer.o
	@gcc -o $(TESTBIN) $^ $(LDFLAGS)

%.o: %.c
	@gcc -o $@ -c $^


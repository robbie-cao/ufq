all: target
FORCE: ;
.PHONY: FORCE

target:	ufqd ufs ufs2

%.o: %.c
	@$(CC) $(CFLAGS) -c $< -o $@

%.o: %.cpp
	@$(CXX) $(CFLAGS) -c $< -o $@

%.o: %.cxx
	@$(CXX) $(CFLAGS) -c $< -o $@

ufqd: ufqd.o log.o filequeue.o
	$(CC) $^ $(LDFLAGS) -o $@

ufs: ufs.o log.o
	$(CC) $^ $(LDFLAGS) -o $@

ufs2: ufs2.o log.o
	$(CC) $^ $(LDFLAGS) -o $@

clean:
	rm -f *.o $(target)


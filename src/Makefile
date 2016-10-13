all: target
FORCE: ;
.PHONY: FORCE

target:	ufqd

%.o: %.c
	@$(CC) $(CFLAGS) -c $< -o $@

%.o: %.cpp
	@$(CXX) $(CFLAGS) -c $< -o $@

%.o: %.cxx
	@$(CXX) $(CFLAGS) -c $< -o $@

ufqd: ufqd.o log.o filequeue.o
	$(CC) $(LDFLAGS) $^ -o $@

clean:
	rm -f *.o $(target)


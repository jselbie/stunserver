.PHONY: all everything copybin debug clean

all: everything copybin

everything:
	$(MAKE) $(T) --directory=common
	$(MAKE) $(T) --directory=stuncore
	$(MAKE) $(T) --directory=networkutils
	$(MAKE) $(T) --directory=testcode
	$(MAKE) $(T) --directory=client
	$(MAKE) $(T) --directory=server

copybin: everything
	rm -f ./stunserver ./stunclient ./stuntestcode
	cp server/stunserver .
	cp client/stunclient .
	cp testcode/stuntestcode .


debug: T := debug
debug: all

profile: T := profile
profile: all

pgo1: T := pgo1
pgo1: all

pgo2: T := pgo2
pgo2: all

clean:	T := clean
clean: everything
	rm -f stunserver stunclient stuntestcode




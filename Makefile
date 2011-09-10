.PHONY: all everything copybin debug clean

all: everything copybin

everything:
	$(MAKE) $(T) --directory=common
	$(MAKE) $(T) --directory=stuncore
	$(MAKE) $(T) --directory=networkutils
	$(MAKE) $(T) --directory=testcode
	$(MAKE) $(T) --directory=client
	$(MAKE) $(T) --directory=server

copybin:
	rm -f ./stunserver ./stunclient ./stuntestcode
	cp server/stunserver .
	cp client/stunclient .
	cp testcode/stuntestcode .


debug: T := debug
debug: all


clean:	T := clean
clean: everything
	rm -f stunserver stunclient stuntestcode




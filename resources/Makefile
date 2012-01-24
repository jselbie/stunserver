include ../common.inc



%.txtcode: %.txt
	sh makecodefile.sh $^ $@ $(*)_text

%.txt: %.md
	pandoc --no-wrap -s -w plain -o $@ $^

%.1: %.md
	pandoc -s -w man -o $@ $^


all: stunserver.txtcode README stunclient.txtcode stunserver.1 stunclient.1

README: readme.src
	../stuntestcode --pp < readme.src > README

clean:
	rm -f *.txtcode *.txt README *.1



include ../common.inc



%.txtcode: %.txt
	sh makecodefile.sh $^ $@ $(*)_text

%.txt: %.md
	cat $^ | sed s/\*\*//g | pandoc --no-wrap -s -w plain -o $@ 

%.1: %.md
	pandoc -s -w man -o $@ $^


all:
	@echo "specify one of the following make options: textres manpages README or clean"

textres: stunserver.txtcode stunserver_lite.txtcode stunclient.txtcode stunclient_lite.txtcode

manpages: stunserver.1 stunclient.1

README: readme.src
	../stuntestcode --pp < readme.src > README

clean:
	rm -f *.txtcode README *.1



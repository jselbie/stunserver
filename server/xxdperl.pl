#!/usr/bin/perl


# xxdperl.pl
# emulates "xxd -i" in perl (stdin and stdout only)

$count = 0;

while ($input = <STDIN>) {
   @characters = split(//, $input);

   foreach (@characters) {

        if ($count != 0) {
            print ',';
            if (($count % 12)==0) {
                print "\n ";
            }
        }
        else {
           print ' ';
        }

        print sprintf(" 0x%.2x", ord($_));
        $count++;
    }
}



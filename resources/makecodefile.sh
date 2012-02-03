#
#   Copyright 2011 John Selbie
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.

plfile=`dirname $0`/xxdperl.pl

echo BUILDING $1 INTO $2
echo const char $3[] = { > $2
#xxd -i < $1 >> $2
echo "perl $plfile <" $1 ">>" $2
perl $plfile  < $1 >> $2
echo ",0x00};" >> $2
echo "" >> $2




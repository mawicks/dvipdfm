rm -f *.o
export CFLAGS='-pg -g -a -O3 --pedantic'
export LDFLAGS='-pg -g'
./configure --prefix=/home/mwicks --datadir=/home/mwicks
make

# Makefile for ipupdate
#
# This Makefile is not to be used on windows systems

CC+=-Wall -O3 -s

# compile targets

all: ipupdate dollardns whatismyip ifip failover

ipupdate: ipupdate.o config.o base64.o dns.o md5.o memm.o array.o \
          tcp.o daemonize.o ipc.o
	$(CC) -o ipupdate ipupdate.o config.o base64.o dns.o md5.o \
              memm.o array.o tcp.o daemonize.o ipc.o

dollardns: getip/dollardns.c tcp.o
	$(CC) -o dollardns getip/dollardns.c tcp.o

whatismyip: getip/whatismyip.c tcp.o
	$(CC) -o whatismyip getip/whatismyip.c tcp.o

ifip: getip/ifip.c
	$(CC) -o ifip getip/ifip.c

failover: getip/failover.c tcp.o
	$(CC) -o failover getip/failover.c tcp.o

# 
# the main module and modules that depend on it
#

ipupdate.o: ipupdate.c ipupdate.h
	$(CC) -c ipupdate.c

config.o: config.c config.h
	$(CC) -c config.c

# 
# independant modules
#

base64.o: include/base64.c include/base64.h
	$(CC) -c include/base64.c

dns.o: include/dns.c include/dns.h
	$(CC) -c include/dns.c

md5.o: include/md5.c include/md5.h
	$(CC) -c include/md5.c

memm.o: include/memm.c include/memm.h
	$(CC) -c include/memm.c

array.o: include/array.c include/array.h
	$(CC) -c include/array.c

tcp.o: include/tcp.c include/tcp.h
	$(CC) -c include/tcp.c

#
# platform dependant modules
#

daemonize.o: include/linux/daemonize.c include/linux/daemonize.h
	$(CC) -c include/linux/daemonize.c

ipc.o: include/linux/ipc.c include/linux/ipc.h
	$(CC) -c include/linux/ipc.c

#
# utility commands
#

clean:
	-rm ipupdate dollardns whatismyip ifip failover *.o

install: all
	cp -f ipupdate /usr/sbin
	mkdir -p /etc/ipupdate
	cp -f dollardns whatismyip ifip failover /etc/ipupdate
	@echo "installing man pages"
	@-if [ -x /usr/share/man ]; then \
		cp -f man/getip.7 /usr/share/man/man7/getip.7; \
		cp -f man/ipupdate.8 /usr/share/man/man8/ipupdate.8; \
		cp -f man/ipupdate.conf.5 /usr/share/man/man5/ipupdate.conf.5; \
	elif [ -x /usr/local/man ]; then \
		cp -f man/getip.7 /usr/local/man/man7/getip.7; \
		cp -f man/ipupdate.8 /usr/local/man/man8/ipupdate.8; \
		cp -f man/ipupdate.conf.5 /usr/local/man/man5/ipupdate.conf.5; \
	else \
		cp -f man/getip.7 /usr/man/man7/getip.7; \
		cp -f man/ipupdate.8 /usr/man/man8/ipupdate.8; \
		cp -f man/ipupdate.conf.5 /usr/man/man5/ipupdate.conf.5; fi
	cp -i ipupdate.conf /etc
	chmod 640 /etc/ipupdate.conf

uninstall:
	-rm /usr/sbin/ipupdate
	-rm -r /etc/ipupdate
	@echo "uninstalling man pages"
	@-if [ -x /usr/share/man ]; then \
		rm /usr/share/man/man7/getip.7; \
		rm /usr/share/man/man8/ipupdate.8; \
		rm /usr/share/man/man5/ipupdate.conf.5; \
	elif [ -x /usr/local/man ]; then \
		rm /usr/local/man/man7/getip.7 \
		rm /usr/local/man/man8/ipupdate.8; \
		rm /usr/local/man/man5/ipupdate.conf.5; \
	else \
		rm /usr/man/man7/getip.7; \
		rm /usr/man/man8/ipupdate.8; \
		rm /usr/man/man5/ipupdate.conf.5; fi
	rm -i /etc/ipupdate.conf

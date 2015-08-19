AM_CFLAGS = -Wall --std=c99 -O2
AM_LDFLAGS = 

bin_PROGRAMS = argos
check_PROGRAMS = mnl

argos_SOURCES = main.c cpu.c disk.c history.c memory.c net.c snapshot.c time.c transform.c util.c
argos_LDADD = -lmnl
argos_LDFLAGS = -lpcap -pthread

mnl_SOURCES = rtnl-link-dump.c
mnl_LDADD = -lmnl

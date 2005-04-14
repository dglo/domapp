.SUFFIXES: .c .o .elf .srec .xml .html .tab .bin

HEADERS = version.h lbm.h

.c.o: $(HEADERS)
	$(CC) -c -I. $(CFLAGS) $<

.S.o:
	$(CPP) $(CPPFLAGS) -o $*.i $<
	$(AS) $(AFLAGS) -o $*.o $*.i

.elf.bin:
	$(OBJCOPY) -O binary $*.elf $*.bin
	$(CPP) $(CPPFLAGS) -DBINFILE=\"$*.bin\" -o $*-raw.i $(RAWS)
	$(AS) $(AFLAGS) -o $*-raw.o $*-raw.i
	$(LD) --script=$(RAWX) -o $*-raw.elf $*-raw.o
	$(OBJCOPY) -O binary $*-raw.elf $*.bin

all: ../domapp.bin.gz

clean:
	cd ../../dom-ws/epxa10/domapp && rm -f *.o *.i *.bin *.elf

msgHandler.o: msgHandler.c version.h

OBJS = \
        commonServices.o     domSControlRoutines.o \
        compressEvent.o      expControl.o          \
        dataAccess.o         messageBuffers.o      \
        dataAccessRoutines.o genericMsgSendRecv.o  \
        DOMdataCompression.o moniDataAccess.o      \
        domSControl.o        msgHandler.o          \
        engFormat.o          lbm.o                 \
        message.o


../bin/domapp: $(OBJS)
	gcc -o domapp  $(LIBHAL) $(OBJS) -lm

KOBJS = ../lib/crt0.o ../lib/libkernel.a

domapp.elf: domapp.o $(OBJS) $(LIBHAL)
	$(LD) --script=$(KERNELX) -o domapp.elf \
		$(KOBJS) domapp.o $(OBJS) $(LIBHAL) $(SYSLIBS)

../bin/%.bin.gz: %.bin; gzip -c $< > $@

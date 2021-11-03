# Makefile for the boot monitor package.

SYS	= ..
#https://users.informatik.haw-hamburg.de/~krabat/FH-Labor/gnupro/2_GNUPro_Compiler_Tools/Using_GNU_CC/gccIntel_386_Options.html
#-mcpu=cpu type’ options are defined for Intel’s i386 family of computers
#https://docs.oracle.com/cd/E19957-01/806-3567/cc_options.html cc编译器参考
#-s Removes all symbolic debugging information from the output object file.
# -.o含义待查询
CC	= exec cc
CC86	= exec cc -mi86 -Was-ncc
CFLAGS	= -I$(SYS) -O
LIBS	= -lsys
LD	= $(CC) -s -.o
LD86	= $(CC86) -.o
BIN	= /usr/bin
MDEC	= /usr/mdec

all:	bootblock boot edparams masterboot jumpboot installboot
dos:	boot.com mkfile.com

bootblock:	bootblock.s
	$(LD86) -com -o $@ bootblock.s

masterboot:	masterboot.s
	$(LD86) -com -o $@ masterboot.s

jumpboot:	jumpboot.s
	$(LD86) -com -o $@ jumpboot.s

boot.o:	boot.c
	$(CC86) $(CFLAGS) -c boot.c

bootimage.o:	bootimage.c
	$(CC86) $(CFLAGS) -c bootimage.c

rawfs86.o:	rawfs.c rawfs.o
	ln -f rawfs.c rawfs86.c
	#生成rawfs86.o中间文件
	$(CC86) $(CFLAGS) -c rawfs86.c
	rm rawfs86.c
	#如果文件相同，则给出值 0，如果不同，则给出值 1，或者如果发生错误，则给出值 2
	#&&，&& 之前命令执行成功(即$?==0)时才执行之后的命令,因此当 rawfs.o rawfs86.o文件完全相同时创建软链接指向rawfs.o
	#只有rawfs.o rawfs86.o都是i86平台才会相同，在32位平台编译时rawfs.o为32，rawfs86.o为16位
	-cmp -s rawfs.o rawfs86.o && ln -f rawfs.o rawfs86.o

boot:	boothead.s boot.o bootimage.o rawfs86.o
	$(LD86) -o $@ \
		boothead.s boot.o bootimage.o rawfs86.o $(LIBS)
	install -S 12kb boot

edparams.o:	boot.c
	ln -f boot.c edparams.c
	$(CC) $(CFLAGS) -DUNIX -c edparams.c
	rm edparams.c

edparams:	edparams.o rawfs.o
	$(CC) $(CFLAGS) $(STRIP) -o $@ edparams.o rawfs.o
	install -S 16kw edparams

dosboot.o:	boot.c
	$(CC86) $(CFLAGS) -DDOS -o $@ -c boot.c

doshead.o:	doshead.s
	$(CC) -mi386 -o $@ -c doshead.s

dosboot:	doshead.o dosboot.o bootimage.o rawfs86.o
	$(LD86) -com -o $@ \
		doshead.o dosboot.o bootimage.o rawfs86.o $(LIBS)

boot.com:	dosboot
	./a.out2com dosboot boot.com

mkfile:	mkfhead.s mkfile.c
	$(LD) -.o -mi86 -com -o $@ mkfhead.s mkfile.c $(LIBS)

mkfile.com:	mkfile
	./a.out2com mkfile mkfile.com

installboot:	installboot.o rawfs.o
	$(CC) $(STRIP) -o installboot installboot.o rawfs.o
	install -S 6kw installboot
#命令自动推导会生成对应名称的目标文件
#多规则的目标,重建此目标的命令只能出现在一个规则中(可以是多条命令) 因此生成rawfs.o为推导生成32位目标文件，boot.o为i86(16位)平台目标文件
installboot.o bootimage.o: image.h
boot.o bootimage.o dosboot.o edparams.o: boot.h
rawfs.o rawfs86.o installboot.o boot.o bootimage.o: rawfs.h

install:	$(MDEC)/bootblock $(MDEC)/boot $(MDEC)/masterboot \
		$(MDEC)/jumpboot $(BIN)/installboot $(BIN)/edparams
dosinstall:	$(MDEC)/boot.com $(MDEC)/mkfile.com

$(MDEC)/bootblock:	bootblock
	install -cs -o bin -m 644 $? $@

$(MDEC)/boot:		boot
	install -cs -o bin -m 644 $? $@

$(MDEC)/boot.com:	boot.com
	install -c -m 644 $? $@

$(MDEC)/mkfile.com:	mkfile.com
	install -c -m 644 $? $@

$(MDEC)/masterboot:	masterboot
	install -cs -o bin -m 644 $? $@

$(MDEC)/jumpboot:	jumpboot
	install -cs -o bin -m 644 $? $@

$(BIN)/installboot:	installboot
	install -cs -o bin $? $@

$(BIN)/edparams:	edparams
	install -cs -o bin $? $@

clean:
	rm -f *.bak *.o
	rm -f bootblock installboot boot masterboot jumpboot edparams
	rm -f dosboot boot.com mkfile mkfile.com

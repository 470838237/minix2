!	masterboot 2.0 - Master boot block code		Author: Kees J. Bot
!
! This code may be placed in the first sector (the boot sector) of a floppy,
! hard disk or hard disk primary partition.  There it will perform the
! following actions at boot time:
!
! - If the booted device is a hard disk and one of the partitions is active
!   then the active partition is booted.
!
! - Otherwise the next floppy or hard disk device is booted, trying them one
!   by one.
!
! To make things a little clearer, the boot path might be:
!	/dev/fd0	- Floppy disk containing data, tries fd1 then d0
!	[/dev/fd1]	- Drive empty
!	/dev/c0d0	- Master boot block, selects active partition 2
!	/dev/c0d0p2	- Submaster, selects active subpartition 0
!	/dev/c0d0p2s0	- Minix bootblock, reads Boot Monitor /boot
!	Minix		- Started by /boot from a kernel image in /minix

	LOADOFF	   =	0x7C00	! 0x0000:LOADOFF is where this code is loaded
	BUFFER	   =	0x0600	! First free memory
	PART_TABLE =	   446	! Location of partition table within this code
	PENTRYSIZE =	    16	! Size of one partition table entry
	MAGIC	   =	   510	! Location of the AA55 magic number

	! <ibm/partition>.h:
	bootind	   =	     0
	sysind	   =	     4
	lowsec	   =	     8 !分区表项中逻辑起始扇区低16位相对分区表项偏移地址,参考硬盘分区DPT详解文档


.text

! Find active (sub)partition, load its first sector, run it.

master:
	xor	ax, ax
	mov	ds, ax
	mov	es, ax
	cli
	mov	ss, ax			! ds = es = ss = Vector segment
	mov	sp, #LOADOFF
	sti

! Copy this code to safety, then jump to it.
	mov	si, sp			! si = start of this code
	push	si			! Also where we'll return to eventually
	mov	di, #BUFFER		! Buffer area
	mov	cx, #512/2		! One sector
	cld
  rep	movs
	jmpf	BUFFER+migrate, 0	! To safety
migrate:

! Find the active partition
findactive:
	testb	dl, dl
	jns	nextdisk		! No bootable partitions on floppies
	mov	si, #BUFFER+PART_TABLE
find:	cmpb	sysind(si), #0		! Partition type, nonzero when in use
	jz	nextpart
	testb	bootind(si), #0x80	! Active partition flag in bit 7
	jz	nextpart		! It's not active
loadpart:
	call	load			! Load partition bootstrap
	jc	error1			! Not supposed to fail
bootstrap:
	ret				! Jump to the master bootstrap  ret指令pop栈数据至ip寄存器，此处pop的数据是上文保存的si寄存器值0x7C00，ret后执行0x7C00的指令
nextpart:
	add	si, #PENTRYSIZE
	cmp	si, #BUFFER+PART_TABLE+4*PENTRYSIZE
	jb	find
! No active partition, tell 'em
	call	print
	.ascii	"No active partition\0"
	jmp	reboot

! There are no active partitions on this drive, try the next drive.
nextdisk:
	incb	dl			! Increment dl for the next drive
	testb	dl, dl
	js	nexthd			! Hard disk if negative
	int	0x11			! Get equipment configuration
	shl	ax, #1			! Highest floppy drive # in bits 6-7
	shl	ax, #1			! Now in bits 0-1 of ah
	andb	ah, #0x03		! Extract bits
	cmpb	dl, ah			! Must be dl <= ah for drive to exist
	ja	nextdisk		! Otherwise try disk 0 eventually
	call	load0			! Read the next floppy bootstrap
	jc	nextdisk		! It failed, next disk please
	ret				! Jump to the next master bootstrap
nexthd:	call	load0			! Read the hard disk bootstrap
error1:	jc	error			! No disk?
	ret


! Load sector 0 from the current device.  It's either a floppy bootstrap or
! a hard disk master bootstrap.
load0:
	mov	si, #BUFFER+zero-lowsec	! si = where lowsec(si) is zero  如果是软盘si+lowsec执行标号zero的地址 (起始4字节记录了起始扇区偏移)
	!jmp	load

! Load sector lowsec(si) from the current device.  The obvious head, sector,
! and cylinder numbers are ignored in favour of the more trustworthy absolute
! start of partition.
load:
	mov	di, #3		! Three retries for floppy spinup
retry:	push	dx		! Save drive code
	push	es
	push	di		! Next call destroys es and di
	movb	ah, #0x08	! Code for drive parameters
	int	0x13        ! 驱动器对应于硬盘或软盘，而不是分区，bios并不知道分区存在，如何分区
	pop	di
	pop	es
	andb	cl, #0x3F	! cl = max sector number (1-origin)
	incb	dh		! dh = 1 + max head number (0-origin)  此处是指硬盘总磁头数和磁道数
	movb	al, cl		! al = cl = sectors per track
	mulb	dh		! dh = heads, ax = heads * sectors
	mov	bx, ax		! bx = sectors per cylinder = heads * sectors
	mov	ax, lowsec+0(si)
	mov	dx, lowsec+2(si)! dx:ax = sector within drive  引导块起始扇区逻辑地址
	cmp	dx, #[1024*255*63-255]>>16  ! Near 8G limit? 分区扇区数目为dx:ax,dx为扇区的高16位，#[1024*255*63-255]>>16取高16位和dx比较
	jae	bigdisk
	div	bx		! ax = cylinder, dx = sector within cylinder
	xchg	ax, dx		! ax = sector within cylinder, dx = cylinder
	movb	ch, dl		! ch = low 8 bits of cylinder
	divb	cl		! al = head, ah = sector (0-origin)
	xorb	dl, dl		! About to shift bits 8-9 of cylinder into dl
	shr	dx, #1
	shr	dx, #1		! dl[6..7] = high cylinder
	orb	dl, ah		! dl[0..5] = sector (0-origin)
	movb	cl, dl		! cl[0..5] = sector, cl[6..7] = high cyl
	incb	cl		! cl[0..5] = sector (1-origin)
	pop	dx		! Restore drive code in dl
	movb	dh, al		! dh = al = head
	mov	bx, #LOADOFF	! es:bx = where sector is loaded
	mov	ax, #0x0201	! Code for read, just one sector
	int	0x13		! Call the BIOS for a read   引导块逻辑起始地址小于8G时使用0x02读写扇区数据，0x02功能通过扇区+柱面+磁头定位读写区域起始地址
	jmp	rdeval		! Evaluate read result       当0x02功能传参磁道位置为10位因此磁道号大于1024时不能使用该功能(不能传参)，因为需要使用扩展读写(cmp dx, #[1024*255*63-255]>>16)
bigdisk:
	mov	bx, dx		! bx:ax = dx:ax = sector to read
	pop	dx		! Restore drive code in dl
	push	si		! Save si
	mov	si, #BUFFER+ext_rw ! si = extended read/write parameter packet
	mov	8(si), ax	! Starting block number = bx:ax
	mov	10(si), bx
	movb	ah, #0x42	! Extended read  扩展读使用逻辑起始扇区+要读写的扇区数定位读写磁盘区域,引导块逻辑起始地址大于8G时使用0x42读写扇区数据
	int	0x13            ! 将引导块程序加载到内存0x7C00
	pop	si		! Restore si to point to partition entry
	!jmp	rdeval
rdeval:
	jnc	rdok		! Read succeeded
	cmpb	ah, #0x80	! Disk timed out?  (Floppy drive empty)
	je	rdbad
	dec	di
	jl	rdbad		! Retry count expired
	xorb	ah, ah
	int	0x13		! Reset
	jnc	retry		! Try again
rdbad:	stc			! Set carry flag
	ret
rdok:	cmp	LOADOFF+MAGIC, #0xAA55
	jne	nosig		! Error if signature wrong
	ret			! Return with carry still clear
nosig:	call	print
	.ascii	"Not bootable\0"
	jmp	reboot

! A read error occurred, complain and hang
error:
	mov	si, #LOADOFF+errno+1
prnum:	movb	al, ah		! Error number in ah
	andb	al, #0x0F	! Low 4 bits
	cmpb	al, #10		! A-F?
	jb	digit		! 0-9!
	addb	al, #7		! 'A' - ':'
digit:	addb	(si), al	! Modify '0' in string
	dec	si
	movb	cl, #4		! Next 4 bits
	shrb	ah, cl
	jnz	prnum		! Again if digit > 0
	call	print
	.ascii	"Read error "
errno:	.ascii	"00\0"
	!jmp	reboot

reboot:
	call	print
	.ascii	".  Hit any key to reboot.\0"
	xorb	ah, ah		! Wait for keypress
	int	0x16
	call	print
	.ascii	"\r\n\0"
	int	0x19

! Print a message.
print:	pop	si		! si = String following 'call print'
prnext:	lodsb			! al = *si++ is char to be printed
	testb	al, al		! Null marks end
	jz	prdone
	movb	ah, #0x0E	! Print character in teletype mode
	mov	bx, #0x0001	! Page 0, foreground color
	int	0x10
	jmp	prnext
prdone:	jmp	(si)		! Continue after the string

.data

! Extended read/write commands require a parameter packet.
ext_rw:
	.data1	0x10		! Length of extended r/w packet
	.data1	0		! Reserved
	.data2	1		! Blocks to transfer (just one)
	.data2	LOADOFF		! Buffer address offset
	.data2	0		! Buffer address segment
	.data4	0		! Starting block number low 32 bits (tbfi)
zero:	.data4	0		! Starting block number high 32 bits
!如果设备是软盘则标号zero:地址指向引导块起始扇区逻辑地址,此数据被installboot,install_master写入(猜测，也可能被make_bootable写入)
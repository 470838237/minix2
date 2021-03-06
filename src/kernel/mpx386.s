#
! This file contains the assembler startup code for Minix and the 32-bit
! interrupt handlers.  It cooperates with start.c to set up a good
! environment for main().

! This file is part of the lowest layer of the MINIX kernel.  The other part
! is "proc.c".  The lowest layer does process switching and message handling.

! Every transition to the kernel goes through this file.  Transitions are
! caused by sending/receiving messages and by most interrupts.  (RS232
! interrupts may be handled in the file "rs2.s" and then they rarely enter
! the kernel.)

! Transitions to the kernel may be nested.  The initial entry may be with a
! system call, exception or hardware interrupt; reentries may only be made
! by hardware interrupts.  The count of reentries is kept in "k_reenter".
! It is important for deciding whether to switch to the kernel stack and
! for protecting the message passing code in "proc.c".
! 中断可以被嵌套,初始中断可以是系统调用，异常和硬件中断，再入中断只能是硬件中断,再入中断保存在k_reenter中，
! k_reenter决定哪些再入中断可以被换入kernel堆栈,并且通过proc.c保存消息

!对于消息陷阱发生时(应该是系统调用),大部分机器状态被保存(部分寄存器未被保存)在proc table(进程表,用于进程管理),然后运行栈切换到k_stack(kernel stack)
!并且设置中断可被重入(即中断可以嵌套),然后执行系统调用的处理程序,当系统调用的处理程序执行完毕时，中断位再次被禁用,
!执行流程返回到mpx386.s _restart例程结束中断，然后执行proc_ptr指向的进程或任务程序
!硬件中断处理流程和消息陷阱基本一致除了以下几点:
!1 所有的状态必须被保存
!2
!3 已经切换到核心栈时避免再次切换(待理解)
!4 259硬件中断控制器在mpx386.s save例程中被再次职位(?待验证)
!5

!处理程序太多，无法内联执行此操作，因此调用save例程。通过按下适当的重启例程的地址以备稍后返回，可以节省一些周期
! For the message passing trap, most of the machine state is saved in the
! proc table.  (Some of the registers need not be saved.)  Then the stack is
! switched to "k_stack", and interrupts are reenabled.  Finally, the system
! call handler (in C) is called.  When it returns, interrupts are disabled
! again and the code falls into the restart routine, to finish off held-up
! interrupts and run the process or task whose pointer is in "proc_ptr".

! Hardware interrupt handlers do the same, except  (1) The entire state must
! be saved.  (2) There are too many handlers to do this inline, so the save
! routine is called.  A few cycles are saved by pushing the address of the
! appropiate restart routine for a return later.  (3) A stack switch is
! avoided when the stack is already switched.  (4) The (master) 8259 interrupt
! controller is reenabled centrally in save().  (5) Each interrupt handler
! masks its interrupt line using the 8259 before enabling (other unmasked)
! interrupts, and unmasks it after servicing the interrupt.  This limits the
! nest level to the number of lines and protects the handler from itself.

! For communication with the boot monitor at startup time some constant
! data are compiled into the beginning of the text segment. This facilitates 
! reading the data at the start of the boot process, since only the first
! sector of the file needs to be read.      flags: read by boot

! Some data storage is also allocated at the end of this file. This data 
! will be at the start of the data segment of the kernel and will be read
! and modified by the boot monitor before the kernel starts.

! sections

.sect .text
begtext:
.sect .rom
begrom:
.sect .data
begdata:
.sect .bss
begbss:

#include <minix/config.h>
#include <minix/const.h>
#include <minix/com.h>
#include "const.h"
#include "protect.h"
#include "sconst.h"

/* Selected 386 tss offsets. */
#define TSS3_S_SP0	4

! Exported functions
! Note: in assembly language the .define statement applied to a function name 
! is loosely equivalent to a prototype in C code -- it makes it possible to
! link to an entity declared in the assembly code but does not create
! the entity.

.define	_idle_task
.define	_restart
.define	save

.define	_divide_error
.define	_single_step_exception
.define	_nmi
.define	_breakpoint_exception
.define	_overflow
.define	_bounds_check
.define	_inval_opcode
.define	_copr_not_available
.define	_double_fault
.define	_copr_seg_overrun
.define	_inval_tss
.define	_segment_not_present
.define	_stack_exception
.define	_general_protection
.define	_page_fault
.define	_copr_error

.define	_hwint00	! handlers for hardware interrupts
.define	_hwint01
.define	_hwint02
.define	_hwint03
.define	_hwint04
.define	_hwint05
.define	_hwint06
.define	_hwint07
.define	_hwint08
.define	_hwint09
.define	_hwint10
.define	_hwint11
.define	_hwint12
.define	_hwint13
.define	_hwint14
.define	_hwint15

.define	_s_call
.define	_p_s_call
.define	_level0_call

! Exported variables.
.define	begbss
.define	begdata

.sect .text
!*===========================================================================*
!*				MINIX					     *
!*===========================================================================*
MINIX:				! this is the entry point for the MINIX kernel
	jmp	over_flags	! skip over the next few bytes
	.data2	CLICK_SHIFT	! for the monitor: memory granularity
flags:
	.data2	0x01FD		! boot monitor flags:
				!	call in 386 mode, make bss, make stack,
				!	load high, don`t patch, will return,
				!	uses generic INT, memory vector,
				!	new boot code return
	nop			! extra byte to sync up disassembler
over_flags:

! Set up a C stack frame on the monitor stack.  (The monitor sets cs and ds
! right.  The ss descriptor still references the monitor data segment.)
	movzx	esp, sp		! monitor stack is a 16 bit stack
	push	ebp
	mov	ebp, esp
	push	esi
	push	edi
	cmp	4(ebp), 0	! monitor return vector is
	jz	noret		! nonzero if return possible
	inc	(_mon_return)
noret:	mov	(_mon_sp), esp	! save stack pointer for later return

! Copy the monitor global descriptor table to the address space of kernel and
! switch over to it.  Prot_init() can then update it with immediate effect.
    ! mon_sp  mon_return gdt 定义在kernel/glo.h
	sgdt	(_gdt+GDT_SELECTOR)		! get the monitor gdtr
	mov	esi, (_gdt+GDT_SELECTOR+2)	! absolute address of GDT
	mov	ebx, _gdt			! address of kernel GDT
	mov	ecx, 8*8			! copying eight descriptors
copygdt:
 eseg	movb	al, (esi)
	movb	(ebx), al
	inc	esi
	inc	ebx
	loop	copygdt
	mov	eax, (_gdt+DS_SELECTOR+2)	! base of kernel data
	and	eax, 0x00FFFFFF			! only 24 bits
	add	eax, _gdt			! eax = vir2phys(gdt)数据段的基址加_gdt(gdt偏移地址) = gdt的物理地址
	mov	(_gdt+GDT_SELECTOR+2), eax	! set base of GDT  将gdt的物理地址设置为gdt描述符表的基址
	lgdt	(_gdt+GDT_SELECTOR)		! switch over to kernel GDT

! Locate boot parameters, set up kernel segment registers and stack.
	mov	ebx, 8(ebp)	! boot parameters offset
	mov	edx, 12(ebp)	! boot parameters length
	mov	eax, 16(ebp)	! address of a.out headers
	mov	(_aout), eax
	mov	ax, ds		! kernel data
	mov	es, ax
	mov	fs, ax
	mov	gs, ax
	mov	ss, ax
	mov	ss, ax
	mov	esp, k_stktop	! set sp to point to the top of kernel stack

! Call C startup code to set up a proper environment to run main().
	push	edx
	push	ebx
	push	SS_SELECTOR
	push	DS_SELECTOR
	push	CS_SELECTOR
	call	_cstart		! cstart(cs, ds, mds, parmoff, parmlen)
	add	esp, 5*4        !释放参数

! Reload gdtr, idtr and the segment registers to global descriptor table set
! up by prot_init().

	lgdt	(_gdt+GDT_SELECTOR)
	lidt	(_gdt+IDT_SELECTOR)

	jmpf	CS_SELECTOR:csinit
csinit:
    o16	mov	ax, DS_SELECTOR
	mov	ds, ax
	mov	es, ax
	mov	fs, ax
	mov	gs, ax
	mov	ss, ax
    o16	mov	ax, TSS_SELECTOR	! no other TSS is used
	ltr	ax
	push	0			! set flags to known good state
	popf				! esp, clear nested task and int enable

	jmp	_main			! main()


!*===========================================================================*
!*				interrupt handlers			     *
!*		interrupt handlers for 386 32-bit protected mode	     *
!*===========================================================================*

!*===========================================================================*
!*				hwint00 - 07				     *
!*===========================================================================*
! Note this is a macro, it just looks like a subroutine.
#define hwint_master(irq)	\
	call	save			/* save interrupted process state */;\
	inb	INT_CTLMASK						    ;\
	orb	al, [1<<irq]						    ;\
	outb	INT_CTLMASK		/* disable the irq		  */;\
	movb	al, ENABLE						    ;\
	outb	INT_CTL			/* reenable master 8259		  */;\
	push	(_irq_hooks+4*irq)	/* irq_hooks[irq]		  */;\
	sti				/* enable interrupts		  */;\
	call	_intr_handle		/* intr_handle(irq_hooks[irq])	  */;\
	cli				/* disable interrupts		  */;\
	pop	ecx							    ;\
	cmp	(_irq_actids+4*irq), 0	/* interrupt still active?	  */;\
	jnz	0f							    ;\
	inb	INT_CTLMASK						    ;\
	andb	al, ~[1<<irq]						    ;\
	outb	INT_CTLMASK		/* enable the irq		  */;\
0:	ret				/* restart (another) process      */

! Each of these entry points is an expansion of the hwint_master macro
	.align	16
_hwint00:		! Interrupt routine for irq 0 (the clock).
	hwint_master(0)

	.align	16
_hwint01:		! Interrupt routine for irq 1 (keyboard)
	hwint_master(1)

	.align	16
_hwint02:		! Interrupt routine for irq 2 (cascade!)
	hwint_master(2)

	.align	16
_hwint03:		! Interrupt routine for irq 3 (second serial)
	hwint_master(3)

	.align	16
_hwint04:		! Interrupt routine for irq 4 (first serial)
	hwint_master(4)

	.align	16
_hwint05:		! Interrupt routine for irq 5 (XT winchester)
	hwint_master(5)

	.align	16
_hwint06:		! Interrupt routine for irq 6 (floppy)
	hwint_master(6)

	.align	16
_hwint07:		! Interrupt routine for irq 7 (printer)
	hwint_master(7)

!*===========================================================================*
!*				hwint08 - 15				     *
!*===========================================================================*
! Note this is a macro, it just looks like a subroutine.
#define hwint_slave(irq)	\
	call	save			/* save interrupted process state */;\
	inb	INT2_CTLMASK						    ;\
	orb	al, [1<<[irq-8]]					    ;\
	outb	INT2_CTLMASK		/* disable the irq		  */;\
	movb	al, ENABLE						    ;\
	outb	INT_CTL			/* reenable master 8259		  */;\
	push	(_irq_hooks+4*irq)	/* irq_hooks[irq]		  */;\
	outb	INT2_CTL		/* reenable slave 8259		  */;\
	sti				/* enable interrupts		  */;\
	call	_intr_handle		/* intr_handle(irq_hooks[irq])	  */;\
	cli				/* disable interrupts		  */;\
	pop	ecx							    ;\
	cmp	(_irq_actids+4*irq), 0	/* interrupt still active?	  */;\
	jnz	0f							    ;\
	inb	INT2_CTLMASK						    ;\
	andb	al, ~[1<<[irq-8]]					    ;\
	outb	INT2_CTLMASK		/* enable the irq		  */;\
0:	ret				/* restart (another) process      */

! Each of these entry points is an expansion of the hwint_slave macro
	.align	16
_hwint08:		! Interrupt routine for irq 8 (realtime clock)
	hwint_slave(8)

	.align	16
_hwint09:		! Interrupt routine for irq 9 (irq 2 redirected)
	hwint_slave(9)

	.align	16
_hwint10:		! Interrupt routine for irq 10
	hwint_slave(10)

	.align	16
_hwint11:		! Interrupt routine for irq 11
	hwint_slave(11)

	.align	16
_hwint12:		! Interrupt routine for irq 12
	hwint_slave(12)

	.align	16
_hwint13:		! Interrupt routine for irq 13 (FPU exception)
	hwint_slave(13)

	.align	16
_hwint14:		! Interrupt routine for irq 14 (AT winchester)
	hwint_slave(14)

	.align	16
_hwint15:		! Interrupt routine for irq 15
	hwint_slave(15)

!*===========================================================================*
!*				save					     *
!*===========================================================================*
! Save for protected mode.
! This is much simpler than for 8086 mode, because the stack already points
! into the process table, or has already been switched to the kernel stack.

	.align	16
save:
	cld			! set direction flag to a known value
	pushad			! save "general" registers
    o16	push	ds		! save ds
    o16	push	es		! save es
    o16	push	fs		! save fs
    o16	push	gs		! save gs
	mov	dx, ss		! ss is kernel data segment
	mov	ds, dx		! load rest of kernel segments
	mov	es, dx		! kernel does not use fs, gs
	mov	eax, esp	! prepare to return
	incb	(_k_reenter)	! from -1 if not reentering
	jnz	set_restart1	! stack is already kernel stack
	mov	esp, k_stktop
	push	_restart	! build return address for int handler
	xor	ebp, ebp	! for stacktrace
	jmp	RETADR-P_STACKBASE(eax)  !硬件中断时call指令push save调用的下一条指令地址，使用jmp不用ret,是因为ret会弹出地址，但此处不需要该操作

	.align	4
set_restart1:
	push	restart1
	jmp	RETADR-P_STACKBASE(eax)

!*===========================================================================*
!*				_s_call					     *
!*===========================================================================*
	.align	16
_s_call:
_p_s_call:
	cld			! set direction flag to a known value
	sub	esp, 6*4	! skip RETADR, eax, ecx, edx, ebx, est
	push	ebp		! stack already points into proc table
	push	esi
	push	edi
    o16	push	ds
    o16	push	es
    o16	push	fs
    o16	push	gs
	mov	dx, ss
	mov	ds, dx
	mov	es, dx
	incb	(_k_reenter)
	mov	esi, esp	! assumes P_STACKBASE == 0
	mov	esp, k_stktop
	xor	ebp, ebp	! for stacktrace
				! end of inline save
	sti			! allow SWITCHER to be interrupted
				! now set up parameters for sys_call()
	push	ebx		! pointer to user message
	push	eax		! src/dest
	push	ecx		! SEND/RECEIVE/BOTH
	call	_sys_call	! sys_call(function, src_dest, m_ptr)
				! caller is now explicitly in proc_ptr
	mov	AXREG(esi), eax	! sys_call MUST PRESERVE si
	cli			! disable interrupts 

! Fall into code to restart proc/task running.

!*===========================================================================*
!*				restart					     *
!*===========================================================================*
_restart:

! Flush any held-up interrupts.
! This reenables interrupts, so the current interrupt handler may reenter.
! This does not matter, because the current handler is about to exit and no
! other handlers can reenter since flushing is only done when k_reenter == 0.

	cmp	(_held_head), 0	! do fast test to usually avoid function call
	jz	over_call_unhold
	call	_unhold		! this is rare so overhead acceptable
over_call_unhold:
	mov	esp, (_proc_ptr)	! will assume P_STACKBASE == 0
	lldt	P_LDT_SEL(esp)		! enable segment descriptors for task
	lea	eax, P_STACKTOP(esp)	! arrange for next interrupt
	mov	(_tss+TSS3_S_SP0), eax	! to save state in process table
restart1:
	decb	(_k_reenter)
    o16	pop	gs
    o16	pop	fs
    o16	pop	es
    o16	pop	ds
	popad           ! popad弹出通用寄存器eax, ebx,ecx,edx,esi,edi,ebp,esp
	add	esp, 4		! skip return adr
	iretd			! continue process

!*===========================================================================*
!*				exception handlers			     *
!*===========================================================================*
_divide_error:
	push	DIVIDE_VECTOR
	jmp	exception

_single_step_exception:
	push	DEBUG_VECTOR
	jmp	exception

_nmi:
	push	NMI_VECTOR
	jmp	exception

_breakpoint_exception:
	push	BREAKPOINT_VECTOR
	jmp	exception

_overflow:
	push	OVERFLOW_VECTOR
	jmp	exception

_bounds_check:
	push	BOUNDS_VECTOR
	jmp	exception

_inval_opcode:
	push	INVAL_OP_VECTOR
	jmp	exception

_copr_not_available:
	push	COPROC_NOT_VECTOR
	jmp	exception

_double_fault:
	push	DOUBLE_FAULT_VECTOR
	jmp	errexception

_copr_seg_overrun:
	push	COPROC_SEG_VECTOR
	jmp	exception

_inval_tss:
	push	INVAL_TSS_VECTOR
	jmp	errexception

_segment_not_present:
	push	SEG_NOT_VECTOR
	jmp	errexception

_stack_exception:
	push	STACK_FAULT_VECTOR
	jmp	errexception

_general_protection:
	push	PROTECTION_VECTOR
	jmp	errexception

_page_fault:
	push	PAGE_FAULT_VECTOR
	jmp	errexception

_copr_error:
	push	COPROC_ERR_VECTOR
	jmp	exception

!*===========================================================================*
!*				exception				     *
!*===========================================================================*
! This is called for all exceptions which do not push an error code.

	.align	16
exception:
 sseg	mov	(trap_errno), 0		! clear trap_errno
 sseg	pop	(ex_number)
	jmp	exception1

!*===========================================================================*
!*				errexception				     *
!*===========================================================================*
! This is called for all exceptions which push an error code.

	.align	16
errexception:
 sseg	pop	(ex_number)
 sseg	pop	(trap_errno)
exception1:				! Common for all exceptions.
	push	eax			! eax is scratch register
	mov	eax, 0+4(esp)		! old eip
 sseg	mov	(old_eip), eax
	movzx	eax, 4+4(esp)		! old cs
 sseg	mov	(old_cs), eax
	mov	eax, 8+4(esp)		! old eflags
 sseg	mov	(old_eflags), eax
	pop	eax
	call	save
	push	(old_eflags)
	push	(old_cs)
	push	(old_eip)
	push	(trap_errno)
	push	(ex_number)
	call	_exception		! (ex_number, trap_errno, old_eip,
					!	old_cs, old_eflags)
	add	esp, 5*4
	cli
	ret

!*===========================================================================*
!*				level0_call				     *
!*===========================================================================*
_level0_call:
	call	save
	jmp	(_level0_func)

!*===========================================================================*
!*				idle_task				     *
!*===========================================================================*
_idle_task:
! This task is called when the system has nothing else to do.  The HLT
! instruction puts the processor in a state where it draws minimum power.
	push	halt
	call	_level0		! level0(halt)
	pop	eax
	jmp	_idle_task
halt:
	sti
	hlt
	cli
	ret

!*===========================================================================*
!*				data					     *
!*===========================================================================*

.sect .rom	! Before the string table please
	.data2	0x526F		! this must be the first data entry (magic #)

.sect .bss
k_stack:
	.space	K_STACK_BYTES	! kernel stack
k_stktop:			! top of kernel stack
	.comm	ex_number, 4
	.comm	trap_errno, 4
	.comm	old_eip, 4
	.comm	old_cs, 4
	.comm	old_eflags, 4

/* This file contains the main program of the File System.  It consists of
 * a loop that gets messages requesting work, carries out the work, and sends
 * replies.
 *
 * The entry points into this file are
 *   main:	main program of the File System
 *   reply:	send a reply to a process after the requested work is done
 */

struct super_block;		/* proto.h needs to know this */

#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <sys/ioc_memory.h>
#include <sys/svrctl.h>
#include <minix/callnr.h>
#include <minix/com.h>
#include <minix/const.h>
#include <minix/config.h>
#include "buf.h"
#include "dev.h"
#include "file.h"
#include "fproc.h"
#include "inode.h"
#include "param.h"
#include "super.h"
#include "fs.h"

FORWARD _PROTOTYPE( void buf_pool, (void)				);
FORWARD _PROTOTYPE( void fs_init, (void)				);
FORWARD _PROTOTYPE( int igetenv, (char *var)				);
FORWARD _PROTOTYPE( void get_work, (void)				);
FORWARD _PROTOTYPE( void load_ram, (void)				);
FORWARD _PROTOTYPE( void load_super, (Dev_t super_dev)			);


/*===========================================================================*
 *				main					     *
 *===========================================================================*/
PUBLIC void main()
{
/* This is the main program of the file system.  The main loop consists of
 * three major activities: getting new work, processing the work, and sending
 * the reply.  This loop never terminates as long as the file system runs.
 */
  int error;

  fs_init();

  /* This is the main loop that gets work, processes it, and sends replies. */
  while (TRUE) {
	get_work();		/* sets who and fs_call */

	fp = &fproc[who];	/* pointer to proc table struct */
	super_user = (fp->fp_effuid == SU_UID ? TRUE : FALSE);   /* su? */

	/* Call the internal function that does the work. */
	if (fs_call < 0 || fs_call >= NCALLS)
		error = ENOSYS;
	else
		error = (*call_vec[fs_call])();

	/* Copy the results back to the user and send reply. */
	if (error != SUSPEND) reply(who, error);
	if (rdahed_inode != NIL_INODE) read_ahead(); /* do block read ahead */
  }
}


/*===========================================================================*
 *				get_work				     *
 *===========================================================================*/
PRIVATE void get_work()
{  
  /* Normally wait for new input.  However, if 'reviving' is
   * nonzero, a suspended process must be awakened.
   */

  register struct fproc *rp;

  if (reviving != 0) {
	/* Revive a suspended process. */
	for (rp = &fproc[0]; rp < &fproc[NR_PROCS]; rp++) 
		if (rp->fp_revived == REVIVING) {
			who = (int)(rp - fproc);
			fs_call = rp->fp_fd & BYTE;
			fd = (rp->fp_fd >>8) & BYTE;
			buffer = rp->fp_buffer;
			nbytes = rp->fp_nbytes;
			rp->fp_suspended = NOT_SUSPENDED; /*no longer hanging*/
			rp->fp_revived = NOT_REVIVING;
			reviving--;
			return;
		}
	panic("get_work couldn't revive anyone", NO_NUM);
  }

  /* Normal case.  No one to revive. */
  if (receive(ANY, &m) != OK) panic("fs receive error", NO_NUM);

  who = m.m_source;
  fs_call = m.m_type;
}


/*===========================================================================*
 *				reply					     *
 *===========================================================================*/
PUBLIC void reply(whom, result)
int whom;			/* process to reply to */
int result;			/* result of the call (usually OK or error #) */
{
/* Send a reply to a user process. It may fail (if the process has just
 * been killed by a signal), so don't check the return code.  If the send
 * fails, just ignore it.
 */

  reply_type = result;
  send(whom, &m1);
}


/*===========================================================================*
 *				fs_init					     *
 *===========================================================================*/
PRIVATE void fs_init()
{
/* Initialize global variables, tables, etc. */

  register struct inode *rip;
  int i;
  message mess;
  
  /* The following initializations are needed to let dev_opcl succeed .*/
  fp = (struct fproc *) NULL;
  who = FS_PROC_NR;

  buf_pool();			/* initialize buffer pool */
  load_ram();			/* init RAM disk, load if it is root */
  load_super(root_dev);		/* load super block for root device */

  /* Initialize the 'fproc' fields for process 0 .. INIT. */
  for (i = 0; i <= LOW_USER; i+= 1) {
	if (i == FS_PROC_NR) continue;	/* do not initialize FS */
	fp = &fproc[i];
	rip = get_inode(root_dev, ROOT_INODE);
	fp->fp_rootdir = rip;
	dup_inode(rip);//???????????????????????????root??????
	fp->fp_workdir = rip;
	fp->fp_realuid = (uid_t) SYS_UID;
	fp->fp_effuid = (uid_t) SYS_UID;
	fp->fp_realgid = (gid_t) SYS_GID;
	fp->fp_effgid = (gid_t) SYS_GID;
	fp->fp_umask = ~0;
	fp->fp_pid = i < LOW_USER ? PID_SERVER : 1;
  }

  /* Certain relations must hold for the file system to work at all. */
  if (SUPER_SIZE > BLOCK_SIZE) panic("SUPER_SIZE > BLOCK_SIZE", NO_NUM);
  if (BLOCK_SIZE % V2_INODE_SIZE != 0)	/* this checks V1_INODE_SIZE too */
	panic("BLOCK_SIZE % V2_INODE_SIZE != 0", NO_NUM);
  if (OPEN_MAX > 127) panic("OPEN_MAX > 127", NO_NUM);
  if (NR_BUFS < 6) panic("NR_BUFS < 6", NO_NUM);
  if (V1_INODE_SIZE != 32) panic("V1 inode size != 32", NO_NUM);
  if (V2_INODE_SIZE != 64) panic("V2 inode size != 64", NO_NUM);
  if (OPEN_MAX > 8 * sizeof(long)) panic("Too few bits in fp_cloexec", NO_NUM);

  /* Tell the memory task where my process table is for the sake of ps(1). */
  mess.m_type = DEV_IOCTL;
  mess.PROC_NR = FS_PROC_NR;
  mess.DEVICE = RAM_DEV;
  mess.REQUEST = MIOCSPSINFO;
  mess.ADDRESS = (void *) fproc;
  (void) sendrec(MEM, &mess);
}


/*===========================================================================*
 *				buf_pool				     *
 *===========================================================================*/
PRIVATE void buf_pool()
{
/* Initialize the buffer pool. */

  register struct buf *bp;

  bufs_in_use = 0;
  front = &buf[0];
  rear = &buf[NR_BUFS - 1];

  for (bp = &buf[0]; bp < &buf[NR_BUFS]; bp++) {
	bp->b_blocknr = NO_BLOCK;
	bp->b_dev = NO_DEV;
	bp->b_next = bp + 1;
	bp->b_prev = bp - 1;
  }
  buf[0].b_prev = NIL_BUF;
  buf[NR_BUFS - 1].b_next = NIL_BUF;

  for (bp = &buf[0]; bp < &buf[NR_BUFS]; bp++) bp->b_hash = bp->b_next;
  buf_hash[0] = front;
}


/*===========================================================================*
 *				igetenv					     *
 *===========================================================================*/
PRIVATE int igetenv(var)
char *var;
{
/* Ask kernel for an integer valued boot environment variable. */
  struct sysgetenv sysgetenv;
  char value[64];

  sysgetenv.key = var;
  sysgetenv.keylen = strlen(var)+1;
  sysgetenv.val = value;
  sysgetenv.vallen = sizeof(value);
  (void) sys_sysctl(FS_PROC_NR, SYSGETENV, 1, (vir_bytes) &sysgetenv);
  return(atoi(value));
}


/*===========================================================================*
 *				load_ram				     *
 *===========================================================================*/
PRIVATE void load_ram()
{
/* If the root device is the RAM disk, copy the entire root image device
 * block-by-block to a RAM disk with the same size as the image.
 * Otherwise, just allocate a RAM disk with size given in the boot parameters.
 */

  register struct buf *bp, *bp1;
  u32_t lcount, ram_size, fsmax;
  zone_t zones;
  struct super_block *sp, *dsp;
  block_t b;
  Dev_t image_dev;

  /* Get some boot environment variables. */
  //if ((dev = name2dev(value)) == -1) return 0;  bootimage.c
  root_dev = igetenv("rootdev");
  image_dev = igetenv("ramimagedev");
  ram_size = igetenv("ramsize");

  /* Open the root device. */
  if (dev_open(root_dev, FS_PROC_NR, R_BIT|W_BIT) != OK)
	panic("Cannot open root device",NO_NUM);

  /* If the root device is the ram disk then fill it from the image device. */
  if (root_dev == DEV_RAM) {
	if (dev_open(image_dev, FS_PROC_NR, R_BIT) != OK)
		panic("Cannot open RAM image device", NO_NUM);

	/* Get size of RAM disk by reading root file system's super block. */
	sp = &super_block[0];
	sp->s_dev = image_dev;
	if (read_super(sp) != OK) panic("Bad root file system", NO_NUM);

	lcount = sp->s_zones << sp->s_log_zone_size;	/* # blks on root dev*/

	/* Stretch the RAM disk file system to the boot parameters size, but
	 * no further than the last zone bit map block allows.
	 */
	if (ram_size < lcount) ram_size = lcount;
	fsmax = (u32_t) sp->s_zmap_blocks * CHAR_BIT * BLOCK_SIZE;//s_zmap_blocks only include data blocks
	fsmax = (fsmax + (sp->s_firstdatazone-1)) << sp->s_log_zone_size;
	if (ram_size > fsmax) ram_size = fsmax;
  }

  /* Tell RAM driver how big the RAM disk must be. */
  m1.m_type = DEV_IOCTL;
  m1.PROC_NR = FS_PROC_NR;
  m1.DEVICE = RAM_DEV;
  m1.REQUEST = MIOCRAMSIZE;
  m1.POSITION = ram_size;
  if (sendrec(MEM, &m1) != OK || m1.REP_STATUS != OK)
	panic("Can't set RAM disk size", NO_NUM);

  /* Tell MM the RAM disk size, and wait for it to come "on-line". */
  m1.m1_i1 = ((long) ram_size * BLOCK_SIZE) >> CLICK_SHIFT;
  if (sendrec(MM_PROC_NR, &m1) != OK)
	panic("FS can't sync up with MM", NO_NUM);

#if ENABLE_CACHE2
  /* The RAM disk is a second level block cache while not otherwise used. */
  init_cache2(ram_size);
#endif

  /* If the root device is not the RAM disk, it doesn't need loading. */
  if (root_dev != DEV_RAM) return;

  /* Copy the blocks one at a time from the image to the RAM disk. */
  printf("Loading RAM disk.\33[23CLoaded:    0K ");

  inode[0].i_mode = I_BLOCK_SPECIAL;	/* temp inode for rahead() */
  inode[0].i_size = LONG_MAX;
  inode[0].i_dev = image_dev;
  inode[0].i_zone[0] = image_dev;

  for (b = 0; b < (block_t) lcount; b++) {
	bp = rahead(&inode[0], b, (off_t)BLOCK_SIZE * b, BLOCK_SIZE);
	bp1 = get_block(root_dev, b, NO_READ);
	memcpy(bp1->b_data, bp->b_data, (size_t) BLOCK_SIZE);
	bp1->b_dirt = DIRTY;
	put_block(bp, FULL_DATA_BLOCK);
	put_block(bp1, FULL_DATA_BLOCK);
	printf("\b\b\b\b\b\b\b%5ldK ", ((long) b * BLOCK_SIZE)/1024L);
  }

  printf("\rRAM disk loaded.\33[K\n\n");

  /* Invalidate and close the image device. */
  invalidate(image_dev);
  dev_close(image_dev);

  /* Resize the RAM disk root file system. */
  bp = get_block(root_dev, SUPER_BLOCK, NORMAL);
  dsp = (struct super_block *) bp->b_data;
  zones = ram_size >> sp->s_log_zone_size;
  dsp->s_nzones = conv2(sp->s_native, (u16_t) zones);
  dsp->s_zones = conv4(sp->s_native, zones);
  bp->b_dirt = DIRTY;
  put_block(bp, ZUPER_BLOCK);
}


/*===========================================================================*
 *				load_super				     *
 *===========================================================================*/
PRIVATE void load_super(super_dev)
dev_t super_dev;			/* place to get superblock from */
{
  int bad;
  register struct super_block *sp;
  register struct inode *rip;

  /* Initialize the super_block table. */
  for (sp = &super_block[0]; sp < &super_block[NR_SUPERS]; sp++)
  	sp->s_dev = NO_DEV;

  /* Read in super_block for the root file system. */
  sp = &super_block[0];
  sp->s_dev = super_dev;

  /* Check super_block for consistency. */
  bad = (read_super(sp) != OK);
  if (!bad) {
	rip = get_inode(super_dev, ROOT_INODE);	/* inode for root dir */
	if ( (rip->i_mode & I_TYPE) != I_DIRECTORY || rip->i_nlinks < 3) bad++;
  }
  if (bad) panic("Invalid root file system", NO_NUM);

  sp->s_imount = rip;
  dup_inode(rip);//??????root????????????????????????
  sp->s_isup = rip;
  sp->s_rd_only = 0;
  return;
}

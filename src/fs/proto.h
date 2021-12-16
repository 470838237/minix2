/* Function prototypes. */

/* Structs used in prototypes must be declared as such first. */
struct buf;
struct filp;
struct inode;
struct super_block;

//申请分配zmap
/* cache.c */
_PROTOTYPE(zone_t alloc_zone, (Dev_t dev, zone_t z));
//将所有藏块集中起来写入磁盘
_PROTOTYPE(void flushall, (Dev_t dev));
//释放numb对应的zmap
_PROTOTYPE(void free_zone, (Dev_t dev, zone_t numb));
//从缓存中读取block,不存在时从dev读取,引用书+1
_PROTOTYPE(struct buf *get_block, (Dev_t dev, block_t block,int only_search));
//使block无效
_PROTOTYPE(void invalidate, (Dev_t device));
//将磁盘放回缓存,脏块根据情况是否写入设备中,引用书-1
_PROTOTYPE(void put_block, (struct buf *bp, int block_type));
//从设备读入block或将block写入设备
_PROTOTYPE(void rw_block, (struct buf *bp, int rw_flag));
//将flushall搜集的脏块排序,然后写入设备,之所以一起写入并排序是为了高效写入
_PROTOTYPE(void rw_scattered, (Dev_t dev,
        struct buf **bufq, int bufqsize, int rw_flag));

#if ENABLE_CACHE2
/* cache2.c */
_PROTOTYPE(void init_cache2, (unsigned long size));

_PROTOTYPE(int get_block2, (struct buf *bp, int only_search));

_PROTOTYPE(void put_block2, (struct buf *bp));

_PROTOTYPE(void invalidate2, (Dev_t device));

#endif

/* device.c */
_PROTOTYPE(int dev_open, (Dev_t dev, int proc, int flags));

_PROTOTYPE(void dev_close, (Dev_t dev));

_PROTOTYPE(int dev_io, (int op, Dev_t dev, int proc, void *buf,
        off_t pos, int bytes, int flags));

_PROTOTYPE(int gen_opcl, (int op, Dev_t dev, int proc, int flags));

_PROTOTYPE(void gen_io, (int task_nr, message *mess_ptr));

_PROTOTYPE(int no_dev, (int op, Dev_t dev, int proc, int flags));

_PROTOTYPE(int tty_opcl, (int op, Dev_t dev, int proc, int flags));

_PROTOTYPE(int ctty_opcl, (int op, Dev_t dev, int proc, int flags));

_PROTOTYPE(int clone_opcl, (int op, Dev_t dev, int proc, int flags));

_PROTOTYPE(void ctty_io, (int task_nr, message *mess_ptr));

_PROTOTYPE(int do_ioctl, (void));

_PROTOTYPE(int do_setsid, (void));

/* filedes.c */
_PROTOTYPE(struct filp *find_filp, (struct inode *rip, Mode_t bits));

_PROTOTYPE(int get_fd, (int start, Mode_t bits, int *k, struct filp **fpt));

_PROTOTYPE(struct filp *get_filp, (int fild));

/* inode.c */
//搜寻imap找到空闲inum然后根据inum定位inode,如果缓存没有数据则读取磁盘块
_PROTOTYPE(struct inode *alloc_inode, (Dev_t dev, Mode_t bits));
//ip的引用数+1
_PROTOTYPE(void dup_inode, (struct inode *ip));
//释放 numb的imap
_PROTOTYPE(void free_inode, (Dev_t dev, Ino_t numb));
//先从缓存中读取numb没有从磁盘中读取inode引用数+1
_PROTOTYPE(struct inode *get_inode, (Dev_t dev, int numb));
//将rip放回缓存,引用数-1,如果需要写入磁盘则写入磁盘
_PROTOTYPE(void put_inode, (struct inode *rip));

_PROTOTYPE(void update_times, (struct inode *rip));
//从磁盘中读取到rip或写入rip到磁盘
_PROTOTYPE(void rw_inode, (struct inode *rip, int rw_flag));
//将rip所有i_zone置0
_PROTOTYPE(void wipe_inode, (struct inode *rip));

/* link.c */
_PROTOTYPE(int do_link, (void));

_PROTOTYPE(int do_unlink, (void));

_PROTOTYPE(int do_rename, (void));
//释放rip占用的zone的zmap
_PROTOTYPE(void truncate, (struct inode *rip));

/* lock.c */
_PROTOTYPE(int lock_op, (struct filp *f, int req));

_PROTOTYPE(void lock_revive, (void));

/* main.c */
_PROTOTYPE(void main, (void));

_PROTOTYPE(void reply, (int whom, int result));

/* misc.c */
_PROTOTYPE(int do_dup, (void));

_PROTOTYPE(int do_exit, (void));

_PROTOTYPE(int do_fcntl, (void));

_PROTOTYPE(int do_fork, (void));

_PROTOTYPE(int do_exec, (void));

_PROTOTYPE(int do_revive, (void));

_PROTOTYPE(int do_set, (void));

_PROTOTYPE(int do_sync, (void));

_PROTOTYPE(int do_reboot, (void));

_PROTOTYPE(int do_svrctl, (void));

/* mount.c */
_PROTOTYPE(int do_mount, (void));

_PROTOTYPE(int do_umount, (void));

_PROTOTYPE(int unmount, (Dev_t dev));

/* open.c */
_PROTOTYPE(int do_close, (void));

_PROTOTYPE(int do_creat, (void));

_PROTOTYPE(int do_lseek, (void));

_PROTOTYPE(int do_mknod, (void));

_PROTOTYPE(int do_mkdir, (void));

_PROTOTYPE(int do_open, (void));

/* path.c */
//查询目录节点dirp下文件名为string节点,不存在返回0
_PROTOTYPE(struct inode *advance, (struct inode *dirp, char string[NAME_MAX]));
//flag为Lookup时,搜索目录ldir_ptr下文件名为string,存在时将节点号赋值给numb,不存在错误码置ENOENT
//flag为delete时,搜索目录ldir_ptr下文件名为string,存在时删除该节点,不存在错误码置ENOENT 未使用到numb,numb传递0
//flag为ENTER时,在ldir_ptr目录下为numb创建目录项
//flag为EMPTY时,判断ldir_ptr目录是否为空目录即只包含.. 和.
_PROTOTYPE(int search_dir, (struct inode *ldir_ptr,
        char string[NAME_MAX], ino_t *numb, int flag));
//返回路径最后的文件名节点inode
_PROTOTYPE(struct inode *eat_path, (char *path));
//返回路劲最后的目录节点inode,string赋值为文件名
_PROTOTYPE(struct inode *last_dir, (char *path, char string[NAME_MAX]));

/* pipe.c */
_PROTOTYPE(int do_pipe, (void));

_PROTOTYPE(int do_unpause, (void));

_PROTOTYPE(int pipe_check, (struct inode *rip, int rw_flag,
        int oflags, int bytes, off_t position, int *canwrite));

_PROTOTYPE(void release, (struct inode *ip, int call_nr, int count));

_PROTOTYPE(void revive, (int proc_nr, int bytes));

_PROTOTYPE(void suspend, (int task));

/* protect.c */
_PROTOTYPE(int do_access, (void));

_PROTOTYPE(int do_chmod, (void));

_PROTOTYPE(int do_chown, (void));

_PROTOTYPE(int do_umask, (void));

_PROTOTYPE(int forbidden, (struct inode *rip, Mode_t access_desired));

_PROTOTYPE(int read_only, (struct inode *ip));

/* read.c */
_PROTOTYPE(int do_read, (void));

//从磁盘读取baseblock同时根据bytes_ahead和position等条件判断预读更多的磁盘块到缓存
_PROTOTYPE(struct buf *rahead, (struct inode *rip, block_t baseblock,
        off_t position, unsigned bytes_ahead));
//文件系统调用时适当根据需求调用rahead裕度磁盘到缓存备用提高磁盘访问速度
_PROTOTYPE(void read_ahead, (void));
//根据rip对应文件的相对文件起始位置position定位该position相对于分区的绝对block位置
_PROTOTYPE(block_t read_map, (struct inode *rip, off_t position));
//文件读取和写入操作
_PROTOTYPE(int read_write, (int rw_flag));
//
_PROTOTYPE(zone_t rd_indir, (struct buf *bp, int index));

/* stadir.c */
_PROTOTYPE(int do_chdir, (void));

_PROTOTYPE(int do_chroot, (void));

_PROTOTYPE(int do_fstat, (void));

_PROTOTYPE(int do_stat, (void));
//分配一个空闲的zmap或imap号
/* super.c */
_PROTOTYPE(bit_t alloc_bit, (struct super_block *sp, int map, bit_t origin));
//释放一个zmap或imap号
_PROTOTYPE(void free_bit, (struct super_block *sp, int map,
        bit_t bit_returned));

_PROTOTYPE(struct super_block *get_super, (Dev_t dev));

_PROTOTYPE(int mounted, (struct inode *rip));

_PROTOTYPE(int read_super, (struct super_block *sp));

/* time.c */
_PROTOTYPE(int do_stime, (void));

_PROTOTYPE(int do_time, (void));

_PROTOTYPE(int do_tims, (void));

_PROTOTYPE(int do_utime, (void));

/* utility.c */
_PROTOTYPE(time_t clock_time, (void));

_PROTOTYPE(unsigned conv2, (int norm, int w));

_PROTOTYPE(long conv4, (int norm, long x));

_PROTOTYPE(int fetch_name, (char *path, int len, int flag));

_PROTOTYPE(int no_sys, (void));

_PROTOTYPE(void panic, (char *format, int num));

/* write.c */
_PROTOTYPE(void clear_zone, (struct inode *rip, off_t pos, int flag));

_PROTOTYPE(int do_write, (void));
//在rip对应文件中搜索相对文件position对应的zone号如果zone存在则在zone中分配一个新的block,如果不存在zone则申请分配新的zone,再从新的zone分配新的block
_PROTOTYPE(struct buf *new_block, (struct inode *rip, off_t position));
//block数据清0
_PROTOTYPE(void zero_block, (struct buf *bp));

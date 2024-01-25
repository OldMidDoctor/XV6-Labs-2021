struct buf {
  int valid;   // has data been read from disk?
               // indicates that the buffer contains a copy of the block or not
  int disk;    // does disk "own" buf?
               // indicates that the buffer content has been handed to the disk
               // if disk == 1, wait for virtio_disk_intr() to say request has finished
               // if disk == 0, then disk is done with buf
  uint dev;
  uint blockno; // block number, but more exactly, is the sector number
  struct sleeplock lock;   // 每个缓存块一把睡眠锁
  uint refcnt;      // 当前有多少个内核线程在排队等待读这个缓存块
  struct buf *prev; // LRU cache list
  struct buf *next;
  uchar data[BSIZE];  // BSIZE = 1024，xv6的块大小是1024B
};


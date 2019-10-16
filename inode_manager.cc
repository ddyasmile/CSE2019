#include <time.h>
#include <iostream>
#include "inode_manager.h"

// disk layer -----------------------------------------

disk::disk()
{
  bzero(blocks, sizeof(blocks));
}

void
disk::read_block(blockid_t id, char *buf)
{
  //std::cout << "read_block" << id << std::endl;
  memcpy(buf, blocks[id], BLOCK_SIZE);
}

void
disk::write_block(blockid_t id, const char *buf)
{
  //std::cout << "write_block" << id << std::endl;
  memcpy(blocks[id], buf, BLOCK_SIZE);
}

// block layer -----------------------------------------

// Allocate a free disk block.
blockid_t
block_manager::alloc_block(uint32_t type)
{
  /*
   * your code goes here.
   * note: you should mark the corresponding bit in block bitmap when alloc.
   * you need to think about which block you can start to be allocated.
   */
  char buf[BLOCK_SIZE];
  uint32_t local_block_num = 0;
  uint32_t start, end;

  if (type == INODE) {
    start = INODE_START;
    end = DATA_START;
  }else if (type == DATA) {
    start = DATA_START;
    end = BLOCK_NUM;
  }else{
    return 0;
  }

  for (uint32_t i = start; i < end; i++) {
    if (local_block_num!=BBLOCK(i)) {
      local_block_num = BBLOCK(i);
      d->read_block(local_block_num, buf);
    }
    //printf("alloc_block::local_block_num: %d\n", local_block_num);
    //0 means free
    //printf("alloc_block::check: %d\n", (int)CHECK(buf, BBLOCK_INDEX(i)));
    //printf("%p %d %d\n", buf+BBLOCK_INDEX(i)/8, *(buf+BBLOCK_INDEX(i)/8), 
            //*(buf+BBLOCK_INDEX(i)/8)>>(8-BBLOCK_INDEX(i)%8-1));

    if (!CHECK(buf, BBLOCK_INDEX(i))) {
      char *p = buf + BBLOCK_INDEX(i)/8;
      //printf("alloc_block:buf: %p\n", buf);
      //printf("alloc_block::p:*p: %p %x\n", p, *(unsigned int*)p);
      *p = *p | (0x1<<(8-BBLOCK_INDEX(i)%8-1));
      //printf("alloc_block::p:*p: %p %x\n", p, *(unsigned int*)p);
      //printf("alloc_block::*buf+BBLOCK_INDEX(i)/8: %x\n", *(unsigned int*)(buf+BBLOCK_INDEX(i)/8));
      d->write_block(local_block_num, buf);

      //printf("alloc_block::i: %d\n", i);

      return i;
    }
  }

  return 0;
}

void
block_manager::free_block(uint32_t id)
{
  /* 
   * your code goes here.
   * note: you should unmark the corresponding bit in the block bitmap when free.
   */
  char buf[BLOCK_SIZE];
  char *p;

  if (id < INODE_START || id >= BLOCK_NUM)
    return;

  d->read_block(BBLOCK(id), buf);
  p = buf + BBLOCK_INDEX(id)/8;
  *p = ~(~(*p) | (0x1<<(8-BBLOCK_INDEX(id)%8-1)));
  d->write_block(BBLOCK(id), buf);

  return;
}

// The layout of disk should be like this:
// |<-sb->|<-free block bitmap->|<-inode table->|<-data->|
block_manager::block_manager()
{
  d = new disk();

  // format the disk
  sb.size = BLOCK_SIZE * BLOCK_NUM;
  sb.nblocks = BLOCK_NUM;
  sb.ninodes = INODE_NUM;

}

void
block_manager::read_block(uint32_t id, char *buf)
{
  d->read_block(id, buf);
}

void
block_manager::write_block(uint32_t id, const char *buf)
{
  d->write_block(id, buf);
}

// inode layer -----------------------------------------

inode_manager::inode_manager()
{
  bm = new block_manager();
  uint32_t root_dir = alloc_inode(extent_protocol::T_DIR);
  if (root_dir != 1) {
    printf("\tim: error! alloc first inode %d, should be 1\n", root_dir);
    exit(0);
  }
}

/* Create a new file.
 * Return its inum. */
uint32_t
inode_manager::alloc_inode(uint32_t type)
{
  /* 
   * your code goes here.
   * note: the normal inode block should begin from the 2nd inode block.
   * the 1st is used for root_dir, see inode_manager::inode_manager().
   */

  uint32_t iblock_num = bm->alloc_block(INODE);
  uint32_t bblock_num = bm->alloc_block(DATA);
  time_t now_time = time(NULL);

  inode_t *tem_inode = (inode_t *)malloc(sizeof(inode_t));
  tem_inode->type = (short)type;
  tem_inode->size = 0;
  tem_inode->atime = (unsigned int)now_time;
  tem_inode->mtime = (unsigned int)now_time;
  tem_inode->ctime = (unsigned int)now_time;
  tem_inode->blocks[0] = bblock_num;

  char inode[BLOCK_SIZE];
  memcpy(inode, tem_inode, sizeof(inode_t));
  free(tem_inode);

  bm->write_block(iblock_num, inode);

  //printf("alloc_inode::iblock_num:inum %d %d\n", iblock_num, iblock_num - INODE_START + 1);

  return iblock_num - INODE_START + 1;
}

void
inode_manager::free_inode(uint32_t inum)
{
  /* 
   * your code goes here.
   * note: you need to check if the inode is already a freed one;
   * if not, clear it, and remember to write back to disk.
   */
  inode_t *inode = get_inode(inum);
  inode->type = 0;
  put_inode(inum, inode);
  
  bm->free_block(IBLOCK(inum, bm->sb.nblocks));
  
  int cur = 0;
  int cur_index = 0;
  int file_size = inode->size;
  while(file_size > 0) {
    if (cur_index > NDIRECT-1) {
      inode_t *next_inode = get_inode(inode->blocks[NDIRECT]);
      next_inode->size = file_size;
      put_inode(inode->blocks[NDIRECT], next_inode);
      free(next_inode);
      free_inode(inode->blocks[NDIRECT]);
      break;
    }

    bm->free_block(inode->blocks[cur_index]);
    file_size -= BLOCK_SIZE;
    cur++;
    cur_index++;
  }
  free(inode);
  return;
}


/* Return an inode structure by inum, NULL otherwise.
 * Caller should release the memory. */
struct inode* 
inode_manager::get_inode(uint32_t inum)
{
  struct inode *ino, *ino_disk;
  char buf[BLOCK_SIZE];

  printf("\tim: get_inode %d\n", inum);

  if (inum < 0 || inum >= INODE_NUM) {
    printf("\tim: inum out of range\n");
    return NULL;
  }

  //printf("get_inode:block_num: %d\n", IBLOCK(inum, bm->sb.nblocks));
  bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
  // printf("%s:%d\n", __FILE__, __LINE__);

  ino_disk = (struct inode*)buf + inum%IPB;
  if (ino_disk->type == 0) {
    //printf("\tim: inode not exist\n");
    //return NULL;
  }

  ino = (struct inode*)malloc(sizeof(struct inode));
  *ino = *ino_disk;

  return ino;
}

void
inode_manager::put_inode(uint32_t inum, struct inode *ino)
{
  char buf[BLOCK_SIZE];
  struct inode *ino_disk;

  printf("\tim: put_inode %d\n", inum);
  if (ino == NULL)
    return;

  bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
  ino_disk = (struct inode*)buf + inum%IPB;
  *ino_disk = *ino;
  bm->write_block(IBLOCK(inum, bm->sb.nblocks), buf);
}

#define MIN(a,b) ((a)<(b) ? (a) : (b))

/* Get all the data of a file by inum. 
 * Return alloced data, should be freed by caller. */
void
inode_manager::read_file(uint32_t inum, char **buf_out, int *size)
{
  /*
   * your code goes here.
   * note: read blocks related to inode number inum,
   * and copy them to buf_Out
   */
  printf("\tim: read_file::inum: %d\n", inum);

  int flag = 1, cur = 0, cur_block_indx = 0;
  inode_t *file_inode = get_inode(inum);

  int file_size = file_inode->size;
  *size = file_size;
  char *buf = (char *)malloc(file_size);
  char *p = buf;

  while(flag) {
    if ((cur+1)*BLOCK_SIZE >= file_size) {
      flag = 0;
    }

    if (cur_block_indx > NDIRECT-1) {
      std::cout << "read into" << std::endl;
      inode_t *old_file_inode = file_inode;
      file_inode = get_inode(old_file_inode->blocks[NDIRECT]);
      free(old_file_inode);
      cur_block_indx = 0;
    }

    char block[BLOCK_SIZE];
    bm->read_block(file_inode->blocks[cur_block_indx], block);
    //std::cout << file_inode->blocks[cur_block_indx] << std::endl;
    //printf("block: %s\n", block);

    if (flag == 0) {
      int file_block_size = file_size-cur*BLOCK_SIZE;
      memcpy(p, block, file_block_size);
    }else{
      memcpy(p, block, BLOCK_SIZE);
      p += BLOCK_SIZE;
    }
    cur++;
    cur_block_indx++;
  }
  free(file_inode);
  file_inode = get_inode(inum);

  *buf_out = buf;
  //printf("buf_out: %s\n", buf);
  unsigned int now_time = (unsigned int)time(NULL);

  file_inode->atime = now_time;
  put_inode(inum, file_inode);
  free(file_inode);
  return;
}

/* alloc/free blocks if needed */
void
inode_manager::write_file(uint32_t inum, const char *buf, int size)
{
  /*
   * your code goes here.
   * note: write buf to blocks of inode inum.
   * you need to consider the situation when the size of buf 
   * is larger or smaller than the size of original inode
   */
  printf("\tim: write_file::inum: %d\n", inum);
  //std::cout << buf << std::endl;
  //std::cout << size << std::endl;

  int lost_size = size;
  const char *cur = buf;
  inode_t *file_inode = get_inode(inum);

  int file_size = file_inode->size;

  int block_num = 0;
  int real_block_num = 0;
  uint32_t last_inum = inum;

  while(lost_size > 0) {
    if (real_block_num > NDIRECT-1) {
      std::cout << "write into" << std::endl;
      inode_t *old_file_inode = file_inode;
      if (file_size<=(block_num*BLOCK_SIZE)) {
        uint32_t new_inum = alloc_inode(extent_protocol::T_FILE);
        old_file_inode->blocks[NDIRECT] = new_inum;
      }

      file_inode = get_inode(old_file_inode->blocks[NDIRECT]);
      put_inode(last_inum, old_file_inode);
      last_inum = old_file_inode->blocks[NDIRECT];
      free(old_file_inode);
      real_block_num = 0;
    }

    if (block_num*BLOCK_SIZE >= file_size)
      file_inode->blocks[real_block_num] = bm->alloc_block(DATA);
    
    if (lost_size < BLOCK_SIZE) {
      char tem_block[BLOCK_SIZE] = {0};
      memcpy(tem_block, cur, lost_size);
      //printf("cur: %s\n", cur);
      //std::cout << file_inode->blocks[real_block_num] << std::endl;
      bm->write_block(file_inode->blocks[real_block_num], tem_block);
    }else{
      //printf("cur: %s\n", cur);
      bm->write_block(file_inode->blocks[real_block_num], cur);
    }

    block_num++;
    real_block_num++;
    lost_size -= BLOCK_SIZE;
    cur += BLOCK_SIZE;
  }

  while(block_num*BLOCK_SIZE < file_size) {
    if (real_block_num > NDIRECT-1) {
      free_inode(file_inode->blocks[NDIRECT]);
      break;
    }

    bm->free_block(file_inode->blocks[real_block_num]);
    block_num++;
    real_block_num++;
  }
  put_inode(last_inum, file_inode);
  free(file_inode);
  file_inode = get_inode(inum);

  //std::cout << "change time: " << inum << std::endl;
  //std::cout << "old time: " << file_inode->mtime << std::endl;
  file_inode->size = size;
  file_inode->ctime = (unsigned int)time(NULL);
  file_inode->mtime = (unsigned int)time(NULL);
  
  put_inode(inum, file_inode);
  free(file_inode);
  return;
}

void
inode_manager::getattr(uint32_t inum, extent_protocol::attr &a)
{
  /*
   * your code goes here.
   * note: get the attributes of inode inum.
   * you can refer to "struct attr" in extent_protocol.h
   */
  char inode[BLOCK_SIZE];
  uint32_t block_num = inum - 1 + INODE_START;
  inode_t *tem_inode = (inode_t *)malloc(sizeof(inode_t));

  bm->read_block(block_num, inode);
  memcpy(tem_inode, inode, sizeof(inode_t));

  a.type = (uint32_t)tem_inode->type;
  a.size = tem_inode->size;
  a.atime = tem_inode->atime;
  a.mtime = tem_inode->mtime;
  a.ctime = tem_inode->ctime;
  free(tem_inode);

  return;
}

void
inode_manager::remove_file(uint32_t inum)
{
  /*
   * your code goes here
   * note: you need to consider about both the data block and inode of the file
   */
  free_inode(inum);
  return;
}

#include "inode_manager.h"
#include <cstring>
#include <ctime>

// disk layer -----------------------------------------

disk::disk()
{
  bzero(blocks, sizeof(blocks));
}

void
disk::read_block(blockid_t id, char *buf)
{
  /*
   *your lab1 code goes here.
   *if id is smaller than 0 or larger than BLOCK_NUM 
   *or buf is null, just return.
   *put the content of target block into buf.
   *hint: use memcpy
  */
	if(id < 0 || id > BLOCK_NUM || buf == NULL){
		printf("\tdisk: error! invalid blockid %d\n", id);
		return;
	}
	else
	  std::memcpy(buf, blocks[id], BLOCK_SIZE);
}

void
disk::write_block(blockid_t id, const char *buf)
{
  /*
   *your lab1 code goes here.
   *hint: just like read_block
  */
	if(id < 0 || id > BLOCK_NUM || buf == NULL){
		printf("\tdisk: error! invalid blockid %d\n", id);
		return;
	}
	else
	  std::memcpy(blocks[id], buf, BLOCK_SIZE);

}

// block layer -----------------------------------------

// Allocate a free disk block.
blockid_t
block_manager::alloc_block()
{
  /*
   * your lab1 code goes here.
   * note: you should mark the corresponding bit in block bitmap when alloc.
   * you need to think about which block you can start to be allocated.

   *hint: use macro IBLOCK and BBLOCK.
          use bit operation.
          remind yourself of the layout of disk.
   */
	char buf[BLOCK_SIZE];
	// search for a free block
	blockid_t first_data_block = IBLOCK(sb.ninodes, sb.nblocks) + 1;
	blockid_t now_block = first_data_block;
	while(now_block < sb.nblocks){
		read_block(BBLOCK(now_block), buf);
		for(int i = 0; i < BLOCK_SIZE && now_block < sb.nblocks; i++){
			unsigned char mask = 0x80;  // 1000,0000 , mask length is 8 bit, so use char
			// unsigned char will not get 0xff when >>

			// read 8 bit and search for a free block
			while(mask != 0 && now_block < sb.nblocks){
				// find out a free block
				if((mask & buf[i]) == 0){
					buf[i] = buf[i] | mask;  // update bitmap info
					write_block(BBLOCK(now_block), buf);
					printf("\tbm: alloc_block: %d\n", now_block);
					return now_block;
				}
				
				mask = mask >> 1;
				now_block ++;
			}
		}
	}

	printf("\tbm: error! out of blocks\n");
	exit(0);
}

void
block_manager::free_block(uint32_t id)
{
  /* 
   * your lab1 code goes here.
   * note: you should unmark the corresponding bit in the block bitmap when free.
   */
	char buf[BLOCK_SIZE];
	read_block(BBLOCK(id), buf);
	int index = (id % BPB) >> 3;  // mask is 8 bit, so use 8 bit as a group and index is the group index
	int index_in_group = (id % BPB) % 8;
	unsigned char mask = 0x80;
	mask = !(mask >> index_in_group);
	buf[index] = buf[index] | mask;

	write_block(BBLOCK(id), buf);
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
   * your lab1 code goes here.
   * note: the normal inode block should begin from the 2nd inode block.
   * the 1st is used for root_dir, see inode_manager::inode_manager().
    
   * if you get some heap memory, do not forget to free it.
   */
	// search through inode table to find a blank to insert a new indo
	char buf[BLOCK_SIZE]; 

	uint32_t now_inode = 1;  
	while(now_inode <= bm->sb.ninodes){
		bm->read_block(IBLOCK(now_inode, bm->sb.nblocks), buf);
		for(int i = 0; now_inode < bm->sb.ninodes && i < IPB; i++){
			inode_t *inode = (inode_t *)buf + i;
			if(inode->type == 0) {// this is a blank
				//printf("\tim: alloc_inode %d type: %d\n", now_inode, type);
				inode->type = type;
				inode->size = 0;
				inode->ctime = std::time(0);
				inode->atime = std::time(0);
				inode->mtime = std::time(0);
				bm->write_block(IBLOCK(now_inode, bm->sb.nblocks), buf);
				return now_inode;
			}
			now_inode++;
		}
	}

	printf("\tim: error! out of inodes\n");
	exit(0);

}

void
inode_manager::free_inode(uint32_t inum)
{
  /* 
   * your lab1 code goes here.
   * note: you need to check if the inode is already a freed one;
   * if not, clear it, and remember to write back to disk.
   * do not forget to free memory if necessary.
   */
	
	// check inode num range
	if (inum < 0 || inum >= INODE_NUM) {
		printf("\tim: inum out of range\n");
		return;
	}

	char a_block[BLOCK_SIZE];
	bm->read_block(IBLOCK(inum, bm->sb.nblocks), a_block);
	// modify inode table
	int index = inum % IPB;   //
	inode_t *inode = (inode_t *)a_block + index;
	if (inode->type == 0) {
		printf("\tim: inode not exist\n");
		return;
	}
	// update info
	inode->type = 0;
	bm->write_block(IBLOCK(inum, bm->sb.nblocks), a_block);

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

  bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
  // printf("%s:%d\n", __FILE__, __LINE__);

  ino_disk = (struct inode*)buf + inum%IPB;
  if (ino_disk->type == 0) {
    printf("\tim: inode not exist\n");
    return NULL;
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
   * your lab1 code goes here.
   * note: read blocks related to inode number inum,
   * and copy them to buf_out
   */
	inode_t *inode = get_inode(inum);
	char *buf = (char *) malloc(inode->size);
	char a_block[BLOCK_SIZE];
	char indirect_inode[BLOCK_SIZE];
	unsigned int now_position = 0;

	for (int i = 0; i < NDIRECT && now_position < inode->size ; i++)
	{
		if ((inode->size - now_position) >= BLOCK_SIZE){
			bm->read_block(inode->blocks[i], buf + now_position);
			now_position += BLOCK_SIZE;
		}else
		{
			int remain = inode->size - now_position;
			bm->read_block(inode->blocks[i], a_block);
			memcpy(buf + now_position, a_block, remain);
			now_position += remain;
		}
	}

	if(now_position < inode->size){
		// read indirect inode
		bm->read_block(inode->blocks[NDIRECT], indirect_inode);
		
		for (unsigned int i = 0; i < NINDIRECT && now_position < inode->size ; i++)
		{
			blockid_t indirect_id = *((blockid_t *)indirect_inode + i);
			if ((inode->size - now_position) >= BLOCK_SIZE){
				bm->read_block(indirect_id, buf + now_position);
				now_position += BLOCK_SIZE;
			}else
			{
				int remain = inode->size - now_position;
				bm->read_block(indirect_id, a_block);
				memcpy(buf + now_position, a_block, remain);
				now_position += remain;
			}
		}
		
	}
	

	// update inode information
	*buf_out = buf;
	*size = inode->size;
	inode->atime = std::time(0);
	put_inode(inum, inode);

	free(inode);
	inode = NULL;

}

/* alloc/free blocks if needed */
void
inode_manager::write_file(uint32_t inum, const char *buf, int size)
{
  /*
   * your lab1 code goes here.
   * note: write buf to blocks of inode inum.
   * you need to consider the situation when the size of buf 
   * is larger or smaller than the size of original inode.
   * you should free some blocks if necessary.
   */
	if(buf == NULL){ // check write content not null
		printf("\tim: error: write content is null\n");
		return;
	}
	//if(size > MAXFILE){  // check new file size
	//	printf("\tim: error: size of file is too big!\n");
	//	return;
	//}

	inode_t *inode = get_inode(inum);
	unsigned int old_block_cnt = (inode->size + BLOCK_SIZE - 1) / BLOCK_SIZE;
	unsigned int new_block_cnt = (size + BLOCK_SIZE -1) / BLOCK_SIZE;
	char indirect_inode[BLOCK_SIZE];

	// free some blocks
	if (old_block_cnt > new_block_cnt){
		if(new_block_cnt > NDIRECT){
			bm->read_block(inode->blocks[NDIRECT], indirect_inode);
			for(unsigned int i = new_block_cnt; i<old_block_cnt; i++){
				bm->free_block(*((blockid_t *)indirect_inode + i - NDIRECT));  // get id of block need to be free
			}
		}
		// new_block_cnt <= NDIRECT
		else {
			if(old_block_cnt > NDIRECT){
				// clear indirect inode block
				bm->read_block(inode->blocks[NDIRECT], indirect_inode);
				for(unsigned int i = NDIRECT; i<old_block_cnt; i++){
					bm->free_block(*((blockid_t *)indirect_inode + i - NDIRECT));  // get id of block need to be free
				}
				// clear indirect inode number block
				bm->free_block(inode->blocks[NDIRECT]);
				// clear direct inode block 
				for(unsigned int i = new_block_cnt; i < NDIRECT; i++){
					bm->free_block(inode->blocks[i]);
				}
			}
			else{ // old_block_cnt <= NDIRECT
				for(unsigned int i = new_block_cnt; i < old_block_cnt; i++){
					bm->free_block(inode->blocks[i]);
				}
			}
		}
	}

	// need to allocate more blocks
	if(old_block_cnt < new_block_cnt){
		if(new_block_cnt <= NDIRECT){
			for(unsigned int i = old_block_cnt; i < new_block_cnt; i++){
				inode->blocks[i] = bm->alloc_block();
			}
		}
		
		else{  // new_block_cnt > NDIRECT
			if(old_block_cnt <= NDIRECT){
				for(unsigned int i = old_block_cnt; i <= NDIRECT; i++){
					inode->blocks[i] = bm->alloc_block();
				}

				// fill indirect inode block
				bzero(indirect_inode, BLOCK_SIZE);  // first clear indirect inode block
				for(unsigned int i = NDIRECT; i< new_block_cnt; i++){
					*((blockid_t *)indirect_inode + (i - NDIRECT)) = bm->alloc_block();
				}
				bm->write_block(inode->blocks[NDIRECT], indirect_inode);
			}
			
			else{  // old_block_cnt > NDIRECT
				bm->read_block(inode->blocks[NDIRECT], indirect_inode);
				for(unsigned int i = old_block_cnt; i < new_block_cnt; i++){
					*((blockid_t *)indirect_inode + (i - NDIRECT)) = bm->alloc_block();
				}
				bm->write_block(inode->blocks[NDIRECT], indirect_inode);
			}

		}
	}

	// pre operation done, begin to write file
	int now_position = 0;
	char a_block[BLOCK_SIZE];
	// write DIRECT part first
	for (int i = 0; i < NDIRECT; i++)
	{
		if ((size - now_position) >= BLOCK_SIZE)
		{
			bm->write_block(inode->blocks[i], buf + now_position);
			now_position += BLOCK_SIZE;
		} else
		{
			int remain = size - now_position;
			memcpy(a_block, buf+now_position, remain);
			bm->write_block(inode->blocks[i], a_block);
			now_position += remain;
		}
	}
	// write INDIRECT part then
	if(now_position < size){
		bm->read_block(inode->blocks[NDIRECT],indirect_inode);
		for (unsigned int i = 0; i < NDIRECT && now_position < size; i++)
		{
			blockid_t indirect_id = (*(blockid_t *)indirect_inode + i);
			if (size-now_position >= BLOCK_SIZE)
			{
				bm->write_block(indirect_id, buf + now_position);
				now_position += BLOCK_SIZE;
			} else
			{
				int remain = size - now_position;
				memcpy(a_block, buf+now_position, remain);
				bm->write_block(indirect_id, a_block);
				now_position += remain;
				
			}
		}
		
	}
	


	// write done, then update inode
	inode->size = size;
	inode->mtime = std::time(0);
	put_inode(inum, inode);

	free(inode);
	inode = NULL;
}

void
inode_manager::getattr(uint32_t inum, extent_protocol::attr &a)
{
  /*
   * your lab1 code goes here.
   * note: get the attributes of inode inum.
   * you can refer to "struct attr" in extent_protocol.h
   */
	struct inode *inode = get_inode(inum);
	if(inode){
		a.type = inode->type;
		a.atime = inode->atime;
		a.ctime = inode->ctime;
		a.mtime = inode->mtime;
		a.size = inode->size;
		
		// free the memory allocated in get_inode function
		free(inode);
		inode = NULL;
	}
}

void
inode_manager::remove_file(uint32_t inum)
{
  /*
   * your lab1 code goes here
   * note: you need to consider about both the data block and inode of the file
   * do not forget to free memory if necessary.
   */
	struct inode *inode = get_inode(inum);
	int block_num = (inode->size + BLOCK_SIZE - 1) / BLOCK_SIZE;
	if(block_num <= NDIRECT){
		for(int i = 0; i < block_num; i++){
			bm->free_block(inode->blocks[i]);
		}
	}
	else{  // block num > NDIRECT
		// remove direct block first
		for(int i = 0; i < NDIRECT; i++){
			bm->free_block(inode->blocks[i]);
		}
		char a_block[BLOCK_SIZE];
		bm->read_block(inode->blocks[NDIRECT], a_block);  // indirect block
		for(int i = NDIRECT; i < block_num; i++){
			blockid_t block_id = *((blockid_t *)a_block + i);
			bm->free_block(block_id);
		}
		// remove indirect block
		bm->free_block(inode->blocks[NDIRECT]);
	}

	free(inode);
	inode = NULL;
	free_inode(inum);
}

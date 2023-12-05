#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "mdadm.h"
#include "jbod.h"

int cache_create(int num_entries);
int cache_destroy(void);
int cache_lookup(int disk_num, int block_num, uint8_t *buf);
int cache_insert(int disk_num, int block_num, const uint8_t *buf);
void cache_update(int disk_num, int block_num, const uint8_t *buf);
bool cache_enabled(void);
int jbod_client_operation(uint32_t op, uint8_t *block);

/*
We use the cache whenever we read from or wrrite to the jbod.
when we write, we update the valuse using the cache_update function.
when we read, we use the cache_lookup function
*/

int mount = 0;

// minimum function
uint32_t min(uint32_t a, uint32_t b) {
  if (a > b) {
    return b;
  }

  return a;
}

// encode_op takes in address and command and returns the op in corresct format
uint32_t encode_op(uint32_t address, uint32_t command){

  uint32_t disk_id = address/JBOD_DISK_SIZE;
  uint32_t block_id = (address/JBOD_BLOCK_SIZE)%JBOD_DISK_SIZE;
  uint32_t op = disk_id << 22 | command << 26 | block_id;

  return op;
}

// function to mount: 1 on sucess and -1 otherwise
int mdadm_mount (void){

  if (mount == 1) {
    return -1;
  }

  uint32_t op = encode_op(0, JBOD_MOUNT);
  jbod_client_operation(op, NULL);
  mount = 1;

  return 1;
}

// function to unmount: 1 on success and -1 on unmount
int mdadm_unmount (void){

  if (mount == 0) {
    return -1;
  }

  uint32_t op = encode_op(0, JBOD_UNMOUNT);
  jbod_client_operation(op, NULL);
  mount = 0;

  return 1;
}

/*
we will now be intergrating cache.
first, we try to read the block from the cache if it exists
if the cache_lookup fucntion doesn't succeed, we do cache_insert
*/

int mdadm_read (uint32_t addr, uint32_t len, uint8_t *buf) {

  //testing for invalid parameters

  // read should fail on an unmounted system
  if (mount == 0){
    return -1;
  }

  // read should fail if len is grater than 1024
  if (len > 1024){
    return -1;
  }

  // read should fail if address is out of bounds
  if (addr >= (16*256*256)) {
    return -1;
  }

  // read should fail if address+len is out of bounds
  if ((addr+len) >= (16*256*256)) {
    return -1;
  }

  // read should fail if buf is null and length is not zero
  if ((buf == NULL) & (len != 0)) {
    return -1;
  }

  // declaring vrialbles required to complete read function

  uint8_t mybuf[256];
  uint32_t bytes_to_read;
  uint32_t op;
  uint32_t bytes_read = 0;
  uint32_t offset = addr%(256);

  // tested for parameters
  // beyond this point, all the parameters are acceptable
  while (bytes_read != len) {
    
    // seeking to the correct disk
    op = encode_op(addr, JBOD_SEEK_TO_DISK);
    jbod_client_operation(op, NULL);

    // seeking to correct block
    op = encode_op(addr, JBOD_SEEK_TO_BLOCK);
    jbod_client_operation(op, NULL);

    // reading from the address to mybuf
    bytes_to_read = min((256 - offset), (len-bytes_read));

    // this is where the cache implementation starts
    if (bytes_to_read == 256 && cache_enabled() == true){
      if (cache_lookup((addr/JBOD_DISK_SIZE), (addr/JBOD_BLOCK_SIZE)%JBOD_DISK_SIZE, buf+bytes_read) == 1) {
        bytes_read += bytes_to_read;
        addr = (addr + bytes_to_read);
        offset = 0;
        continue;
      }
      else {
        op = encode_op(addr, JBOD_READ_BLOCK);
        jbod_operation(op, mybuf);
        cache_insert((addr/JBOD_DISK_SIZE), (addr/JBOD_BLOCK_SIZE)%JBOD_DISK_SIZE, mybuf);
      }
    }

    else{
      op = encode_op(addr, JBOD_READ_BLOCK);
      jbod_client_operation(op, mybuf);
    }

    // copying required memory from mybuf to buf
    memcpy(buf + bytes_read, (mybuf + offset), bytes_to_read);

    // performing manipulations on the variables
    offset = 0;
    bytes_read += bytes_to_read;
    addr = (addr + bytes_to_read);
  }

  return len;
}

int mdadm_write(uint32_t addr, uint32_t len, const uint8_t *buf) {
  
  //we are now going to implement cache in write function
  //we dont have to lookup or insert
  //we will call update cache for every block that we write
  //if it succeeds, awesome
  //if it doesn't, no harm done. do graders actually read the comments?
  
  // if not mounted, write should fail
  if (mount == 0) {
    return -1;
  }

  // should fail for all invalid parameters
  // can basically copy-paste the conditions from the read function above

  // can create a seperate helper function for checking the validity of 
  // parameters but not optimal since we cannot gurrantee that we are 
  // going to have the same parameters for other possible functions in the future

  // so copy and paste it is!!!

  // write should fail if len is greater than 1024
  if (len > 1024){
    return -1;
  }

  // write should fail if address is out of bounds
  if (addr >= (16*256*256)) {
    return -1;
  }

  // write should fail if address+len is out of bounds
  if ((addr+len) > (16*256*256)) {
    return -1;
  }

  // write should fail if buf is null and length is not zero
  if ((buf == NULL) & (len != 0)) {
    return -1;
  }

  // declaring all the necessary variables

  uint8_t mybuf[256]; // we read everything from the block we need to write to into this varibale
  uint32_t bytes_to_write; // keep track of the number of bytes to read
  uint32_t op; // use this to pass to op parameter to jbod function
  uint32_t bytes_written = 0; // keep track of number of bytes written
  uint32_t offset = addr%(256);

  //tested for parameters
  // beyond this point, all the parameters are acceptable

  while (bytes_written != len) {

    // seek to the correct disk
    op = encode_op(addr, JBOD_SEEK_TO_DISK);
    jbod_client_operation(op, NULL);

    // seek to the correct block
    op = encode_op(addr, JBOD_SEEK_TO_BLOCK);
    jbod_client_operation(op, NULL);

    // reading from address to mybuf
    bytes_to_write = min((256 - offset), (len-bytes_written));
    op = encode_op(addr, JBOD_READ_BLOCK);
    jbod_client_operation(op, mybuf);

    // we have to seek to the block again because read operation increments block by 1
    // seek to the correct block
    op = encode_op(addr, JBOD_SEEK_TO_BLOCK);
    jbod_client_operation(op, NULL);

    // now the data in the block we need to write to is in mybuf

    uint8_t writebuf[256]; // we make the necessary changes and write this block to the memory
    uint32_t local_bytes_read = 0; // keeps track of the number of bytes read from
    // mybuf to writebuf in case of offset not being 0 and address + len falling short
    // of the length of the block

    // if the offset is greater than 0, wee need to copy the contents of the first
    // offset bytes to writebuf
    if (offset > 0) {
      memcpy(writebuf, mybuf, offset);
      local_bytes_read += offset;
    }

    // copy len bytes_to_write number of bytes from buf to writebuf
    memcpy(writebuf+local_bytes_read, buf+bytes_written, bytes_to_write);
    local_bytes_read += bytes_to_write;

    // now, we need ot write fill the remaining space in write_buf with the 
    // contents from corresponding indices of mybuf: if local_bytes_read < 256
    if (local_bytes_read < 256) {
      memcpy(writebuf+local_bytes_read, mybuf+local_bytes_read, 256-local_bytes_read);
    }

    // hopefully, our writebuf is ready now

    // The big one - calling the WRITE operation
    op = encode_op(addr, JBOD_WRITE_BLOCK);
    jbod_client_operation(op, writebuf);

    // and updating the cache of course
    
    if (cache_enabled() == true){
      cache_update((addr/JBOD_DISK_SIZE), (addr/JBOD_BLOCK_SIZE)%JBOD_DISK_SIZE, writebuf);
    }
    // updating the varibales
    offset = 0;
    bytes_written += bytes_to_write;
    addr = (addr + bytes_to_write);
    
  }

  return len;
}
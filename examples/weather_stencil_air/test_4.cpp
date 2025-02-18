// (c) 2023 SAFARI Research Group at ETH Zurich, Gagandeep Singh, D-ITET
// SPDX-License-Identifier: MIT

#include "air.hpp"
#include "test_library.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <stdlib.h>
#include <sys/mman.h>
#include <thread>
#include <time.h>
#include <unistd.h>
#include <vector>
#include <xaiengine.h>

#define HIGH_ADDR(addr) ((addr & 0xffffffff00000000) >> 32)
#define LOW_ADDR(addr) (addr & 0x00000000ffffffff)
#define MLIR_STACK_OFFSET 4096

#define TOTAL_B_BLOCK 1 // only 1
#define B_BLOCK_DEPTH 4 // set how many rows
#define HDIFF_COL 3     // columns
#define START_ROW 1
#define INPUT_ROWS 9
#define DMA_COUNT_IN 256 * INPUT_ROWS
#define DMA_COUNT_OUT 256 * 2 * B_BLOCK_DEPTH
#define XAIE_NUM_COLS 10

#include "aie_inc.cpp"

int main(int argc, char *argv[]) {

  // Starting in the first DU of the VCK5000
  uint64_t row = 0;
  uint64_t shim_one_col = 2;
  uint64_t shim_two_col = 3;

  hsa_status_t init_status = air_init();

  if (init_status != HSA_STATUS_SUCCESS) {
    std::cout << "air_init() failed. Exiting" << std::endl;
    return -1;
  }

  std::vector<air_agent_t> agents;
  auto get_agents_ret = air_get_agents(agents);
  assert(get_agents_ret == HSA_STATUS_SUCCESS && "failed to get agents!");

  if (agents.empty()) {
    std::cout << "No agents found. Exiting." << std::endl;
    return -1;
  }

  std::cout << "Found " << agents.size() << " agents" << std::endl;

  std::vector<queue_t *> queues;
  for (auto agent : agents) {
    // create the queue
    queue_t *q = nullptr;
    auto create_queue_ret =
        air_queue_create(MB_QUEUE_SIZE, HSA_QUEUE_TYPE_SINGLE, &q, agent.handle,
                         0 /* device_id (optional) */);
    assert(create_queue_ret == 0 && "failed to create queue!");
    queues.push_back(q);
  }

  aie_libxaie_ctx_t *xaie = (aie_libxaie_ctx_t *)air_get_libxaie_ctx();
  if (xaie == NULL) {
    std::cout << "Error getting libxaie context" << std::endl;
    return -1;
  }

  // Initializing the device memory allocator
  if (air_init_dev_mem_allocator(0x20000 /* dev_mem_size */,
                                 0 /* device_id (optional)*/)) {
    std::cout << "Error creating device memory allocator" << std::endl;
    return -1;
  }

  // Initializing the device
  uint64_t wr_idx = queue_add_write_index(queues[0], 1);
  uint64_t packet_id = wr_idx % queues[0]->size;
  dispatch_packet_t *shim_pkt =
      (dispatch_packet_t *)(queues[0]->base_address_vaddr) + packet_id;
  air_packet_device_init(shim_pkt, XAIE_NUM_COLS);
  air_queue_dispatch_and_wait(queues[0], wr_idx, shim_pkt);

  //
  // Set up a 8x8 segment
  //
  wr_idx = queue_add_write_index(queues[0], 1);
  packet_id = wr_idx % queues[0]->size;
  dispatch_packet_t *segment_pkt =
      (dispatch_packet_t *)(queues[0]->base_address_vaddr) + packet_id;
  air_packet_segment_init(segment_pkt, 0, shim_one_col, 8, row, 8);

  mlir_aie_configure_cores(xaie);
  mlir_aie_configure_switchboxes(xaie);
  mlir_aie_initialize_locks(xaie);
  mlir_aie_configure_dmas(xaie);
  mlir_aie_start_cores(xaie);
  int errors = 0;

  uint32_t *ddr_ptr_in_0 =
      (uint32_t *)air_dev_mem_alloc(DMA_COUNT_IN * sizeof(uint32_t));
  if (ddr_ptr_in_0 == NULL) {
    printf("Failed to allocate ddr_ptr_in_0\n");
    return 1;
  }

  uint32_t *ddr_ptr_in_1 =
      (uint32_t *)air_dev_mem_alloc(DMA_COUNT_IN * sizeof(uint32_t));
  if (ddr_ptr_in_1 == NULL) {
    printf("Failed to allocate ddr_ptr_in_1\n");
    return 1;
  }

  uint32_t *ddr_ptr_in_2 =
      (uint32_t *)air_dev_mem_alloc(DMA_COUNT_IN * sizeof(uint32_t));
  if (ddr_ptr_in_2 == NULL) {
    printf("Failed to allocate ddr_ptr_in_2\n");
    return 1;
  }

  uint32_t *ddr_ptr_in_3 =
      (uint32_t *)air_dev_mem_alloc(DMA_COUNT_IN * sizeof(uint32_t));
  if (ddr_ptr_in_3 == NULL) {
    printf("Failed to allocate ddr_ptr_in_3\n");
    return 1;
  }

  uint32_t *ddr_ptr_out_0 =
      (uint32_t *)air_dev_mem_alloc(DMA_COUNT_OUT * sizeof(uint32_t));
  if (ddr_ptr_out_0 == NULL) {
    printf("Failed to allocate ddr_ptr_out_0\n");
    return 1;
  }

  uint32_t *ddr_ptr_out_1 =
      (uint32_t *)air_dev_mem_alloc(DMA_COUNT_OUT * sizeof(uint32_t));
  if (ddr_ptr_out_1 == NULL) {
    printf("Failed to allocate ddr_ptr_out_1\n");
    return 1;
  }

  uint32_t *ddr_ptr_out_2 =
      (uint32_t *)air_dev_mem_alloc(DMA_COUNT_OUT * sizeof(uint32_t));
  if (ddr_ptr_out_2 == NULL) {
    printf("Failed to allocate ddr_ptr_out_2\n");
    return 1;
  }

  uint32_t *ddr_ptr_out_3 =
      (uint32_t *)air_dev_mem_alloc(DMA_COUNT_OUT * sizeof(uint32_t));
  if (ddr_ptr_out_3 == NULL) {
    printf("Failed to allocate ddr_ptr_out_3\n");
    return 1;
  }

  // initialize the external buffers
  for (int i = 0; i < DMA_COUNT_IN; i++) {
    *(ddr_ptr_in_0 + i) = i; // input
    *(ddr_ptr_in_1 + i) = i; // input
    *(ddr_ptr_in_2 + i) = i; // input
    *(ddr_ptr_in_3 + i) = i; // input
  }

  for (int i = 0; i < DMA_COUNT_OUT; i++) {
    *(ddr_ptr_out_0 + i) = 0; // input
    *(ddr_ptr_out_1 + i) = 0; // input
    *(ddr_ptr_out_2 + i) = 0; // input
    *(ddr_ptr_out_3 + i) = 0; // input
  }

  printf("Finish configure\n");

  //////////////////////////////////////// B Block 0
  //
  // send the data
  //

  wr_idx = queue_add_write_index(queues[0], 1);
  packet_id = wr_idx % queues[0]->size;
  dispatch_packet_t *pkt =
      (dispatch_packet_t *)(queues[0]->base_address_vaddr) + packet_id;
  air_packet_nd_memcpy(pkt, 0, shim_one_col, 1, 0, 4, 2,
                       air_dev_mem_get_pa(ddr_ptr_in_0),
                       DMA_COUNT_IN * sizeof(float), 1, 0, 1, 0, 1, 0);
  // air_queue_dispatch_and_wait(queues[0], wr_idx, pkt);

  //
  // read the data
  //

  wr_idx = queue_add_write_index(queues[0], 1);
  packet_id = wr_idx % queues[0]->size;
  dispatch_packet_t *pkt2 =
      (dispatch_packet_t *)(queues[0]->base_address_vaddr) + packet_id;
  air_packet_nd_memcpy(pkt2, 0, shim_one_col, 0, 0, 4, 2,
                       air_dev_mem_get_pa(ddr_ptr_out_0),
                       DMA_COUNT_OUT * sizeof(float), 1, 0, 1, 0, 1, 0);
  // air_queue_dispatch_and_wait(queues[0], wr_idx, pkt2);

  /*
    for (int i = 0; i < 512; i++) {
      printf("Location %d:  %d\n", i, ddr_ptr_out_0[i]);
    }
  */

  //////////////////////////////////////// B Block 1
  //
  // send the data
  //

  wr_idx = queue_add_write_index(queues[0], 1);
  packet_id = wr_idx % queues[0]->size;
  dispatch_packet_t *pkt3 =
      (dispatch_packet_t *)(queues[0]->base_address_vaddr) + packet_id;
  air_packet_nd_memcpy(pkt3, 0, shim_one_col, 1, 1, 4, 2,
                       air_dev_mem_get_pa(ddr_ptr_in_1),
                       DMA_COUNT_IN * sizeof(float), 1, 0, 1, 0, 1, 0);
  // air_queue_dispatch_and_wait(queues[0], wr_idx, pkt3);

  //
  // read the data
  //

  wr_idx = queue_add_write_index(queues[0], 1);
  packet_id = wr_idx % queues[0]->size;
  dispatch_packet_t *pkt4 =
      (dispatch_packet_t *)(queues[0]->base_address_vaddr) + packet_id;
  air_packet_nd_memcpy(pkt4, 0, shim_one_col, 0, 1, 4, 2,
                       air_dev_mem_get_pa(ddr_ptr_out_1),
                       DMA_COUNT_OUT * sizeof(float), 1, 0, 1, 0, 1, 0);
  // air_queue_dispatch_and_wait(queues[0], wr_idx, pkt4);

  /*
    for (int i = 0; i < 512; i++) {
      printf("Location %d:  %d\n", i, ddr_ptr_out_0[i]);
    }
  */

  //////////////////////////////////////// B Block 2
  //
  // send the data
  //

  wr_idx = queue_add_write_index(queues[0], 1);
  packet_id = wr_idx % queues[0]->size;
  dispatch_packet_t *pkt5 =
      (dispatch_packet_t *)(queues[0]->base_address_vaddr) + packet_id;
  air_packet_nd_memcpy(pkt5, 0, shim_two_col, 1, 0, 4, 2,
                       air_dev_mem_get_pa(ddr_ptr_in_2),
                       DMA_COUNT_IN * sizeof(float), 1, 0, 1, 0, 1, 0);
  // air_queue_dispatch_and_wait(queues[0], wr_idx, pkt5);

  //
  // read the data
  //

  wr_idx = queue_add_write_index(queues[0], 1);
  packet_id = wr_idx % queues[0]->size;
  dispatch_packet_t *pkt6 =
      (dispatch_packet_t *)(queues[0]->base_address_vaddr) + packet_id;
  air_packet_nd_memcpy(pkt6, 0, shim_two_col, 0, 0, 4, 2,
                       air_dev_mem_get_pa(ddr_ptr_out_2),
                       DMA_COUNT_OUT * sizeof(float), 1, 0, 1, 0, 1, 0);
  // air_queue_dispatch_and_wait(queues[0], wr_idx, pkt6);

  //////////////////////////////////////// B Block 3
  //
  // send the data
  //

  wr_idx = queue_add_write_index(queues[0], 1);
  packet_id = wr_idx % queues[0]->size;
  dispatch_packet_t *pkt7 =
      (dispatch_packet_t *)(queues[0]->base_address_vaddr) + packet_id;
  air_packet_nd_memcpy(pkt7, 0, shim_two_col, 1, 1, 4, 2,
                       air_dev_mem_get_pa(ddr_ptr_in_3),
                       DMA_COUNT_IN * sizeof(float), 1, 0, 1, 0, 1, 0);
  // air_queue_dispatch_and_wait(queues[0], wr_idx, pkt7);

  //
  // read the data
  //

  wr_idx = queue_add_write_index(queues[0], 1);
  packet_id = wr_idx % queues[0]->size;
  dispatch_packet_t *pkt8 =
      (dispatch_packet_t *)(queues[0]->base_address_vaddr) + packet_id;
  air_packet_nd_memcpy(pkt8, 0, shim_two_col, 0, 1, 4, 2,
                       air_dev_mem_get_pa(ddr_ptr_out_3),
                       DMA_COUNT_OUT * sizeof(float), 1, 0, 1, 0, 1, 0);
  air_queue_dispatch_and_wait(queues[0], wr_idx, pkt8);

  for (int i = 0; i < 512; i++) {
    if (ddr_ptr_out_0[i] != ddr_ptr_out_1[i]) {
      printf("[ERROR] ddr_ptr_out_0[%d] (%d) != ddr_ptr_out_1[%d (%d)]\n", i,
             ddr_ptr_out_0[i], i, ddr_ptr_out_1[i]);
      errors++;
    }

    if (ddr_ptr_out_0[i] != ddr_ptr_out_2[i]) {
      printf("[ERROR] ddr_ptr_out_0[%d] (%d) != ddr_ptr_out_2[%d (%d)]\n", i,
             ddr_ptr_out_0[i], i, ddr_ptr_out_2[i]);
      errors++;
    }

    if (ddr_ptr_out_0[i] != ddr_ptr_out_3[i]) {
      printf("[ERROR] ddr_ptr_out_0[%d] (%d) != ddr_ptr_out_3[%d (%d)]\n", i,
             ddr_ptr_out_0[i], i, ddr_ptr_out_3[i]);
      errors++;
    }

    printf("Location %d:  %d\n", i, ddr_ptr_out_0[i]);
    printf("Location %d:  %d\n", i, ddr_ptr_out_1[i]);
    printf("Location %d:  %d\n", i, ddr_ptr_out_2[i]);
    printf("Location %d:  %d\n", i, ddr_ptr_out_3[i]);
  }

  int res = 0;
  if (!errors) {
    printf("PASS!\n");
    res = 0;
  } else {
    printf("Fail!\n");
    res = -1;
  }

  printf("test done weather predicted = chocolate :D\n");

  return 0;
}

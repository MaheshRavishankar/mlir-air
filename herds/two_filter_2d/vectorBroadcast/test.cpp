//===- test.cpp -------------------------------------------------*- C++ -*-===//
//
// This file is licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// (c) Copyright 2021 Xilinx Inc.
//
//===----------------------------------------------------------------------===//

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <thread>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <xaiengine.h>
#include "test_library.h"

#define XAIE_NUM_ROWS            8
#define XAIE_NUM_COLS           50
#define XAIE_ADDR_ARRAY_OFF     0x800

#define HIGH_ADDR(addr)	((addr & 0xffffffff00000000) >> 32)
#define LOW_ADDR(addr)	(addr & 0x00000000ffffffff)

#define MLIR_STACK_OFFSET 4096

#define LOCK_TIMEOUT 1000

//define some constants, CAREFUL NEED TO MATCH MLIR
#define LINE_WIDTH 16
#define HEIGHT 10

namespace {

XAieGbl_Config *AieConfigPtr;	                          /**< AIE configuration pointer */
XAieGbl AieInst;	                                      /**< AIE global instance */
XAieGbl_HwCfg AieConfig;                                /**< AIE HW configuration instance */
XAieGbl_Tile TileInst[XAIE_NUM_COLS][XAIE_NUM_ROWS+1];  /**< Instantiates AIE array of [XAIE_NUM_COLS] x [XAIE_NUM_ROWS] */
XAieDma_Tile TileDMAInst[XAIE_NUM_COLS][XAIE_NUM_ROWS+1];

#include "aie_inc.cpp"

}

int main(int argc, char *argv[])
{
    printf("test start.\n");

    size_t aie_base = XAIE_ADDR_ARRAY_OFF << 14;
    XAIEGBL_HWCFG_SET_CONFIG((&AieConfig), XAIE_NUM_ROWS, XAIE_NUM_COLS, XAIE_ADDR_ARRAY_OFF);
    XAieGbl_HwInit(&AieConfig);
    AieConfigPtr = XAieGbl_LookupConfig(XPAR_AIE_DEVICE_ID);
    XAieGbl_CfgInitialize(&AieInst, &TileInst[0][0], AieConfigPtr);

    ACDC_clear_tile_memory(TileInst[1][2]);
    ACDC_clear_tile_memory(TileInst[1][3]);

    mlir_configure_cores();
    mlir_configure_switchboxes();
    mlir_configure_dmas();
    mlir_initialize_locks();

    ACDC_print_tile_status(TileInst[1][2]);
    ACDC_print_tile_status(TileInst[1][3]);

    printf("\nStarting cores.\n");

    mlir_start_cores();

    int errors = 0;


    printf("Waiting to acquire output lock for read ...\n");
    if(!XAieTile_LockAcquire(&(TileInst[1][2]), 0, 1, LOCK_TIMEOUT)) {
        printf("ERROR: timeout hit!\n");
    }


    printf("vector broadcast line generation\n");
    for(int j=0; j < LINE_WIDTH; j++)
        printf("%d ",mlir_read_buffer_outVector(j));
    printf("\n\n");
    
    printf("scalar line generation\n");
    for(int j=0; j < LINE_WIDTH; j++)
        printf("%d ",mlir_read_buffer_outScalar(j));
    printf("\n\n");
    
    for(int j=0; j < LINE_WIDTH; j++)
        ACDC_check("AFTER", mlir_read_buffer_outVector(j), 5, errors);        

    if (!errors) {
        printf("PASS!\n"); return 0;
    } else {
        printf("Fail!\n"); return -1;
    }
    printf("test done.\n");
}

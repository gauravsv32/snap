#ifndef __ACTION_HARDWARE_H__
#define __ACTION_HARDWARE_H__

/*
 * Copyright 2017 International Business Machines
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdint.h>
#include <string.h>
#include <ap_int.h>
#include <ap_cint.h>
#include <ap_fixed.h>
#include "hls_snap.H"
#include <action_commonheader.h> /* PCA Job definition */
#include "hls_linear_algebra.h"
   
//--------------------------------------------------------------------
// 22: simplify the data casting style
#define RELEASE_LEVEL		0x00000022

#define MAX_NB_OF_BYTES_READ	(256 * 1024)
#define MAX_NB_OF_WORDS_READ	(MAX_NB_OF_BYTES_READ/BPERDW)
#define CARD_DRAM_SIZE		(8 * 1024 *1024 * 1024L)

const unsigned A_ROWS = MAX_LOCAL_MATRIX_ROWS;
const unsigned A_COLS = MAX_LOCAL_MATRIX_COLS;
typedef ap_uint<8> matrix_in_element_hw_t;
typedef float matrix_element_hw_t;
typedef char word_t[BPERDW];

struct MY_CONFIG : hls::svd_traits<A_ROWS,A_COLS,matrix_element_hw_t,matrix_element_hw_t>{ 
    static const int NUM_SWEEPS = 6;
    static const int DIAG_II = 4;
    static const int OFF_DIAG_II = 4;
};
//---------------------------------------------------------------------
// This is generic. Just adapt names for a new action
// CONTROL is defined and handled by SNAP 
// matrixmultiply_job_t is user defined in hls_matrixmultiply/include/action_commonheader.h
typedef struct {
	CONTROL Control;	/*  16 bytes */
	pca_job_t Data;	/* up to 108 bytes */
	uint8_t padding[SNAP_HLS_JOBSIZE - sizeof(pca_job_t)];
} action_reg;

#endif  /* __ACTION_HARDWARE_H__*/

#ifndef __ACTION_COMMONHEADER_H__
#define __ACTION_COMMONHEADER_H__

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

#include <snap_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* This number is unique and is declared in ~snap/ActionTypes.md */
#define PCA_ACTION_TYPE 0x1014100B

// macro definitions for cov_mod
#define MAX_ELEMENTS_PER_ROW_IN_MEMBUS 64
#define MAX_ELEMENTS_PER_COVROW_IN_MEMBUS 16	
#define MAX_LOCAL_MATRIX_ROWS 1024
#define MAX_LOCAL_MATRIX_COLS 65536
#define MAX_INROW_SIZE_IN_MEMBUS_WORDS MAX_LOCAL_MATRIX_COLS/MAX_ELEMENTS_PER_ROW_IN_MEMBUS
#define MAX_COVROW_SIZE_IN_MEMBUS_WORDS MAX_LOCAL_MATRIX_ROWS/MAX_ELEMENTS_PER_COVROW_IN_MEMBUS
#define MAX_MAT_ELEMENT_COUNT (MAX_LOCAL_MATRIX_ROWS*MAX_LOCAL_MATRIX_COLS)
#define SORT_SWEEP_DEPTH 10
typedef uint8_t inp_matrix_element_t;
//



//#define MAX_LOCAL_MATRIX_SIZE_IN_MEMBUS_WORDS 16
//#define MAX_MEMBUS_WORDS_PER_ROW 2
//#define MAX_INMAT_ROWS 32
//#define MAX_LOCAL_MATRIX_ROWS 16
//#define MAX_LOCAL_MATRIX_COLS 16
//#define MAX_MAT_ELEMENT_COUNT (MAX_LOCAL_MATRIX_ROWS*MAX_LOCAL_MATRIX_COLS)
typedef float matrix_element_t;
typedef uint32_t union_unsigned_element_t;

/* Data structure used to exchange information between action and application */
/* Size limit is 108 Bytes */
typedef struct pca_job {
	struct snap_addr inmat;	    /* input data */   ///snap_addr:: 128bit width= 16bytes
	//struct snap_addr cov_inmat;	    /* input data */
	struct snap_addr outmatS;      /* output data */
	struct snap_addr outmatU;
	struct snap_addr outmatV;
	struct snap_addr outSortedIndexMatS;
	uint32_t inmat_num_rows;
	uint32_t inmat_num_cols;
	//uint64_t dummy_to_round_to_multiple_of_16_bytes;
}pca_job_t;

#ifdef __cplusplus
}
#endif

#endif	/* __ACTION_COMMONHEADER_H__ */

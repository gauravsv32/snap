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

/*
 * SNAP HLS_MATRIXMULTIPLY EXAMPLE
 *
 * Tasks for the user:
 *   1. Explore HLS pragmas to get better timing behavior.
 *   2. Try to measure the time needed to do data transfers (advanced)
 */
 
#include "action_hardware.H"
#include "hls_svd_mod.h"

#define NUM_ROWS 1024
#define NUM_COLS 1024
#define NUM_MAT_ELEMENTS_PER_MEMBUS_ROW 16
#define NUM_MEMBUS_READS_PER_MAT_ROW (NUM_ROWS/NUM_MAT_ELEMENTS_PER_MEMBUS_ROW)
#define NUM_OFFDIAG_ROWS (NUM_ROWS-2)
#define NUM_OFFDIAG_COLS (NUM_COLS-2)
#define SIZE_OF_ONE_MAT_ROW (NUM_ROWS*(sizeof(matrix_element_hw_t)))

// WRITE DATA FROM OUTPUT BUFFER TO MEMORY
short write_burst_of_data_to_mem(snap_membus_t *dout_gmem,
				 snapu64_t output_address,
				 snap_membus_t *buffer,
				 snapu64_t size_in_bytes_to_transfer)
{
  short rc=0;

  int size_in_words;
  size_in_words = size_in_bytes_to_transfer/BPERDW;

  // Do not insert anything more in this loop to not break the burst
 wb_dout_loop: for (int k=0; k<size_in_words; k++)
    //#pragma HLS PIPELINE
    (dout_gmem + output_address)[k] = buffer[k];
		
  return rc;
}//end write_burst_of_data_to_mem

// WRITE DATA FROM INPUT BUFFER TO FPGA DDR
short write_burst_of_data_to_fpga(snap_membus_t *d_ddrmem,
				  snapu64_t output_address,
				  snap_membus_t *buffer,
				  snapu64_t size_in_bytes_to_transfer)
{
  short rc=0;

  int size_in_words;
  size_in_words = size_in_bytes_to_transfer/BPERDW;

  // Do not insert anything more in this loop to not break the burst
 wb_ddr_loop: for (int k=0; k<size_in_words; k++)
    //#pragma HLS PIPELINE
    (d_ddrmem + output_address)[k] = buffer[k];	
  // end of patch
		

  return rc;
}//end write_burst_of_data_to_fpga

// READ DATA FROM MEMORY to Input buffer
short read_burst_of_data_from_mem(snap_membus_t *din_gmem,
				  snapu64_t input_address,
				  snap_membus_t *buffer,
				  snapu64_t size_in_bytes_to_transfer)
{
	short rc=0;
        int i;

	int size_in_words;
	size_in_words = size_in_bytes_to_transfer/BPERDW;


	// Do not insert anything more in this loop to not break the burst
 rb_din_loop: for (int k=0; k<size_in_words; k++)
 //#pragma HLS PIPELINE
	  buffer[k] = (din_gmem + input_address)[k];
		
	return rc;
}//read_burst_of_data_from_mem


// READ DATA FROM FPGA DDR to Output Buffer
short read_burst_of_data_from_fpga(snap_membus_t *d_ddrmem,
				  snapu64_t input_address,
				  snap_membus_t *buffer,
				  snapu64_t size_in_bytes_to_transfer)
{
	short rc=0;
        int i;

	int size_in_words;
	size_in_words = size_in_bytes_to_transfer/BPERDW;

	// Do not insert anything more in this loop to not break the burst
 rb_ddr_loop: for (int k=0; k<size_in_words; k++)
	  //#pragma HLS PIPELINE
	  buffer[k] = (d_ddrmem + input_address)[k];	
		
		
	return rc;
}//read_burst_of_data_from_fpga

//COPY FROM HOST MEMORY TO FPGA DDR
short copy_one_input( snap_membus_t *din_gmem
		    ,snap_membus_t *d_ddrmem
		    ,ap_uint<32> in_mat_sz
		    ,ap_uint<64> in_mat_base_addr
		    ,ap_uint<33> ddr_start_addr
		    ){

	// Copy Image data from HOST to DDR
	snap_membus_t  buf_gmem[MAX_NB_OF_WORDS_READ];
	snapu32_t nb_blocks_to_xfer;
	snapu32_t xfer_size;
	snapu64_t address_xfer_offset=0;
	short rc=0;	

	// buffer size is hardware limited by MAX_NB_OF_BYTES_READ
	if( in_mat_sz % MAX_NB_OF_BYTES_READ == 0)
		nb_blocks_to_xfer = (in_mat_sz / MAX_NB_OF_BYTES_READ);
	else
		nb_blocks_to_xfer = (in_mat_sz / MAX_NB_OF_BYTES_READ) + 1;
	
	// transferring buffers one after the other
	L0:
	for ( int i = 0; i < nb_blocks_to_xfer; i++ ) {

		xfer_size = MIN(in_mat_sz,
				(snapu32_t)MAX_NB_OF_BYTES_READ);

		rc |= read_burst_of_data_from_mem(din_gmem,
						  in_mat_base_addr+ address_xfer_offset,
						  buf_gmem,
						  xfer_size);

		rc |= write_burst_of_data_to_fpga(d_ddrmem,
						  ddr_start_addr + address_xfer_offset,
					       	  buf_gmem, 
						  xfer_size);

		in_mat_sz -= xfer_size;
		address_xfer_offset += (snapu64_t)(xfer_size >> ADDR_RIGHT_SHIFT);
	} // end of L0 loop

	return rc;
}//copy_one_input

//COPY FROM FPGA DDR TO HOST MEMORY
short copy_one_output( snap_membus_t *d_ddrmem
		    ,snap_membus_t *dout_gmem
		    ,ap_uint<32> in_mat_sz
		    ,ap_uint<64> in_mat_base_addr
		    ,ap_uint<33> ddr_start_addr
		    ){

	// Copy Image data from DDR to HOST
	snap_membus_t  buf_gmem[MAX_NB_OF_WORDS_READ];
	snapu32_t nb_blocks_to_xfer;
	snapu32_t xfer_size;
	snapu64_t address_xfer_offset=0;
	short rc=0;	


	// buffer size is hardware limited by MAX_NB_OF_BYTES_READ
	if( in_mat_sz % MAX_NB_OF_BYTES_READ == 0)
		nb_blocks_to_xfer = (in_mat_sz / MAX_NB_OF_BYTES_READ);
	else
		nb_blocks_to_xfer = (in_mat_sz / MAX_NB_OF_BYTES_READ) + 1;
	
	// transferring buffers one after the other
	L0:
	for ( int i = 0; i < nb_blocks_to_xfer; i++ ) {

		xfer_size = MIN(in_mat_sz,
				(snapu32_t)MAX_NB_OF_BYTES_READ);


		rc |= read_burst_of_data_from_fpga(d_ddrmem,
						   ddr_start_addr + address_xfer_offset,
						   buf_gmem,
						   xfer_size);

		rc |= write_burst_of_data_to_mem(dout_gmem,
						 in_mat_base_addr+ address_xfer_offset,
					       	 buf_gmem, 
						 xfer_size);

		in_mat_sz -= xfer_size;
		address_xfer_offset += (snapu64_t)(xfer_size >> ADDR_RIGHT_SHIFT);
	} // end of L0 loop

	return rc;
}//copy_one_output

static int read_row (snap_membus_t *mem_ptr, 		
		     int no_of_bursts_per_row,	
		     int &mem_idx,					
		     snap_membus_t *row
		     ) {	


	// Optimize this to function read_row()
	read_row_label0:for (int burst_idx = 0; burst_idx < no_of_bursts_per_row; burst_idx++) {
    //#pragma HLS PIPELINE II=1
	  row[burst_idx] = mem_ptr[mem_idx];
	  mem_idx++;
	} // end of reading single row

	return(0);
} // end of function read_row

// static int write_row (snap_membus_t *mem_ptr,		// input
// 					 ap_uint<12> no_of_bursts_per_row,		// input
// 					 ap_uint<16> in_size_of_one_row,		// input
// 					 int &mem_idx,					//inout
// 					 snap_membus_t *row) {	// input

//   memcpy((snap_membus_t*)(mem_ptr+mem_idx), row, in_size_of_one_row);
//   mem_idx+=no_of_bursts_per_row;

// 	return(0);

// } // end of function write_row

matrix_element_hw_t get_mean(snap_membus_t *in_row,              
		    int in_no_of_bursts_per_row,        
		    uint32_t in_mat_cols	
		    ) {

    union conv_u_f{
    union_unsigned_element_t mat_element_u;
    matrix_element_t mat_element_f;
    };

    union conv_u_f conv_out;

  ap_uint<32> sum_of_elements = 0;

  unsigned mat_ele_bit_size = (sizeof(matrix_in_element_hw_t)*8);

  for (int burst_idx = 0; burst_idx < in_no_of_bursts_per_row; burst_idx++) {
    //#pragma HLS PIPELINE II=65

    snap_membus_t one_burst = in_row[burst_idx];
    get_mean_label4:for (int ele_idx = 0; ele_idx < 64  ; ele_idx++) {
      //#pragma HLS PIPELINE II = 1
      unsigned lsb = (ele_idx * mat_ele_bit_size);
      unsigned msb = (((ele_idx + 1) * mat_ele_bit_size)-1);
      sum_of_elements += one_burst(msb,lsb);
    }// end of a single burst

  } // end of accumulated sum

  matrix_element_hw_t sum_of_elements_f = (float)sum_of_elements;

  matrix_element_hw_t out_mean=(float)(sum_of_elements_f/in_mat_cols);
#pragma HLS resource variable=out_mean core=FDiv

  return out_mean;

}//End of get_mean

static int set_cov_row_element(snap_membus_t *inp_row,
			       snap_membus_t *inp_row_1,
			       matrix_element_hw_t *cov_row,
			       int no_of_axi_bursts_per_row_rd,
			       uint32_t in_mat_cols,
			       matrix_element_hw_t mean_outrow_val,
			       matrix_element_hw_t mean_inrow_val,
			       ap_uint<12> row_idx_1
			       //int &processing_count,
			       //int &output_mem_idx
			       ){

  matrix_element_hw_t cov_row_element = 0.0;
  unsigned mat_ele_bit_size = (sizeof(matrix_in_element_hw_t)*8);
  
    union conv_u_f{
    union_unsigned_element_t mat_element_u;
    matrix_element_t mat_element_f;
    };

    union conv_u_f cov_out;
    union conv_u_f ele_in;
    union conv_u_f conv_out;   
    
  for (int burst_idx = 0; burst_idx < no_of_axi_bursts_per_row_rd; burst_idx++) {

    snap_membus_t inp_row_burst   = inp_row[burst_idx];
    snap_membus_t inp_row_1_burst = inp_row_1[burst_idx];

  set_cov_row_element_label5:for (int ele_idx = 0; (ele_idx < 64) ; ele_idx++) {//ToDo : make 64 as a define constant
      //#pragma HLS PIPELINE II = 8
      unsigned lsb = (ele_idx * mat_ele_bit_size);
      unsigned msb = (((ele_idx + 1) * mat_ele_bit_size)-1);

      int element   = inp_row_burst(msb,lsb);
      int element_1 = inp_row_1_burst(msb,lsb);

      matrix_element_hw_t cen_row = (element - mean_outrow_val);
      matrix_element_hw_t cen_row_1 = (element_1 - mean_inrow_val);
      cov_row_element += cen_row * cen_row_1;
					    
    }// end of a single burst

  } // end of accumulated sum

//   unsigned mat_cov_ele_bit_size = (sizeof(matrix_element_hw_t)*8);

//   unsigned lsb_index = ((processing_count*((mat_cov_ele_bit_size))));
//   unsigned msb_index = (((processing_count+1)*((mat_cov_ele_bit_size)))-1);

  matrix_element_hw_t cov_row_output_element = (float)(cov_row_element/(in_mat_cols-1));
#pragma HLS resource variable=cov_row_output_element core=FDiv

//   cov_out.mat_element_f = cov_row_output_element;
//   ap_uint<(sizeof(matrix_element_hw_t)*8)> rhs_as_ap_uint = cov_out.mat_element_u;
//   cov_row[output_mem_idx](msb_index,lsb_index) = rhs_as_ap_uint;
//   processing_count++;

//   if( processing_count == 16){//ToDo : make 16 as a define constant
//     output_mem_idx++;
//     processing_count=0;
//   }

  cov_row[row_idx_1]=cov_row_output_element;
  return 0;
}//End of set_cov_row_element

void write_svd_row( ap_uint<8> no_membus_reads_per_mat_row
		  ,snap_membus_t *mem_ptr
		  ,matrix_element_hw_t *p_row
		 ){

  snap_membus_t temp_p_row;
  ap_uint<11> idx=0;

  union {
    union_unsigned_element_t mat_element_u;
    matrix_element_t mat_element_f;
  };

  for(ap_uint<8> col=0; col < NUM_MEMBUS_READS_PER_MAT_ROW ; col++){
    //#pragma HLS PIPELINE II=4

    if(col == no_membus_reads_per_mat_row){
      break;
    }//if(col == no_membus_reads_per_mat_row){

    temp_p_row = 0;
    for(ap_uint<5> i=0 ; i < NUM_MAT_ELEMENTS_PER_MEMBUS_ROW ; i++){

      mat_element_f = p_row[idx+NUM_MAT_ELEMENTS_PER_MEMBUS_ROW-1-i];
      snap_membus_t temp_val = mat_element_u;
      temp_p_row <<=32;
      temp_p_row |= temp_val;

    }//for(ap_uint<5> i=0 ; i < NUM_MAT_ELEMENTS_PER_MEMBUS_ROW ; i++){

    mem_ptr[col]=temp_p_row;
    idx +=NUM_MAT_ELEMENTS_PER_MEMBUS_ROW;

  }//for(ap_uint<8> col=0; col < NUM_MEMBUS_READS_PER_MAT_ROW ; col++){

}//end write_svd_row

void cov_mod(snap_membus_t *d_ddrmem,
	     action_reg *act_reg)
{

	// Get input and output Matrix size in bytes from action_reg
	ap_uint<32> in_mat_sz   = act_reg->Data.inmat.size;
	ap_uint<32> out_matS_sz = act_reg->Data.outmatS.size;

	//Input matrix starts at location 0 of FPGA DDR
	snap_membus_t *in_mem_ptr;
	in_mem_ptr = d_ddrmem;
	
	//Covariance results are stored at location starting
	//after the Input matrix
	ap_uint<33> cov_mat_base_addr = (in_mat_sz >> ADDR_RIGHT_SHIFT);	
	snap_membus_t *cov_out_mem;
	cov_out_mem = (d_ddrmem+cov_mat_base_addr);

	// Get input matrix row and columns
	ap_uint<12> in_mat_rows =  act_reg->Data.inmat_num_rows;
	uint32_t in_mat_cols =  act_reg->Data.inmat_num_cols;
	int no_of_axi_bursts_per_row_rd = 0;
	ap_uint<12> no_of_axi_bursts_per_cov_row_rd = 0;

	  // Number of elements per din_gmem line or per cycle
	  // i.e size of AXI data bus
	  ap_uint<8> no_mt_elements_per_burst = BPERDW/sizeof(matrix_in_element_hw_t);
	  ap_uint<8> no_cov_mt_elements_per_burst = BPERDW/sizeof(matrix_element_hw_t);
	  
	  //No.of AXI bursts per input row read
	  int no_of_membus_elements=0;
	  while(no_of_membus_elements < in_mat_cols){
	    no_of_membus_elements += no_mt_elements_per_burst;
	    no_of_axi_bursts_per_row_rd++;	    
	  }

	  //No.of AXI bursts per cov row read
	  no_of_membus_elements=0;
	  while(no_of_membus_elements < in_mat_rows){
	    no_of_membus_elements += no_cov_mt_elements_per_burst;
	    no_of_axi_bursts_per_cov_row_rd++;	    
	  }

	  unsigned mat_cov_ele_bit_size = (sizeof(matrix_element_hw_t)*8);

	//PML: Required to send output
	ap_uint<16> size_of_one_cov_row = sizeof(matrix_element_hw_t) * in_mat_rows;

	        //PML : Make input as snap_membus_t array to store elements faster
		snap_membus_t inp_row[MAX_INROW_SIZE_IN_MEMBUS_WORDS];
		snap_membus_t inp_row_1[MAX_INROW_SIZE_IN_MEMBUS_WORDS];

		//PML : use snap_membus_t type for cov_row, since this will be used for output
		//snap_membus_t cov_row[MAX_COVROW_SIZE_IN_MEMBUS_WORDS];
		matrix_element_hw_t cov_row[NUM_ROWS];
		snap_membus_t pad_row;

		int outer_mem_idx = 0;
		int inner_mem_idx = 0;
		int ddr_out_idx = 0;

		matrix_element_hw_t mean_array[MAX_LOCAL_MATRIX_ROWS];

		ap_uint<9> pad_processing_count=0;		
		ap_uint<18> pad_mem_offset=0;


#ifdef NO_SYNTH
  
	  printf("No of rows for input(M) Matrix= %f \n",(float)in_mat_rows);
    printf("No of cols for input(M) Matrix= %f \n",(float)in_mat_cols);
    printf("M\n");

		for(int di=0; di < in_mat_rows; di++)
		{
			for(int dj=0; dj < no_of_axi_bursts_per_row_rd; dj++)
			{
				for(int dk=0; dk < 64; dk++)//no_mt_elements_per_burst=64 for a inputmatrix of uint8_t type
				{
					int dk_msb = ((dk+1)*8-1);
					int dk_lsb = dk*8;
					int m_in= (in_mem_ptr)[di*no_of_axi_bursts_per_row_rd+dj]( dk_msb, dk_lsb);
					printf("%d ",m_in);
				}
			}
			printf("\n");
		}
		
#endif

		for (ap_uint<12> row_idx = 0; row_idx < in_mat_rows; row_idx++) {

		  //int processing_count=0;
		  //int output_mem_idx=0;

			//PML : Padding cov row elements which were already calculated
			ap_uint<18> pad_mem_idx = pad_mem_offset;
			for(ap_int<12> row_pad_idx=0; row_pad_idx < row_idx ; row_pad_idx++){
			  //#pragma HLS PIPELINE II=3

			  //PML : Just read the 512-bit chunk in which the copy element resides
			  //pad_row = (cov_out_mem+ out_matS_base_addr)[pad_mem_idx];
 			  pad_row = cov_out_mem[pad_mem_idx];
 			  pad_mem_idx += no_of_axi_bursts_per_cov_row_rd;
      		  			  			  
 			  unsigned pad_lsb_index = pad_processing_count;
 			  unsigned pad_msb_index = pad_processing_count+31;
// 			  unsigned lsb_index     = ((processing_count*((mat_cov_ele_bit_size))));
// 			  unsigned msb_index     = (((processing_count+1)*((mat_cov_ele_bit_size)))-1);
			  
// 			  cov_row[output_mem_idx](msb_index,lsb_index) = pad_row(pad_msb_index,pad_lsb_index);
// 			  processing_count++;	

// 			  if(processing_count == no_cov_mt_elements_per_burst){
// 			    output_mem_idx++;
// 			    processing_count=0;
// 			  }

			  union {
			    union_unsigned_element_t mat_element_u;
			    matrix_element_t mat_element_f;
			  };

			  mat_element_u = pad_row(pad_msb_index,pad_lsb_index);
			  cov_row[row_pad_idx]= mat_element_f;

			}//for(int row_pad_idx=0; row_pad_idx < row_idx ; row_pad_idx++)

			matrix_element_hw_t mean_outrow_val = 0.0;

			//PML : At the start of new iteration, restore the inp_row related elements to inp_row_1
			inner_mem_idx=outer_mem_idx;
			
			for (ap_uint<12> row_idx_1 = row_idx; row_idx_1 < in_mat_rows; row_idx_1++) {

			  matrix_element_hw_t mean_inrow_val = 0.0;

			  read_row (in_mem_ptr,
				    no_of_axi_bursts_per_row_rd,
				    inner_mem_idx,
				    inp_row_1
				    );

			  //PML : If the outer loop index is not 0, this is not the first pass of the memory
			  //Hence mean is already calculated and stored
			  //If its the first pass, calculate mean
			  if(row_idx != 0){
			    mean_inrow_val = mean_array[row_idx_1];
			  }else{
			    mean_inrow_val = get_mean(inp_row_1, 
						      no_of_axi_bursts_per_row_rd, 
						      in_mat_cols);
			    mean_array[row_idx_1] = mean_inrow_val;
			  }

			  //PML : If outer loop and inner loop index are the same, then this 
			  // if covariance of a row by itself. Hence no need of another read. Use the previsoulsy read values
			  // and store them for usage for remainder of the iterations
			  if(row_idx_1 == row_idx){
			    mean_outrow_val=mean_inrow_val;
			    for(int cidx=0; cidx<no_of_axi_bursts_per_row_rd; cidx++){
			      inp_row[cidx]=inp_row_1[cidx];
			    }
			    outer_mem_idx=inner_mem_idx;
			  }

			  //PML : Evaluate a single covariance element
			  // Covariance of A with B
			  set_cov_row_element(inp_row, 
					      inp_row_1,
					      cov_row,
					      no_of_axi_bursts_per_row_rd, 
					      in_mat_cols,
					      mean_outrow_val, 
					      mean_inrow_val,
					      row_idx_1
					      // processing_count,
					      //output_mem_idx
					      );
				
			}//for (int row_idx_1 = 0; row_idx_1 < in_mat_rows; row_idx_1++) 

			//Now we have complete covariance row, write it back to memory
			write_svd_row(	no_of_axi_bursts_per_cov_row_rd,
				       (cov_out_mem+ddr_out_idx), // writing after centered matrix
					cov_row);

			ddr_out_idx+=no_of_axi_bursts_per_cov_row_rd;

			pad_processing_count+=32;
			if(pad_processing_count == 0){
			  pad_mem_offset++;
			}


		}// computing covariance for all rows done
#ifdef NO_SYNTH
    printf("No of rows for COV(COV) Matrix= %f",(float)in_mat_rows);
    printf("No of cols for COV(COV) Matrix= %f",(float)in_mat_rows);
    printf("COV:\n");
		//for(int di=0; di < NUM_ROWS ; di++)
		for(int di=0; di < in_mat_rows; di++)
		{
			//if(di == num_rows_m)
			//{
			//	break;
			//}
			//for(int dj=0; dj < NUM_MEMBUS_READS_PER_MAT_ROW; dj++)
			for(int dj=0; dj < no_of_axi_bursts_per_cov_row_rd; dj++)
			{
				//if(dj == no_of_membus_read_per_mat_row)
				//{
				//	break;
				//}
				for(int dk=0; dk < 16; dk++)//no_cov_mt_elements_per_burst=16 for a inputmatrix of uint8_t type
				{
					int dk_msb = ((dk+1)*32-1);
					int dk_lsb = dk*32;
					float cov_out= (cov_out_mem)[di*no_of_axi_bursts_per_cov_row_rd+dj]( dk_msb, dk_lsb);
					printf("%f ",cov_out);
				}
			}
			printf("\n");
		}
#endif
		


}// END of cov_mod


//End of hls_cov 

void read_svd_row( ap_uint<8> no_membus_reads_per_mat_row
		  ,snap_membus_t *mem_ptr
		  ,matrix_element_hw_t *p_row
		 ){

  snap_membus_t temp_p_row;
  ap_uint<11> idx=0;

  union {
    union_unsigned_element_t mat_element_u;
    matrix_element_t mat_element_f;
  };

  for(ap_uint<8> col=0; col < NUM_MEMBUS_READS_PER_MAT_ROW ; col++){
    //#pragma HLS PIPELINE II=4

    if(col == no_membus_reads_per_mat_row){
      break;
    }//if(col == no_membus_reads_per_mat_row){

    temp_p_row = mem_ptr[col];
    for(ap_uint<5> i=0 ; i < NUM_MAT_ELEMENTS_PER_MEMBUS_ROW ; i++){

      mat_element_u = temp_p_row;
      p_row[idx]= mat_element_f;
      temp_p_row >>=32;
      idx++;

    }//for(ap_uint<5> i=0 ; i < NUM_MAT_ELEMENTS_PER_MEMBUS_ROW ; i++){

  }//for(ap_uint<8> col=0; col < NUM_MEMBUS_READS_PER_MAT_ROW ; col++){

}//end read_svd_row

void read_offDiag(  snap_membus_t *mem_ptr
		   ,unsigned idx
		   ,ap_uint<8> p_col
		   ,ap_uint<4> p_col_ele
		   ,ap_uint<8> q_col
		   ,ap_uint<4> q_col_ele
		   ,matrix_element_hw_t *out_q_p_row
		   ,matrix_element_hw_t *out_q_q_row
		   ,matrix_element_hw_t *out_mat
){

  read_svd_row(1,(mem_ptr+idx+p_col),out_q_p_row);
  out_mat[0] = out_q_p_row[p_col_ele];

  read_svd_row(1,(mem_ptr+idx+q_col),out_q_q_row);
  out_mat[1] = out_q_q_row[q_col_ele];

}//end read_offDiag

void write_offDiag( snap_membus_t *mem_ptr
		   ,unsigned idx
		   ,ap_uint<8> p_col
		   ,ap_uint<4> p_col_ele
		   ,ap_uint<8> q_col
		   ,ap_uint<4> q_col_ele
		   ,matrix_element_hw_t *out_q_p_row
		   ,matrix_element_hw_t *out_q_q_row
){

  write_svd_row(1,(mem_ptr+idx+p_col),out_q_p_row);

  if(p_col==q_col){
    out_q_q_row[p_col_ele]=out_q_p_row[p_col_ele];
    out_q_p_row[q_col_ele]=out_q_q_row[q_col_ele];
  }

  write_svd_row(1,(mem_ptr+idx+q_col),out_q_q_row);

}//end write_offDiag

void read_offDiag_rows(   snap_membus_t *s_mem_ptr
			, snap_membus_t *u_mem_ptr
			, snap_membus_t *v_mem_ptr
			, ap_uint<12> num_rows_m
			, ap_uint<8>  no_membus_reads_per_mat_row
			, ap_uint<8>  col  
			, ap_uint<12> p
			, ap_uint<12> q  
			, matrix_element_hw_t temp_offDiagS_row[NUM_OFFDIAG_ROWS][NUM_MAT_ELEMENTS_PER_MEMBUS_ROW]
			, matrix_element_hw_t temp_offDiagU_row[NUM_OFFDIAG_ROWS][NUM_MAT_ELEMENTS_PER_MEMBUS_ROW]
			, matrix_element_hw_t temp_offDiagV_row[NUM_OFFDIAG_ROWS][NUM_MAT_ELEMENTS_PER_MEMBUS_ROW]
			){

  ap_uint<12> off_idx=0;
  unsigned off_add_idx=0;

  for(ap_uint<12> offDiag=0; offDiag < NUM_ROWS ; offDiag++){

    if(offDiag == num_rows_m){
      break;
    }

    if( (p != offDiag) && (q != offDiag) ){
 	        
      read_svd_row(1,(s_mem_ptr+off_add_idx+col),temp_offDiagS_row[off_idx]);
      read_svd_row(1,(u_mem_ptr+off_add_idx+col),temp_offDiagU_row[off_idx]);
      read_svd_row(1,(v_mem_ptr+off_add_idx+col),temp_offDiagV_row[off_idx]);

      off_idx++;
    }//if( (p != offDiag) && (q != offDiag) ){

    off_add_idx+=no_membus_reads_per_mat_row;
  }//for(ap_uint<12> offDiag=0; offDiag < NUM_ROWS ; offDiag++){

}//end read_offDiag_rows

void write_offDiag_rows(   snap_membus_t *s_mem_ptr
			,  snap_membus_t *u_mem_ptr
			,  snap_membus_t *v_mem_ptr
			, ap_uint<12> num_rows_m
			, ap_uint<8>  no_membus_reads_per_mat_row
			, ap_uint<8>  col  
			, ap_uint<12> p
			, ap_uint<12> q  
			, matrix_element_hw_t temp_offDiagS_row[NUM_OFFDIAG_ROWS][NUM_MAT_ELEMENTS_PER_MEMBUS_ROW]
			, matrix_element_hw_t temp_offDiagU_row[NUM_OFFDIAG_ROWS][NUM_MAT_ELEMENTS_PER_MEMBUS_ROW]
			, matrix_element_hw_t temp_offDiagV_row[NUM_OFFDIAG_ROWS][NUM_MAT_ELEMENTS_PER_MEMBUS_ROW]
			){

  ap_uint<12> off_idx=0;
  unsigned off_add_idx=0;

  for(ap_uint<12> offDiag=0; offDiag < NUM_ROWS ; offDiag++){

    if(offDiag == num_rows_m){
      break;
    }

    if( (p != offDiag) && (q != offDiag) ){
 	        
      write_svd_row(1,(s_mem_ptr+off_add_idx+col),temp_offDiagS_row[off_idx]);
      write_svd_row(1,(u_mem_ptr+off_add_idx+col),temp_offDiagU_row[off_idx]);
      write_svd_row(1,(v_mem_ptr+off_add_idx+col),temp_offDiagV_row[off_idx]);

      off_idx++;
    }//if( (p != offDiag) && (q != offDiag) ){

    off_add_idx+=no_membus_reads_per_mat_row;
  }//for(ap_uint<12> offDiag=0; offDiag < NUM_ROWS ; offDiag++){

}//end write_offDiag_rows

void copy_offDiag_ptoq(   ap_uint<12> no_offdiag_rows_m
			, matrix_element_hw_t temp_offDiagS_p_row[NUM_OFFDIAG_ROWS][NUM_MAT_ELEMENTS_PER_MEMBUS_ROW]
			, matrix_element_hw_t temp_offDiagS_q_row[NUM_OFFDIAG_ROWS][NUM_MAT_ELEMENTS_PER_MEMBUS_ROW]
			, matrix_element_hw_t temp_offDiagU_p_row[NUM_OFFDIAG_ROWS][NUM_MAT_ELEMENTS_PER_MEMBUS_ROW]
			, matrix_element_hw_t temp_offDiagU_q_row[NUM_OFFDIAG_ROWS][NUM_MAT_ELEMENTS_PER_MEMBUS_ROW]
			, matrix_element_hw_t temp_offDiagV_p_row[NUM_OFFDIAG_ROWS][NUM_MAT_ELEMENTS_PER_MEMBUS_ROW]
			, matrix_element_hw_t temp_offDiagV_q_row[NUM_OFFDIAG_ROWS][NUM_MAT_ELEMENTS_PER_MEMBUS_ROW]

			){


  for(ap_uint<12> offDiag=0; offDiag < NUM_OFFDIAG_ROWS ; offDiag++){

    if(offDiag == no_offdiag_rows_m){
      break;
    }

    for(ap_uint<5> i=0 ; i < NUM_MAT_ELEMENTS_PER_MEMBUS_ROW ; i++){
      temp_offDiagS_q_row[offDiag][i]=temp_offDiagS_p_row[offDiag][i];
      temp_offDiagU_q_row[offDiag][i]=temp_offDiagU_p_row[offDiag][i];
      temp_offDiagV_q_row[offDiag][i]=temp_offDiagV_p_row[offDiag][i];
    }//for(ap_uint<5> i=0 ; i < NUM_MAT_ELEMENTS_PER_MEMBUS_ROW ; i++){

   }//for(ap_uint<12> offDiag=0; offDiag < NUM_OFFDIAG_ROWS ; offDiag++){

}//end copy_offDiag_ptoq

void replace_offDiag_row(   matrix_element_hw_t *temp_offDiagS_p_row
			  , matrix_element_hw_t *temp_offDiagS_q_row
			  , matrix_element_hw_t *outS_q_row
			  , matrix_element_hw_t *temp_offDiagU_p_row
			  , matrix_element_hw_t *temp_offDiagU_q_row
			  , matrix_element_hw_t *outU_q_p_row
			  , matrix_element_hw_t *outU_q_q_row
			  , matrix_element_hw_t *temp_offDiagV_p_row
			  , matrix_element_hw_t *temp_offDiagV_q_row
			  , matrix_element_hw_t *outV_q_p_row
			  , matrix_element_hw_t *outV_q_q_row
			  , ap_uint<12> p
			  , ap_uint<12> q  
			 ){

  ap_uint<12> p_col=(p&0xFF0);
  ap_uint<12> q_col=(q&0xFF0);

  for(ap_uint<5> i=0 ; i < NUM_MAT_ELEMENTS_PER_MEMBUS_ROW ; i++){
    temp_offDiagS_p_row[i]=outS_q_row[p_col+i];
    temp_offDiagS_q_row[i]=outS_q_row[q_col+i];
    temp_offDiagU_p_row[i]=outU_q_p_row[i];
    temp_offDiagU_q_row[i]=outU_q_q_row[i];
    temp_offDiagV_p_row[i]=outV_q_p_row[i];
    temp_offDiagV_q_row[i]=outV_q_q_row[i];
  }//for(ap_uint<5> i=0 ; i < NUM_MAT_ELEMENTS_PER_MEMBUS_ROW ; i++){
 
}//replace_offDiag_row

void update_offDiag_result(  ap_uint<12> no_offdiag_rows_m
			   , ap_uint<4> p_col_ele
			   , ap_uint<4> q_col_ele
			   , matrix_element_hw_t temp_offDiagS_p_row[NUM_OFFDIAG_ROWS][NUM_MAT_ELEMENTS_PER_MEMBUS_ROW]
			   , matrix_element_hw_t temp_offDiagS_q_row[NUM_OFFDIAG_ROWS][NUM_MAT_ELEMENTS_PER_MEMBUS_ROW]
			   , matrix_element_hw_t temp_offDiagU_p_row[NUM_OFFDIAG_ROWS][NUM_MAT_ELEMENTS_PER_MEMBUS_ROW]
			   , matrix_element_hw_t temp_offDiagU_q_row[NUM_OFFDIAG_ROWS][NUM_MAT_ELEMENTS_PER_MEMBUS_ROW]
			   , matrix_element_hw_t temp_offDiagV_p_row[NUM_OFFDIAG_ROWS][NUM_MAT_ELEMENTS_PER_MEMBUS_ROW]
			   , matrix_element_hw_t temp_offDiagV_q_row[NUM_OFFDIAG_ROWS][NUM_MAT_ELEMENTS_PER_MEMBUS_ROW]
			   ){

      for(ap_uint<12> offDiag=0; offDiag < NUM_OFFDIAG_ROWS ; offDiag++){

	if(offDiag == no_offdiag_rows_m){
	  break;
	}
  

	    temp_offDiagS_p_row[offDiag][q_col_ele]= temp_offDiagS_q_row[offDiag][q_col_ele];
	    temp_offDiagS_q_row[offDiag][p_col_ele]= temp_offDiagS_p_row[offDiag][p_col_ele];

	    temp_offDiagU_p_row[offDiag][q_col_ele]= temp_offDiagU_q_row[offDiag][q_col_ele];
	    temp_offDiagU_q_row[offDiag][p_col_ele]= temp_offDiagU_p_row[offDiag][p_col_ele];

	    temp_offDiagV_p_row[offDiag][q_col_ele]= temp_offDiagV_q_row[offDiag][q_col_ele];
	    temp_offDiagV_q_row[offDiag][p_col_ele]= temp_offDiagV_p_row[offDiag][p_col_ele];


      }//for(ap_uint<12> offDiag=0; offDiag < NUM_OFFDIAG_ROWS ; offDiag++){

}//update_offDiag_result

void compute_pq_row( 
		     ap_uint<12> num_rows_m
		     ,snap_membus_t *s_mem_ptr
		     ,snap_membus_t *u_mem_ptr
		     ,snap_membus_t *v_mem_ptr
		     ,matrix_element_hw_t *eigen_value_vector
		    ){

  union conv_u_f{
    union_unsigned_element_t mat_element_u;
    matrix_element_t mat_element_f;
  };

  union conv_u_f conv_out, conv_p_out, conv_q_out;
  snap_membus_t out_row;

  matrix_element_hw_t inmatA[2][2];
  matrix_element_hw_t outS[2][2];
  matrix_element_hw_t outU[2][2];
  matrix_element_hw_t outV[2][2];
  matrix_element_hw_t outUIntermediate[2][2];
  matrix_element_hw_t outVIntermediate[2][2];

  matrix_element_hw_t outS_p_row[NUM_ROWS];
  matrix_element_hw_t outU_p_row[NUM_ROWS];
  matrix_element_hw_t outV_p_row[NUM_ROWS];

  matrix_element_hw_t outS_q_row[NUM_ROWS];
  matrix_element_hw_t outU_q_p_row[NUM_MAT_ELEMENTS_PER_MEMBUS_ROW];
  matrix_element_hw_t outU_q_q_row[NUM_MAT_ELEMENTS_PER_MEMBUS_ROW];
  matrix_element_hw_t outV_q_p_row[NUM_MAT_ELEMENTS_PER_MEMBUS_ROW];
  matrix_element_hw_t outV_q_q_row[NUM_MAT_ELEMENTS_PER_MEMBUS_ROW];

  matrix_element_hw_t temp_offDiagS_p_row[NUM_OFFDIAG_ROWS][NUM_MAT_ELEMENTS_PER_MEMBUS_ROW];
  matrix_element_hw_t temp_offDiagU_p_row[NUM_OFFDIAG_ROWS][NUM_MAT_ELEMENTS_PER_MEMBUS_ROW];
  matrix_element_hw_t temp_offDiagV_p_row[NUM_OFFDIAG_ROWS][NUM_MAT_ELEMENTS_PER_MEMBUS_ROW];

  matrix_element_hw_t temp_offDiagS_q_row[NUM_OFFDIAG_ROWS][NUM_MAT_ELEMENTS_PER_MEMBUS_ROW];
  matrix_element_hw_t temp_offDiagU_q_row[NUM_OFFDIAG_ROWS][NUM_MAT_ELEMENTS_PER_MEMBUS_ROW];
  matrix_element_hw_t temp_offDiagV_q_row[NUM_OFFDIAG_ROWS][NUM_MAT_ELEMENTS_PER_MEMBUS_ROW];
 
  ap_uint<8>  no_membus_reads_per_mat_row = (num_rows_m/NUM_MAT_ELEMENTS_PER_MEMBUS_ROW);
  ap_uint<12> no_offdiag_rows_m=num_rows_m-2;


for(ap_uint<8> sweep=0; sweep < 10; sweep++){

  ap_uint<4> p_col_ele=0;
  ap_uint<4> q_col_ele=0;
  
  ap_uint<4> off_p0_col_ele=0;
  ap_uint<4> off_q0_col_ele=0;
  
  ap_uint<4> off_p1_col_ele=0;
  ap_uint<4> off_q1_col_ele=0;
  
  unsigned p_idx=0;
  unsigned q_idx=0;

  ap_uint<8> p_col=0;
  ap_uint<8> q_col=0;

  for(ap_uint<12> p=0; p < (NUM_ROWS-1); p++){

    if(p==(num_rows_m-1)){
      break;
    }

    read_svd_row(no_membus_reads_per_mat_row,(s_mem_ptr+p_idx),outS_p_row);
    read_svd_row(no_membus_reads_per_mat_row,(u_mem_ptr+p_idx),outU_p_row);
    read_svd_row(no_membus_reads_per_mat_row,(v_mem_ptr+p_idx),outV_p_row);
     
    outS[0][0] = outS_p_row[p];//S[p][p]
    outU[0][0] = outU_p_row[p];//U[p][p]
    outV[0][0] = outV_p_row[p];//V[p][p]

    q_col_ele     =p_col_ele+1;
    off_q0_col_ele=off_p0_col_ele+1;
    off_q1_col_ele=off_p1_col_ele+1;

    p_col=(p>>4);
    q_idx = p_idx+no_membus_reads_per_mat_row;

    //Read offdiag p rows
    ap_uint<12> q_temp=p+1;
    read_offDiag_rows(s_mem_ptr,u_mem_ptr,v_mem_ptr,num_rows_m,no_membus_reads_per_mat_row,p_col,p,q_temp
		      ,temp_offDiagS_p_row
		      ,temp_offDiagU_p_row
		      ,temp_offDiagV_p_row
		      //,p_col_ele
		      //,0
		      );

    //Copy p rows into q
    if(off_q0_col_ele!=0){
      copy_offDiag_ptoq(no_offdiag_rows_m
			,temp_offDiagS_p_row,temp_offDiagS_q_row
			,temp_offDiagU_p_row,temp_offDiagU_q_row
			,temp_offDiagV_p_row,temp_offDiagV_q_row
			//,off_q0_col_ele
			);
    }//if(off_q0_col_ele!=0){

    ap_uint<10> replace_idx=p;

    for(ap_uint<12> q=(p+1); q < NUM_ROWS; q++){

      if(q==num_rows_m){
	break;
      }

      q_col=(q>>4);
      
      read_svd_row(no_membus_reads_per_mat_row,(s_mem_ptr+q_idx),outS_q_row);

      outS[0][1] = outS_p_row[q];//S[p][q]
      outS[1][0] = outS_q_row[p];//S[q][p]
      outS[1][1] = outS_q_row[q];//S[q][q]

      outU[0][1] = outU_p_row[q];//U[p][q]
      outV[0][1] = outV_p_row[q];//V[p][q]
	
      read_offDiag(u_mem_ptr,q_idx,p_col,p_col_ele,q_col,q_col_ele,outU_q_p_row,outU_q_q_row,outU[1]);//U[q][p],U[q][q]
      read_offDiag(v_mem_ptr,q_idx,p_col,p_col_ele,q_col,q_col_ele,outV_q_p_row,outV_q_q_row,outV[1]);//V[q][p],V[q][q]

      for(ap_uint<2> i=0; i < 2; i++){
	for(ap_uint<2> j=0; j < 2; j++){
	  outUIntermediate[i][j] = 0;
	  outVIntermediate[i][j] = 0;
	  inmatA[i][j]=0;
	}
      } 
 
      // Call svd_2X2 here since A, S, U, V, UIntermediate, VIntermediate are ready
      svd_basic_mod<2, 2, matrix_element_hw_t, matrix_element_hw_t> (inmatA, outS, outU, outUIntermediate, outV, outVIntermediate);


      if(off_q0_col_ele==0){

	//Shift right by 4 bits (divide by 16) to get col index
	ap_uint<8> q_off_col = (q>>4);

	//Read new offdiag q rows
	read_offDiag_rows(s_mem_ptr,u_mem_ptr,v_mem_ptr,num_rows_m,no_membus_reads_per_mat_row
			  ,q_off_col
			  ,p,q
			  ,temp_offDiagS_q_row
			  ,temp_offDiagU_q_row
			  ,temp_offDiagV_q_row
			  );

      }//if(off_q0_col_ele==0){

#ifdef NO_SYNTH
 
      printf("p=%f,q=%f,off_p0_col_ele=%f,off_q0_col_ele=%f,off_p1_col_ele=%f,off_q1_col_ele=%f,p_col_ele=%f,q_col_ele=%f,rep_idx=%f\n",
	     (float)p,(float)q,(float)off_p0_col_ele,(float)off_q0_col_ele,(float)off_p1_col_ele,(float)off_q1_col_ele,(float)p_col_ele,(float)q_col_ele,(float)replace_idx);


      printf("outS_p_row\n");
      for(int di=0; di < num_rows_m; di++){
	printf("%f ",outS_p_row[di]);	
      }
      printf("\n");

      printf("outS_q_row\n");
      for(int di=0; di < num_rows_m; di++){
	printf("%f ",outS_q_row[di]);	
      }
      printf("\n");


      printf("offDiagRowsS\n");
      for(int di=0; di < no_offdiag_rows_m; di++){
// 	for(int dj=0; dj < 2; dj++){
// 	  printf("%f ",offDiagRowsS[di][dj]);
// 	}

	printf("|");

	for(int dj=0; dj < 16; dj++){
	  printf("%f ",temp_offDiagS_p_row[di][dj]);
	}

	printf("|");

 	for(int dj=0; dj < 16; dj++){
 	  printf("%f ",temp_offDiagS_q_row[di][dj]);
 	}

 	printf("\n");
      }
       printf("\n");

 
#endif

 
       svd_off_diag_row_and_col_updates<NUM_ROWS, NUM_ROWS, NUM_MAT_ELEMENTS_PER_MEMBUS_ROW,matrix_element_hw_t, matrix_element_hw_t, ap_uint<12>, ap_uint<4> > 
	 (
	  outUIntermediate, outVIntermediate
	  , outS_p_row, outS_q_row
	  , temp_offDiagS_p_row, temp_offDiagU_p_row, temp_offDiagV_p_row
	  , temp_offDiagS_q_row, temp_offDiagU_q_row, temp_offDiagV_q_row
	  , num_rows_m
	  , p,q
	  , p_col_ele, q_col_ele
	  );


	outS_p_row[p] = outS[0][0];//S[p][p]
	outS_p_row[q] = outS[0][1];//S[p][q]
	outS_q_row[p] = outS[1][0];//S[q][p]
	outS_q_row[q] = outS[1][1];//S[q][q]
				      
	outU_p_row[p]           = outU[0][0];//U[p][p]
	outU_p_row[q]           = outU[0][1];//U[p][q]
	outU_q_p_row[p_col_ele] = outU[1][0];//U[q][p]
	outU_q_q_row[q_col_ele] = outU[1][1];//U[q][q]
				      
	outV_p_row[p]           = outV[0][0];//V[p][p]
	outV_p_row[q]           = outV[0][1];//V[p][q]
	outV_q_p_row[p_col_ele] = outV[1][0];//V[q][p]
	outV_q_q_row[q_col_ele] = outV[1][1];//V[q][q]
				    
	write_svd_row(no_membus_reads_per_mat_row,(s_mem_ptr+q_idx),outS_q_row);

	write_offDiag(u_mem_ptr,q_idx,p_col,p_col_ele,q_col,q_col_ele,outU_q_p_row,outU_q_q_row);
	write_offDiag(v_mem_ptr,q_idx,p_col,p_col_ele,q_col,q_col_ele,outV_q_p_row,outV_q_q_row);
 	
	if(p_col==q_col){
	  //Update resuld from SVD into temp rows
	  update_offDiag_result(no_offdiag_rows_m
				//,p_col,q_col
				,off_p1_col_ele,off_q1_col_ele
				,temp_offDiagS_p_row,temp_offDiagS_q_row
				,temp_offDiagU_p_row,temp_offDiagU_q_row
				,temp_offDiagV_p_row,temp_offDiagV_q_row
				);
	}


	//Write next row result back to memory, so that in next iteration the values are correct
	if(q!=(num_rows_m-1)){

	  unsigned wr_idx=q_idx+no_membus_reads_per_mat_row;
	  write_offDiag(s_mem_ptr,wr_idx,p_col,off_p1_col_ele,q_col,off_q1_col_ele,temp_offDiagS_p_row[replace_idx],temp_offDiagS_q_row[replace_idx]);
	  write_offDiag(u_mem_ptr,wr_idx,p_col,off_p1_col_ele,q_col,off_q1_col_ele,temp_offDiagU_p_row[replace_idx],temp_offDiagU_q_row[replace_idx]);
	  write_offDiag(v_mem_ptr,wr_idx,p_col,off_p1_col_ele,q_col,off_q1_col_ele,temp_offDiagV_p_row[replace_idx],temp_offDiagV_q_row[replace_idx]);

	}//if(q!=(num_rows_m-1)){

#ifdef NO_SYNTH

   printf("\n");	
   printf("After processing\n");
   printf("\n");	

      printf("p=%f,q=%f,off_p0_col_ele=%f,off_q0_col_ele=%f,off_p1_col_ele=%f,off_q1_col_ele=%f,p_col_ele=%f,q_col_ele=%f,rep_idx=%f\n",
	     (float)p,(float)q,(float)off_p0_col_ele,(float)off_q0_col_ele,(float)off_p1_col_ele,(float)off_q1_col_ele,(float)p_col_ele,(float)q_col_ele,(float)replace_idx);

      printf("outS_p_row\n");
      for(int di=0; di < num_rows_m; di++){
	printf("%f ",outS_p_row[di]);	
      }
      printf("\n");

      printf("outS_q_row\n");
      for(int di=0; di < num_rows_m; di++){
	printf("%f ",outS_q_row[di]);	
      }
      printf("\n");

      printf("offDiagRowsS\n");
      for(int di=0; di < no_offdiag_rows_m; di++){
// 	for(int dj=0; dj < 2; dj++){
// 	  printf("%f ",offDiagRowsS[di][dj]);
// 	}

	printf("|");

	for(int dj=0; dj < 16; dj++){
	  printf("%f ",temp_offDiagS_p_row[di][dj]);
	}

	printf("|");

 	for(int dj=0; dj < 16; dj++){
 	  printf("%f ",temp_offDiagS_q_row[di][dj]);
 	}

 	printf("\n");
       }
       printf("\n");

#endif
  
    q_col_ele++;
    off_q0_col_ele++;
    off_q1_col_ele++;
    
      if(off_q0_col_ele==0){

	//Shift right by 4 bits (divide by 16) to get col index
	ap_uint<8> q_off_col = (q>>4);

	//Write old offdiag q rows
	write_offDiag_rows(s_mem_ptr,u_mem_ptr,v_mem_ptr,num_rows_m,no_membus_reads_per_mat_row
			   ,q_off_col
			   ,p,q
			   ,temp_offDiagS_q_row
			   ,temp_offDiagU_q_row
			   ,temp_offDiagV_q_row
			   );

      }//if(off_q0_col_ele==0){

	if(q!=(num_rows_m-1)){

	  //Update the replace-able row with current q row values
	  replace_offDiag_row( temp_offDiagS_p_row[replace_idx],temp_offDiagS_q_row[replace_idx],outS_q_row
			      ,temp_offDiagU_p_row[replace_idx],temp_offDiagU_q_row[replace_idx],outU_q_p_row,outU_q_q_row
			      ,temp_offDiagV_p_row[replace_idx],temp_offDiagV_q_row[replace_idx],outV_q_p_row,outV_q_q_row
			      ,p,q
			       //,off_p1_col_ele
			      //,off_q1_col_ele
			     );

	}//if(q!=(num_rows_m-1)){


    replace_idx++;

    q_idx +=no_membus_reads_per_mat_row;

// #ifdef NO_SYNTH

//     printf("S\n");
//     for(int di=0; di < num_rows_m ; di++){
//       for(int dj=0; dj < no_membus_reads_per_mat_row; dj++){
// 	for(int dk=0; dk < 16; dk++){
// 	  int dk_msb = ((dk+1)*32-1);
// 	  int dk_lsb = dk*32;
// 	  mat_element_u = (s_mem_ptr)[di*no_membus_reads_per_mat_row+dj]( dk_msb, dk_lsb);
// 	  printf("%f ",mat_element_f);
// 	}
//       }
//       printf("\n");
//     }
//     printf("\n");

//     printf("U\n");
//     for(int di=0; di < num_rows_m ; di++){
//       for(int dj=0; dj < no_membus_reads_per_mat_row; dj++){
// 	for(int dk=0; dk < 16; dk++){
// 	  int dk_msb = ((dk+1)*32-1);
// 	  int dk_lsb = dk*32;
// 	  mat_element_u = (u_mem_ptr)[di*no_membus_reads_per_mat_row+dj]( dk_msb, dk_lsb);
// 	  printf("%f ",mat_element_f);
// 	}
//       }
//       printf("\n");
//     }
//     printf("\n");


//     printf("V\n");
//     for(int di=0; di < num_rows_m ; di++){
//       for(int dj=0; dj < no_membus_reads_per_mat_row; dj++){
// 	for(int dk=0; dk < 16; dk++){
// 	  int dk_msb = ((dk+1)*32-1);
// 	  int dk_lsb = dk*32;
// 	  mat_element_u = (v_mem_ptr)[di*no_membus_reads_per_mat_row+dj]( dk_msb, dk_lsb);
// 	  printf("%f ",mat_element_f);
// 	}
//       }
//       printf("\n");
//     }
//     printf("\n");


// #endif

    }//for(int q=(p+1); q < NUM_ROWS; q++){

    write_svd_row(no_membus_reads_per_mat_row,(s_mem_ptr+p_idx),outS_p_row);
    write_svd_row(no_membus_reads_per_mat_row,(u_mem_ptr+p_idx),outU_p_row);
    write_svd_row(no_membus_reads_per_mat_row,(v_mem_ptr+p_idx),outV_p_row);

    //Write-back old p offDiag rows before starting new iteration
    write_offDiag_rows(s_mem_ptr,u_mem_ptr,v_mem_ptr,num_rows_m,no_membus_reads_per_mat_row
		       ,p_col
		       ,p,(num_rows_m-1)
		       ,temp_offDiagS_p_row
		       ,temp_offDiagU_p_row
		       ,temp_offDiagV_p_row
		       );


#ifdef NO_SYNTH

    union{
	union_unsigned_element_t mat_element_u;
	matrix_element_t mat_element_f;
    };
    union{
    uint32_t unsigned_element;
    float float_element;
    };


    printf("S\n");
    for(int di=0; di < num_rows_m ; di++){
      for(int dj=0; dj < no_membus_reads_per_mat_row; dj++){
	for(int dk=0; dk < 16; dk++){
	  int dk_msb = ((dk+1)*32-1);
	  int dk_lsb = dk*32;
	  mat_element_u = (s_mem_ptr)[di*no_membus_reads_per_mat_row+dj]( dk_msb, dk_lsb);
	  printf("%f ",mat_element_f);
	}
      }
      printf("\n");
    }
    printf("\n");

    printf("U\n");
    for(int di=0; di < num_rows_m ; di++){
      for(int dj=0; dj < no_membus_reads_per_mat_row; dj++){
	for(int dk=0; dk < 16; dk++){
	  int dk_msb = ((dk+1)*32-1);
	  int dk_lsb = dk*32;
	  mat_element_u = (u_mem_ptr)[di*no_membus_reads_per_mat_row+dj]( dk_msb, dk_lsb);
	  printf("%f ",mat_element_f);
	}
      }
      printf("\n");
    }
    printf("\n");


    printf("V\n");
    for(int di=0; di < num_rows_m ; di++){
      for(int dj=0; dj < no_membus_reads_per_mat_row; dj++){
	for(int dk=0; dk < 16; dk++){
	  int dk_msb = ((dk+1)*32-1);
	  int dk_lsb = dk*32;
	  mat_element_u = (v_mem_ptr)[di*no_membus_reads_per_mat_row+dj]( dk_msb, dk_lsb);
	  printf("%f ",mat_element_f);
	}
      }
      printf("\n");
    }
    printf("\n");


#endif


    if(sweep==9)
      {
	eigen_value_vector[p]=outS_p_row[p];
#ifdef NO_SYNTH
	int diag_row=p;
	printf("outS_diag_vector[%d]= %f \t",diag_row,outS_p_row[diag_row]);
	printf("eigen_value_vector[%d]= %f",diag_row,eigen_value_vector[diag_row]);
#endif
      }

    p_col_ele++;
    off_p0_col_ele++;
    off_p1_col_ele++;

    p_idx +=no_membus_reads_per_mat_row;
  }//for(int p=0; p < (NUM_ROWS-1); p++){

 }//for(ap_uint<8> sweep=0; sweep < 10; sweep++){

 eigen_value_vector[num_rows_m-1]=outS_q_row[num_rows_m-1];

#ifdef NO_SYNTH
 for(int diag_row=0; diag_row<num_rows_m; diag_row++)
   {
     if(diag_row==num_rows_m){
       break;
     }
     printf("outS_diag_vector[%d]= %f \t",diag_row,outS_p_row[diag_row]);
     printf("eigen_value_vector[%d]= %f",diag_row,eigen_value_vector[diag_row]);
     printf("\n");
   }
#endif

}//end compute_pq_row

void sort_eigen_vector(  ap_uint<12> in_elements
		       , matrix_element_hw_t *inSdiagMatrix
		       , uint32_t *outSortedIndex
		       )
{
  matrix_element_hw_t outSortedMatrix[SORT_SWEEP_DEPTH];
  for(ap_uint<4> i=0; i<SORT_SWEEP_DEPTH;i++)
    {
      outSortedMatrix[i]=-1073741824.0;
      for(uint32_t inmat_index=0; inmat_index<NUM_ROWS; inmat_index++)
	{

	  if(inmat_index==in_elements)
	    {
	      break;
	    }

	  if(inSdiagMatrix[inmat_index]>outSortedMatrix[i])
	    {
	      outSortedMatrix[i]=inSdiagMatrix[inmat_index];
	      outSortedIndex[i]=inmat_index;
	    }

	}//for(ap_uint<12> inmat_index=0; inmat_index<NUM_ROWS; inmat_index++)

      uint32_t temp_index_val=outSortedIndex[i];
      inSdiagMatrix[temp_index_val]=-1073741824.0;//-2^30

#ifdef NO_SYNTH
      printf("Sorted_eigen_Matrix[%f]= %f \t",(float)i,outSortedMatrix[i]);
      printf("Sorted_Index_of_eigen_value_vector[%f]= %u",(float)i,outSortedIndex[i]);
      printf("\n");
#endif

    }//for(ap_uint<4> i=0; i<SORT_SWEEP_DEPTH;i++)

}//end sort_eigen_vector 


//----------------------------------------------------------------------
//--- MAIN PROGRAM -----------------------------------------------------
//----------------------------------------------------------------------

void svd_mod(snap_membus_t *d_ddrmem
	     ,action_reg *act_reg
	     ,ap_uint<33> out_matS_ddr_start_addr 
	     ,ap_uint<33> out_matU_ddr_start_addr 
	     ,ap_uint<33> out_matV_ddr_start_addr 
	     ,ap_uint<33> out_matEigenSorted_ddr_start_addr 
) {


  //uint32_t size_inmat_bytes, size_outmatS_bytes, size_outmatU_bytes, size_outmatV_bytes;

  union{
    union_unsigned_element_t mat_element_u;
    matrix_element_t mat_element_f;
  };

//     size_inmat_bytes   = act_reg->Data.inmat.size;
//     size_outmatS_bytes = act_reg->Data.outmatS.size;
//     size_outmatU_bytes = act_reg->Data.outmatU.size;
//     size_outmatV_bytes = act_reg->Data.outmatV.size;

    snap_membus_t *s_out_mem;
    snap_membus_t *u_out_mem;
    snap_membus_t *v_out_mem;
 
    s_out_mem=d_ddrmem+out_matS_ddr_start_addr;
    u_out_mem=d_ddrmem+out_matU_ddr_start_addr;
    v_out_mem=d_ddrmem+out_matV_ddr_start_addr;

    ap_uint<12> num_rows_m = act_reg->Data.inmat_num_rows;

#ifdef NO_SYNTH

    printf("No of rows = %f",(float)num_rows_m);
//     printf("S addr = %d\n",o_idxS);
//     printf("U addr = %d\n",o_idxU);
//     printf("V addr = %d\n",o_idxV);

    int no_of_membus_read_per_mat_row = (num_rows_m/NUM_MAT_ELEMENTS_PER_MEMBUS_ROW);

    printf("S\n");
    for(int di=0; di < NUM_ROWS ; di++){
      if(di == num_rows_m){
	break;
      }
      for(int dj=0; dj < NUM_MEMBUS_READS_PER_MAT_ROW; dj++){
	if(dj == no_of_membus_read_per_mat_row){
	  break;
	}
	for(int dk=0; dk < 16; dk++){
	  int dk_msb = ((dk+1)*32-1);
	  int dk_lsb = dk*32;
	  //mat_element_u = (dout_gmem+o_idxS)[di*no_of_membus_read_per_mat_row+dj]( dk_msb, dk_lsb);
	  mat_element_u = s_out_mem[di*no_of_membus_read_per_mat_row+dj]( dk_msb, dk_lsb);
	  printf("%f ",mat_element_f);
	}
      }
      printf("\n");
    }
    printf("\n");

    printf("U\n");
    for(int di=0; di < NUM_ROWS ; di++){
      if(di == num_rows_m){
	break;
      }
      for(int dj=0; dj < NUM_MEMBUS_READS_PER_MAT_ROW; dj++){
	if(dj == no_of_membus_read_per_mat_row){
	  break;
	}
	for(int dk=0; dk < 16; dk++){
	  int dk_msb = ((dk+1)*32-1);
	  int dk_lsb = dk*32;
	  mat_element_u = (u_out_mem)[di*no_of_membus_read_per_mat_row+dj]( dk_msb, dk_lsb);
	  printf("%f ",mat_element_f);
	}
      }
      printf("\n");
    }
    printf("\n");


    printf("V\n");
    for(int di=0; di < NUM_ROWS ; di++){
      if(di == num_rows_m){
	break;
      }
      for(int dj=0; dj < NUM_MEMBUS_READS_PER_MAT_ROW; dj++){
	if(dj == no_of_membus_read_per_mat_row){
	  break;
	}
	for(int dk=0; dk < 16; dk++){
	  int dk_msb = ((dk+1)*32-1);
	  int dk_lsb = dk*32;
	  mat_element_u = (v_out_mem)[di*no_of_membus_read_per_mat_row+dj]( dk_msb, dk_lsb);
	  printf("%f ",mat_element_f);
	}
      }
      printf("\n");
    }
    printf("\n");

#endif

    matrix_element_hw_t eigen_value_vector[NUM_ROWS];
    uint32_t sorted_index[SORT_SWEEP_DEPTH];

    compute_pq_row(  num_rows_m
		    ,s_out_mem
		    ,u_out_mem
		    ,v_out_mem
		    ,eigen_value_vector
		    );


  sort_eigen_vector( num_rows_m
		    ,eigen_value_vector
		    ,sorted_index
		   );

  snap_membus_t *sorted_mem_ptr;
  sorted_mem_ptr=(d_ddrmem+out_matEigenSorted_ddr_start_addr);
	
  snap_membus_t sorted_membus=0;
  for(ap_uint<5> sorted_ele_idx=0; sorted_ele_idx<SORT_SWEEP_DEPTH; sorted_ele_idx++)
    {

      snap_membus_t temp_val=sorted_index[SORT_SWEEP_DEPTH-1-sorted_ele_idx];
      sorted_membus <<=32;
      sorted_membus |= temp_val;

    }//for(ap_uint<5> sorted_ele_idx=0; sorted_ele_idx<SORT_SWEEP_DEPTH; sorted_ele_idx++)

  sorted_mem_ptr[0]=sorted_membus; ///writing back to memory
     	


}// end svd mode

//----------------------------------------------------------------------
//--- MAIN PROGRAM -----------------------------------------------------
//----------------------------------------------------------------------
static int process_action(snap_membus_t *din_gmem,
	      snap_membus_t *dout_gmem,
	      snap_membus_t *d_ddrmem,
	      action_reg *act_reg)
{

  short rc=0;

  // Get input and output Matrix size in bytes from action_reg
  ap_uint<32> in_mat_sz   = act_reg->Data.inmat.size;
  ap_uint<32> out_matS_sz = act_reg->Data.outmatS.size;
  ap_uint<32> out_matU_sz = act_reg->Data.outmatU.size;
  ap_uint<32> out_matV_sz = act_reg->Data.outmatV.size;
  ap_uint<32> out_matEigenSorted_sz = act_reg->Data.outSortedIndexMatS.size;
  ap_uint<32> total_input_sz = (in_mat_sz+out_matS_sz+out_matU_sz+out_matV_sz+out_matEigenSorted_sz);

  // Get input and output Matrix address in bytes from action_reg
  // Address from s/w or action_reg  is BYTE aligned
  // Align it with AXI port width i.e 512 Bit/64 byte boundary
  // let shift by 6 i.e address is aligned at 64 byte boundary
  ap_uint<64> in_mat_base_addr   =  act_reg->Data.inmat.addr   >> ADDR_RIGHT_SHIFT;
  ap_uint<64> out_matS_base_addr =  act_reg->Data.outmatS.addr >> ADDR_RIGHT_SHIFT;
  ap_uint<64> out_matU_base_addr =  act_reg->Data.outmatU.addr >> ADDR_RIGHT_SHIFT;
  ap_uint<64> out_matV_base_addr =  act_reg->Data.outmatV.addr >> ADDR_RIGHT_SHIFT;
  ap_uint<64> out_matEigen_base_addr =  act_reg->Data.outSortedIndexMatS.addr >> ADDR_RIGHT_SHIFT;

  ap_uint<33> in_mat_ddr_start_addr   = 0x0;
  ap_uint<33> out_matS_ddr_start_addr = in_mat_sz >> ADDR_RIGHT_SHIFT;
  ap_uint<33> out_matU_ddr_start_addr = out_matS_ddr_start_addr + (out_matS_sz >> ADDR_RIGHT_SHIFT);
  ap_uint<33> out_matV_ddr_start_addr = out_matU_ddr_start_addr + (out_matU_sz >> ADDR_RIGHT_SHIFT);
  ap_uint<33> out_matEigenSort_ddr_start_addr = out_matV_ddr_start_addr + (out_matV_sz >> ADDR_RIGHT_SHIFT);
  


#ifdef NO_SYNTH

  printf("In Mat Start addr = %f\n",(float)in_mat_ddr_start_addr);
  printf("S  Mat Start addr = %f\n",(float)out_matS_ddr_start_addr);
  printf("U  Mat Start addr = %f\n",(float)out_matU_ddr_start_addr);
  printf("V  Mat Start addr = %f\n",(float)out_matV_ddr_start_addr);
  printf("EigenSorted  Mat Start addr = %f\n",(float)out_matEigenSort_ddr_start_addr);

#endif

  // The card we are using contains 8G DDR$ RAM, check if we can fit input and
  // output images in DDR4 else return with failure 
  if (total_input_sz > CARD_DRAM_SIZE) {
    act_reg->Control.Retc = SNAP_RETC_FAILURE;
    return(0);
  }

  //Copy InMat, U and V Matrices from HOST to FPGA DDR
  //S matrix is filled by cov_mod function, hence not required to copy
  rc=copy_one_input(din_gmem,d_ddrmem,in_mat_sz,in_mat_base_addr,in_mat_ddr_start_addr);
  rc=copy_one_input(din_gmem,d_ddrmem,out_matU_sz,out_matU_base_addr,out_matU_ddr_start_addr);
  rc=copy_one_input(din_gmem,d_ddrmem,out_matV_sz,out_matV_base_addr,out_matV_ddr_start_addr);
  rc=copy_one_input(din_gmem,d_ddrmem,out_matEigenSorted_sz,out_matEigen_base_addr,out_matEigenSort_ddr_start_addr);

  //Covariance calculation
  cov_mod(d_ddrmem, act_reg);

  //SVD calculation
  svd_mod(d_ddrmem, act_reg,out_matS_ddr_start_addr,out_matU_ddr_start_addr,out_matV_ddr_start_addr,out_matEigenSort_ddr_start_addr);

  //Copy Ouput S,U and V matrices from FPGA DDR to HOST DDR
  rc=copy_one_output(d_ddrmem,dout_gmem,out_matS_sz,out_matS_base_addr,out_matS_ddr_start_addr);
  rc=copy_one_output(d_ddrmem,dout_gmem,out_matU_sz,out_matU_base_addr,out_matU_ddr_start_addr);
  rc=copy_one_output(d_ddrmem,dout_gmem,out_matV_sz,out_matV_base_addr,out_matV_ddr_start_addr);
  rc=copy_one_output(d_ddrmem,dout_gmem,out_matEigenSorted_sz,out_matEigen_base_addr,out_matEigenSort_ddr_start_addr);

  act_reg->Control.Retc = SNAP_RETC_SUCCESS;
    return 0;
}
   
//--- TOP LEVEL MODULE -------------------------------------------------
void hls_action(snap_membus_t *din_gmem,
	snap_membus_t *dout_gmem,
	snap_membus_t *d_ddrmem,
	action_reg *act_reg,
	action_RO_config_reg *Action_Config)
{
    // Host Memory AXI Interface - CANNOT BE REMOVED - NO CHANGE BELOW
#pragma HLS INTERFACE m_axi port=din_gmem bundle=host_mem offset=slave depth=512 \
  max_read_burst_length=64  max_write_burst_length=64
#pragma HLS INTERFACE s_axilite port=din_gmem bundle=ctrl_reg offset=0x030

#pragma HLS INTERFACE m_axi port=dout_gmem bundle=host_mem offset=slave depth=512 \
  max_read_burst_length=64  max_write_burst_length=64
#pragma HLS INTERFACE s_axilite port=dout_gmem bundle=ctrl_reg offset=0x040

 // DDR memory Interface - CAN BE COMMENTED IF UNUSED
#pragma HLS INTERFACE m_axi port=d_ddrmem bundle=card_mem0 offset=slave depth=512 \
  max_read_burst_length=64  max_write_burst_length=64
#pragma HLS INTERFACE s_axilite port=d_ddrmem bundle=ctrl_reg offset=0x050

    // Host Memory AXI Lite Master Interface - NO CHANGE BELOW
#pragma HLS DATA_PACK variable=Action_Config
#pragma HLS INTERFACE s_axilite port=Action_Config bundle=ctrl_reg offset=0x010
#pragma HLS DATA_PACK variable=act_reg
#pragma HLS INTERFACE s_axilite port=act_reg bundle=ctrl_reg offset=0x100
#pragma HLS INTERFACE s_axilite port=return bundle=ctrl_reg

    /* Required Action Type Detection - NO CHANGE BELOW */
    //	NOTE: switch generates better vhdl than "if" */
    // Test used to exit the action if no parameter has been set.
    // Used for the discovery phase of the cards */
    switch (act_reg->Control.flags) {
    case 0:
	Action_Config->action_type = PCA_ACTION_TYPE; //TO BE ADAPTED
	Action_Config->release_level = RELEASE_LEVEL;
	act_reg->Control.Retc = 0xe00f;
	return;
	break;
    default:
	    process_action(din_gmem, dout_gmem, d_ddrmem, act_reg);
      //process_action(din_gmem, dout_gmem, d_ddrmem, act_reg);
	break;
    }
} 

//-----------------------------------------------------------------------------
//-- TESTBENCH BELOW IS USED ONLY TO DEBUG THE HARDWARE ACTION WITH HLS TOOL --
//-----------------------------------------------------------------------------

#ifdef NO_SYNTH

int main(void)
{

#define MAT_ROWS     32
#define IN_MAT_COLS  64   //num_cols of input 8bit element matrix
#define ELE_PER_ROW  16   //num_elements of COV_ROW that can accomadate in 512bits=64Bytes=64/(float)=64/4=16elements
#define MEMORY_LINES MAT_ROWS*(MAT_ROWS/ELE_PER_ROW)

    int rc = 0;
    unsigned int i;
    static snap_membus_t  din_gmem[MEMORY_LINES/2];
    static snap_membus_t  dout_gmem[MEMORY_LINES/2];
    static snap_membus_t  d_ddrmem[MEMORY_LINES/2];
    uint32_t matrix_SortedEigenIndexS[SORT_SWEEP_DEPTH] ;

    //snap_membus_t  dout_gmem[2048];
    //snap_membus_t  d_ddrmem[2048];
    action_reg act_reg;
    action_RO_config_reg Action_Config;

    // Discovery Phase .....
    // when flags = 0 then action will just return action type and release
    act_reg.Control.flags = 0x0;
    printf("Discovery : calling action to get config data\n");
    hls_action(din_gmem, dout_gmem, d_ddrmem, &act_reg, &Action_Config);
    fprintf(stderr,
	"ACTION_TYPE:	%08x\n"
	"RELEASE_LEVEL: %08x\n"
	"RETC:		%04x\n",
	(unsigned int)Action_Config.action_type,
	(unsigned int)Action_Config.release_level,
	(unsigned int)act_reg.Control.Retc);

    // Processing Phase .....
    // Fill the memory with 'c' characters
    //unsigned element = 2;
    //memset(din_gmem,  element, 512);
    //printf("Input is : %s\n", (char *)((unsigned long)din_gmem + 0));

    union{
	union_unsigned_element_t mat_element_u;
	matrix_element_t mat_element_f;
    };
    union{
    uint32_t unsigned_element;
    float float_element;
    };

    /*
    for(int i=0; i < MEMORY_LINES; i++)
    {
    	din_gmem[i] = 0;
        for(int j=0; j < MEMORY_LINES; j++)
        {
        	din_gmem[i] = din_gmem[i] << ((sizeof(matrix_element_hw_t))*8);
		mat_element_f = 0;
 		din_gmem[i](((sizeof(matrix_element_hw_t)*8)-1),0) = mat_element_u;
        }
    }
    */


    /*
    matrix_element_hw_t mat1[MEMORY_LINES][MEMORY_LINES] = {
                                        {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16},
                                        {33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48},
                                        {65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80},
                                        {97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,112},
                                        {129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,144},
                                        {161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,176},
                                        {193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,208},
                                        {225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,240},
                                        {257,258,259,260,261,262,263,264,265,266,267,268,269,270,271,272},
                                        {289,290,291,292,293,294,295,296,297,298,299,300,301,302,303,304},
                                        {321,322,323,324,325,326,327,328,329,330,331,332,333,334,335,336},
                                        {353,354,355,356,357,358,359,360,361,362,363,364,365,366,367,368},
                                        {385,386,387,388,389,390,391,392,393,394,395,396,397,398,399,400},
                                        {417,418,419,420,421,422,423,424,425,426,427,428,429,430,431,432},
                                        {449,450,451,452,453,454,455,456,457,458,459,460,461,462,463,464},
                                        {481,482,483,484,485,486,487,488,489,490,491,492,493,494,495,496}
                                       };
    */

    matrix_in_element_hw_t mat1[MAT_ROWS][IN_MAT_COLS] ;
    matrix_element_hw_t matS[MAT_ROWS][MAT_ROWS] ;
    matrix_element_hw_t matU[MAT_ROWS][MAT_ROWS] ;
    matrix_element_hw_t matV[MAT_ROWS][MAT_ROWS] ;


    double double_element = sizeof(matrix_element_hw_t)*MAT_ROWS*MAT_ROWS;
    double_element = (double_element/(MEMDW/8));
    ap_uint<64> ap_element; ap_element(63,0) = (ceil(double_element));
    unsigned num_dout_gmem_lines = ap_element(31,0);
    unsigned num_elements_per_dout_gmem_line = (MEMDW/(sizeof(matrix_element_hw_t)*8));
    unsigned num_elements = 0;
    unsigned row_index=0;
    unsigned col_index=0;
    unsigned break_out = 0;

    //num_dout_gmem_lines=16;
    //num_elements_per_dout_gmem_line=16;
   
    printf("num_dout_gmem_lines             = %u\n",num_dout_gmem_lines);
    printf("num_elements_per_dout_gmem_line = %u\n",num_elements_per_dout_gmem_line);

    matrix_in_element_hw_t int_value=0;
    printf("Input Matrix[%d][%d] = \n", MAT_ROWS, IN_MAT_COLS);
    for(int i=0; i < MAT_ROWS; i++){
      for(int j=0; j < IN_MAT_COLS; j++){
				mat1[i][j]=int_value;
				int_value++;
				printf("%d ", (int)mat1[i][j]);
			}
			printf("\n");
		}//for(int j=0; j < MAT_ROWS; j++)
	//hls::print_matrix<MAT_ROWS, IN_MAT_COLS, matrix_in_element_hw_t, hls::NoTranspose>(mat1, "   ");
	unsigned in_mat_sz_in_bytes = sizeof(matrix_in_element_hw_t) * MAT_ROWS * IN_MAT_COLS;
	//     				16       =  1024/64
	unsigned num_din_gmem_lines = in_mat_sz_in_bytes / BPERDW;
	//					16					 =	64/4
	unsigned num_elements_per_din_gmem_line = (BPERDW / (sizeof(matrix_in_element_hw_t)));

	printf("num_din_gmem_lines             = %u\n",num_din_gmem_lines);
	printf("num_elements_per_din_gmem_line = %u\n",num_elements_per_din_gmem_line);

	for (int i = 0; i < num_din_gmem_lines; i++) {
		for (int j = 0; j < num_elements_per_din_gmem_line; j++) {
			unsigned msb_index = (((j+1) * sizeof(matrix_in_element_hw_t) * 8) - 1);
			unsigned lsb_index =(j * sizeof(matrix_in_element_hw_t) * 8);
			din_gmem[i](msb_index, lsb_index) = mat1[i][j];
		}
	}

    matrix_element_hw_t value=1.0;
    for(int i=0; i < MAT_ROWS; i++){
      for(int j=0; j < MAT_ROWS; j++){
	//mat1[i][j]=value;
	matS[i][j]=0.0;
	value++;
	if(i == j){
	  matU[i][j]=1.0;
	  matV[i][j]=1.0;
	}else{
	  matU[i][j]=0.0;
	  matV[i][j]=0.0;
	}
      }//for(int j=0; j < MAT_ROWS; j++){
    }// for(int i=0; i < MAT_ROWS; i++){
/*
    for(int i=0; i < num_dout_gmem_lines; i++)
		{
			for(int j=0; j < num_elements_per_dout_gmem_line; j++)
			{
				if(col_index==MAT_ROWS) 
				{
					col_index = 0;
					++row_index;
					if(row_index==MAT_ROWS)
					{
						break_out = 1;
						break;
					}
				}
				mat_element_f = mat1[row_index][col_index];
				unsigned lsb_index = (j*(sizeof(matrix_element_hw_t)*8));//((j*((sizeof(matrix_element_hw_t)*8)-1)));
				unsigned msb_index = ((j+1)*(sizeof(matrix_element_hw_t)*8))-1;//(((j+1)*((sizeof(matrix_element_hw_t)*8)-1)));
				//unsigned msb_index = ((MEMDW-1)-(j*((sizeof(matrix_element_hw_t)*8)-1))-j);
				//unsigned lsb_index = ((MEMDW-1)-((j+1)*((sizeof(matrix_element_hw_t)*8)-1))-j);
				ap_uint<(sizeof(matrix_element_hw_t)*8)> rhs_as_ap_uint = mat_element_u;
				din_gmem[i](msb_index,lsb_index) = rhs_as_ap_uint((sizeof(matrix_element_hw_t)*8)-1,0);
				++col_index;
			}
			if(break_out) break;
		}

*/
      
    row_index=0;
    col_index=0;
    break_out = 0;
    for(int i=0; i < num_dout_gmem_lines; i++)
    {
    	for(int j=0; j < num_elements_per_dout_gmem_line; j++)
        {
		if(col_index==MAT_ROWS) 
		{
			col_index = 0;
			++row_index;
			if(row_index==MAT_ROWS)
			{
				break_out = 1;
				break;
			}
		}
		mat_element_f = matS[row_index][col_index];
 	        unsigned lsb_index = (j*(sizeof(matrix_element_hw_t)*8));//((j*((sizeof(matrix_element_hw_t)*8)-1)));
  	        unsigned msb_index = ((j+1)*(sizeof(matrix_element_hw_t)*8))-1;//(((j+1)*((sizeof(matrix_element_hw_t)*8)-1)));
                ap_uint<(sizeof(matrix_element_hw_t)*8)> rhs_as_ap_uint = mat_element_u;
                din_gmem[num_din_gmem_lines+i](msb_index,lsb_index) = rhs_as_ap_uint((sizeof(matrix_element_hw_t)*8)-1,0);
		++col_index;
        }
	if(break_out) break;
    }
    row_index=0;
    col_index=0;
    break_out = 0;
    for(int i=0; i < num_dout_gmem_lines; i++)
    {
    	for(int j=0; j < num_elements_per_dout_gmem_line; j++)
        {
		if(col_index==MAT_ROWS) 
		{
			col_index = 0;
			++row_index;
			if(row_index==MAT_ROWS)
			{
				break_out = 1;
				break;
			}
		}
		mat_element_f = matU[row_index][col_index];
 	        unsigned lsb_index = (j*(sizeof(matrix_element_hw_t)*8));//((j*((sizeof(matrix_element_hw_t)*8)-1)));
  	        unsigned msb_index = ((j+1)*(sizeof(matrix_element_hw_t)*8))-1;//(((j+1)*((sizeof(matrix_element_hw_t)*8)-1)));
                ap_uint<(sizeof(matrix_element_hw_t)*8)> rhs_as_ap_uint = mat_element_u;
                din_gmem[num_din_gmem_lines+(num_dout_gmem_lines)+i](msb_index,lsb_index) = rhs_as_ap_uint((sizeof(matrix_element_hw_t)*8)-1,0);
		++col_index;
        }
	if(break_out) break;
    }
    row_index=0;
    col_index=0;
    break_out = 0;
    for(int i=0; i < num_dout_gmem_lines; i++)
    {
    	for(int j=0; j < num_elements_per_dout_gmem_line; j++)
        {
		if(col_index==MAT_ROWS) 
		{
			col_index = 0;
			++row_index;
			if(row_index==MAT_ROWS)
			{
				break_out = 1;
				break;
			}
		}
		mat_element_f = matV[row_index][col_index];
 	        unsigned lsb_index = (j*(sizeof(matrix_element_hw_t)*8));//((j*((sizeof(matrix_element_hw_t)*8)-1)));
  	        unsigned msb_index = ((j+1)*(sizeof(matrix_element_hw_t)*8))-1;//(((j+1)*((sizeof(matrix_element_hw_t)*8)-1)));
                ap_uint<(sizeof(matrix_element_hw_t)*8)> rhs_as_ap_uint = mat_element_u;
                din_gmem[num_din_gmem_lines+(2*num_dout_gmem_lines)+i](msb_index,lsb_index) = rhs_as_ap_uint((sizeof(matrix_element_hw_t)*8)-1,0);
		++col_index;
        }
	if(break_out) break;
    }

  ////////sorted_eigen_matrix written as 0's in memory
	for(int i=0; i<SORT_SWEEP_DEPTH; i++)
	{
		matrix_SortedEigenIndexS[i]=0;
		unsigned lsb = (i*sizeof(uint32_t)*8);
		unsigned msb = (((i+1)*sizeof(uint32_t)*8)-1);
		din_gmem[num_din_gmem_lines+(3*num_dout_gmem_lines)](msb,lsb) = matrix_SortedEigenIndexS[i];
	}

    // set flags != 0 to have action processed
    act_reg.Control.flags = 0x1; /* just not 0x0 */


    act_reg.Data.inmat.addr = 0;
    act_reg.Data.inmat.size = MAT_ROWS*IN_MAT_COLS*sizeof(matrix_in_element_hw_t);//4096;
    act_reg.Data.inmat.type = SNAP_ADDRTYPE_HOST_DRAM;
    act_reg.Data.inmat_num_rows = MAT_ROWS;
    act_reg.Data.inmat_num_cols= IN_MAT_COLS;


    act_reg.Data.outmatS.addr = (0 << ADDR_RIGHT_SHIFT);
    act_reg.Data.outmatS.size = MAT_ROWS*MAT_ROWS*sizeof(matrix_element_hw_t);//4096;
    act_reg.Data.outmatS.type = SNAP_ADDRTYPE_HOST_DRAM;

    act_reg.Data.outmatU.addr = (num_din_gmem_lines+num_dout_gmem_lines) << (ADDR_RIGHT_SHIFT);
    act_reg.Data.outmatU.size = MAT_ROWS*MAT_ROWS*sizeof(matrix_element_hw_t);//4096;
    act_reg.Data.outmatU.type = SNAP_ADDRTYPE_HOST_DRAM;
    
    act_reg.Data.outmatV.addr = (num_din_gmem_lines+2*num_dout_gmem_lines) << (ADDR_RIGHT_SHIFT);
    act_reg.Data.outmatV.size = MAT_ROWS*MAT_ROWS*sizeof(matrix_element_hw_t);//4096;
    act_reg.Data.outmatV.type = SNAP_ADDRTYPE_HOST_DRAM;
 
    act_reg.Data.outSortedIndexMatS.addr = (num_din_gmem_lines+3*num_dout_gmem_lines) << (ADDR_RIGHT_SHIFT);
    act_reg.Data.outSortedIndexMatS.size = SORT_SWEEP_DEPTH*sizeof(uint32_t);//;
    act_reg.Data.outSortedIndexMatS.type = SNAP_ADDRTYPE_HOST_DRAM;

    printf("Action call \n");
    hls_action(din_gmem, dout_gmem, d_ddrmem, &act_reg, &Action_Config);
    if (act_reg.Control.Retc == SNAP_RETC_FAILURE) {
	fprintf(stderr, " ==> RETURN CODE FAILURE <==\n");
	return 1;
    }

    //printf("Output is : %s\n", (char *)((unsigned long)dout_gmem + 0));

    printf("OutputS\n");
    row_index=0;
    col_index=0;
    break_out = 0;
    for(int i=0; i < num_dout_gmem_lines; i++)
    {
    	for(int j=0; j < num_elements_per_dout_gmem_line; j++)
        {
		if(col_index==MAT_ROWS) 
		{
		  printf("\n");
			col_index = 0;
			++row_index;
			if(row_index==MAT_ROWS)
			{
			  printf("\n");
				break_out = 1;
				break;
			}
		}
		//mat_element_f = matS[row_index][col_index];
 	        unsigned lsb_index = (j*(sizeof(matrix_element_hw_t)*8));//((j*((sizeof(matrix_element_hw_t)*8)-1)));
  	        unsigned msb_index = ((j+1)*(sizeof(matrix_element_hw_t)*8))-1;//(((j+1)*((sizeof(matrix_element_hw_t)*8)-1)));
                //ap_uint<(sizeof(matrix_element_hw_t)*8)> rhs_as_ap_uint = mat_element_u;
                //din_gmem[64+i](msb_index,lsb_index) = rhs_as_ap_uint((sizeof(matrix_element_hw_t)*8)-1,0);
		mat_element_u = dout_gmem[i](msb_index,lsb_index);
		  printf("%f ",mat_element_f);
		++col_index;
        }
	if(break_out) break;
    }
    printf("\nOutputU\n");
    row_index=0;
    col_index=0;
    break_out = 0;
    for(int i=0; i < num_dout_gmem_lines; i++)
    {
    	for(int j=0; j < num_elements_per_dout_gmem_line; j++)
        {
		if(col_index==MAT_ROWS) 
		{
		  printf("\n");
			col_index = 0;
			++row_index;
			if(row_index==MAT_ROWS)
			{
			  printf("\n");
				break_out = 1;
				break;
			}
		}
		//mat_element_f = matS[row_index][col_index];
 	        unsigned lsb_index = (j*(sizeof(matrix_element_hw_t)*8));//((j*((sizeof(matrix_element_hw_t)*8)-1)));
  	        unsigned msb_index = ((j+1)*(sizeof(matrix_element_hw_t)*8))-1;//(((j+1)*((sizeof(matrix_element_hw_t)*8)-1)));
                //ap_uint<(sizeof(matrix_element_hw_t)*8)> rhs_as_ap_uint = mat_element_u;
                //din_gmem[64+i](msb_index,lsb_index) = rhs_as_ap_uint((sizeof(matrix_element_hw_t)*8)-1,0);
		mat_element_u = dout_gmem[num_din_gmem_lines+num_dout_gmem_lines+i](msb_index,lsb_index);
		  printf("%f ",mat_element_f);
		++col_index;
        }
	if(break_out) break;
    }
    printf("\nOutputV\n");
    row_index=0;
    col_index=0;
    break_out = 0;
    for(int i=0; i < num_dout_gmem_lines; i++)
    {
    	for(int j=0; j < num_elements_per_dout_gmem_line; j++)
        {
		if(col_index==MAT_ROWS) 
		{
		  printf("\n");
			col_index = 0;
			++row_index;
			if(row_index==MAT_ROWS)
			{
			  printf("\n");
				break_out = 1;
				break;
			}
		}
		//mat_element_f = matS[row_index][col_index];
 	        unsigned lsb_index = (j*(sizeof(matrix_element_hw_t)*8));//((j*((sizeof(matrix_element_hw_t)*8)-1)));
  	        unsigned msb_index = ((j+1)*(sizeof(matrix_element_hw_t)*8))-1;//(((j+1)*((sizeof(matrix_element_hw_t)*8)-1)));
                //ap_uint<(sizeof(matrix_element_hw_t)*8)> rhs_as_ap_uint = mat_element_u;
                //din_gmem[64+i](msb_index,lsb_index) = rhs_as_ap_uint((sizeof(matrix_element_hw_t)*8)-1,0);
		mat_element_u = dout_gmem[num_din_gmem_lines+(2*num_dout_gmem_lines)+i](msb_index,lsb_index);
		  printf("%f ",mat_element_f);
		++col_index;
        }
	if(break_out) break;
    }

    printf("\nOutput_Sorted_Eigen_vector Indices\n");
    for(int i=0; i<SORT_SWEEP_DEPTH; i++)
      {
	unsigned lsb = (i*sizeof(uint32_t)*8);
	unsigned msb = (((i+1)*sizeof(uint32_t)*8)-1);
	matrix_SortedEigenIndexS[i]= dout_gmem[num_din_gmem_lines+(3*num_dout_gmem_lines)](msb,lsb);
	printf("%u ", matrix_SortedEigenIndexS[i]);
      }
 
    printf("\n");
    printf(">> ACTION TYPE = %08lx - RELEASE_LEVEL = %08lx <<\n",
		    (unsigned int)Action_Config.action_type,
		    (unsigned int)Action_Config.release_level);
    return 0;
}

#endif



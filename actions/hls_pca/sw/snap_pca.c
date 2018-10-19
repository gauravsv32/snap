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

/**
 * SNAP HelloWorld Example
 *
 * Demonstration how to get data into the FPGA, process it using a SNAP
 * action and move the data out of the FPGA back to host-DRAM.
 */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <getopt.h>
#include <malloc.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <assert.h>

#include <snap_tools.h>
#include <libsnap.h>
#include <action_commonheader.h>
#include <snap_hls_if.h>

#include <inttypes.h>

int verbose_flag = 0;

static const char *version = GIT_VERSION;

//static const char *mem_tab[] = { "HOST_DRAM", "CARD_DRAM", "TYPE_NVME" };

/**
 * @brief	prints valid command line options
 *
 * @param prog	current program's name
 */
static void usage(const char *prog)
{
	printf("Usage: %s [-h] [-b, --verbose] [-v, --version]\n"
	"  -C, --card       <cardno>       can be (0...3)\n"
	"  -i, --numrows    <nrows>     num matrix rows\n"
	"  -j, --numcols    <ncols>     num matrix cols\n"
	"  -S, --outputS    <file.bin>  output S file.\n"
	"  -U, --outputU    <file.bin>  output U file.\n"
	"  -V, --outputV    <file.bin>  output V file.\n"
	"  -E, --outputSort <file.bin>  output sorted PCs file.\n"
	"  -A, --type-in <CARD_DRAM, HOST_DRAM, ...>.\n"
	"  -a, --addr-in <addr>      address e.g. in CARD_RAM.\n"
	"  -D, --type-out <CARD_DRAM,HOST_DRAM, ...>.\n"
	"  -d, --addr-out <addr>     address e.g. in CARD_RAM.\n"
	"  -s, --size <size>         size of data.\n"
	"  -t, --timeout             timeout in sec to wait for done.\n"
	"  -X, --verify              verify result if possible\n"
	"  -N, --no-irq              disable Interrupts\n"
	"\n"
	"Useful parameters :\n"
	"-------------------\n"
	"SNAP_TRACE  = 0x0  no debug trace  (default mode)\n"
	"SNAP_TRACE  = 0xF  full debug trace\n"
	"SNAP_CONFIG = FPGA hardware execution   (default mode)\n"
	"SNAP_CONFIG = CPU  software execution\n"
	"\n"
	"Example:\n"
	"----------\n"
	"export SNAP_TRACE=0x0\n"
	"$SNAP_ROOT/software/tools/snap_maint -vvv -C0\n"
	
	"rm /tmp/t2; rm /tmp/t3\n"
	"echo \"Hello world. This is my first CAPI SNAP experience. It's real fun!\n\" > /tmp/t1\n"
	"\n"
	"SNAP_CONFIG=FPGA $ACTION_ROOT/sw/snap_helloworld -i /tmp/t1 -o /tmp/t2\n"
	"SNAP_CONFIG=CPU  $ACTION_ROOT/sw/snap_helloworld -i /tmp/t1 -o /tmp/t3\n"
	"\n"
	"echo \"Display input file\"; cat /tmp/t1\n"
	"Hello world. This is my first CAPI SNAP experience. It's real fun!\n"
	"echo \"Display output file from FPGA EXECUTED ACTION\"; cat /tmp/t2\n"
	"HELLO WORLD. THIS IS MY FIRST CAPI SNAP EXPERIENCE. IT'S REAL FUN!\n"
	"echo \"Display output file from CPU EXECUTED ACTION\"; cat /tmp/t3\n"
	"hello world. this is my first capi snap experience. it's real fun!\n"
	"\n",
        prog);
}

// Function that fills the MMIO registers / data structure 
// these are all data exchanged between the application and the action
static void snap_prepare_pca(struct snap_job *cjob,
				 struct pca_job *mjob,
				 void *addr_inmat,
				 uint32_t size_inmat,
				 uint8_t type_in,
				 void *addr_outmatS,
				 uint32_t size_outmatS,
				 void *addr_outmatU,
				 uint32_t size_outmatU,
				 void *addr_outmatV,
				 uint32_t size_outmatV,
				 void *addr_outSortedIndexMatS,
				 uint32_t size_outSortedIndexMatS,
				 uint8_t type_out,
				 ssize_t num_rows_inmat,  
				 ssize_t num_cols_inmat)
{
	fprintf(stderr, "  prepare pca job of %ld bytes size\n", sizeof(*mjob));

	assert(sizeof(*mjob) <= SNAP_JOBSIZE);
	memset(mjob, 0, sizeof(*mjob));
   
	// Setting input params : where the matrices is located in host memory
	snap_addr_set(&mjob->inmat, addr_inmat, size_inmat, type_in,
		      SNAP_ADDRFLAG_ADDR | SNAP_ADDRFLAG_SRC);
	// Setting output params : where result matrix will be written in host memory
	snap_addr_set(&mjob->outmatS, addr_outmatS, size_outmatS, type_out,
		      SNAP_ADDRFLAG_ADDR | SNAP_ADDRFLAG_DST | SNAP_ADDRFLAG_SRC);
	snap_addr_set(&mjob->outmatU, addr_outmatU, size_outmatU, type_out,
		      SNAP_ADDRFLAG_ADDR | SNAP_ADDRFLAG_DST |
		      SNAP_ADDRFLAG_SRC);
	snap_addr_set(&mjob->outmatV, addr_outmatV, size_outmatV, type_out,
		      SNAP_ADDRFLAG_ADDR | SNAP_ADDRFLAG_DST |
		      SNAP_ADDRFLAG_SRC  | SNAP_ADDRFLAG_END);

	snap_addr_set(&mjob->outSortedIndexMatS, addr_outSortedIndexMatS, size_outSortedIndexMatS, type_out,
		      SNAP_ADDRFLAG_ADDR | SNAP_ADDRFLAG_DST |
		      SNAP_ADDRFLAG_SRC  | SNAP_ADDRFLAG_END);
	mjob->inmat_num_rows = (uint16_t)num_rows_inmat;
	mjob->inmat_num_cols= (uint16_t)num_cols_inmat;

	snap_job_set(cjob, mjob, sizeof(*mjob), NULL, 0);
}

/* main program of the application for the hls_helloworld example        */
/* This application will always be run on CPU and will call either       */
/* a software action (CPU executed) or a hardware action (FPGA executed) */
int main(int argc, char *argv[])
{
	// Init of all the default values used 
	int ch, rc = 0;
	int card_no = 0;
	struct snap_card *card = NULL;
	struct snap_action *action = NULL;
	char device[128];
	struct snap_job cjob;
	struct pca_job mjob;
	//const char *input1 = NULL;
	const char *outputS = NULL;
	const char *outputU = NULL;
	const char *outputV = NULL;
	unsigned long timeout = 600;
	const char *space = "CARD_RAM";
	struct timeval etime, stime;
	ssize_t size_inmat = 1024 * 1024 ;
	ssize_t size_outmatS = 1024 * 1024 ;
	ssize_t size_outmatU = 1024 * 1024 ;
	ssize_t size_outmatV = 1024 * 1024 ;
	matrix_element_t *obuff_matS=NULL, *obuff_matU=NULL, *obuff_matV=NULL;
	inp_matrix_element_t *ibuff_inmat=NULL;
	uint8_t type_in = SNAP_ADDRTYPE_HOST_DRAM;
	uint64_t addr_inmat = 0x0ull;
	uint8_t type_out = SNAP_ADDRTYPE_HOST_DRAM;
	uint64_t addr_outmatS = 0x0ull;
	uint64_t addr_outmatU = 0x0ull;
	uint64_t addr_outmatV = 0x0ull;
	
	const char *outputSort = NULL;
	uint64_t addr_outSortedIndexMatS= 0x0ull;
	ssize_t size_outSortedIndexMatS = 1 * SORT_SWEEP_DEPTH ;
	uint32_t *obuff_SortedIndexMatS=NULL;
	
	int verify = 0;
        ssize_t num_rows_inmat=0;
        ssize_t num_cols_inmat=0;
	int exit_code = EXIT_SUCCESS;
	uint8_t trailing_zeros[1024] = { 0, };
	// default is interrupt mode enabled (vs polling)
	snap_action_flag_t action_irq = (SNAP_ACTION_DONE_IRQ | SNAP_ATTACH_IRQ);

	// collecting the command line arguments
	while (1) {
		int option_index = 0;
		static struct option long_options[] = {
			{ "card",	 required_argument, NULL, 'C' },
			//{ "inputmatrix",required_argument, NULL, 'm' },
			{ "numrowsinputmatrixm",required_argument, NULL, 'i' },
			{"numcolsinputmatrixm", required_argument, NULL, 'j' },
			{ "outputS",	 required_argument, NULL, 'S' },
			{ "outputU",	 required_argument, NULL, 'U' },
			{ "outputV",	 required_argument, NULL, 'V' },
			{ "outputSort",	 required_argument, NULL,'E' },
			{ "src-type",	 required_argument, NULL, 'A' },
			{ "src-addr",	 required_argument, NULL, 'a' },
			{ "dst-type",	 required_argument, NULL, 'D' },
			{ "dst-addr",	 required_argument, NULL, 'd' },
			{ "size",	 required_argument, NULL, 's' },
			{ "timeout",	 required_argument, NULL, 't' },
			{ "verify",	 no_argument,	    NULL, 'X' },
			{ "no-irq",	 no_argument,	    NULL, 'N' },
			{ "version",	 no_argument,	    NULL, 'v' },
			{ "verbose",	 no_argument,	    NULL, 'b' },
			{ "help",	 no_argument,	    NULL, 'h' },
			{ 0,		 no_argument,	    NULL, 0   },
		};

		ch = getopt_long(argc, argv,
                                 "C:i:j:S:U:V:E:A:a:D:d:s:t:XNvbh",
				 long_options, &option_index);
		if (ch == -1)
			break;

		switch (ch) {
		case 'C':
			card_no = strtol(optarg, (char **)NULL, 0);
			break;
			//case 'm':
			//	input1 = optarg;
			//	break;
		case 'S':
			outputS = optarg;
			break;
		case 'U':
			outputU = optarg;
			break;
		case 'V':
			outputV = optarg;
			break;
		case 'E':
			outputSort= optarg;
			break;
		case 'i':
			num_rows_inmat = __str_to_num(optarg);
			break;
		case 'j':
			num_cols_inmat = __str_to_num(optarg);
			break;
			/* input data */
		case 'A':
			space = optarg;
			if (strcmp(space, "CARD_DRAM") == 0)
				type_in = SNAP_ADDRTYPE_CARD_DRAM;
			else if (strcmp(space, "HOST_DRAM") == 0)
				type_in = SNAP_ADDRTYPE_HOST_DRAM;
			else {
				usage(argv[0]);
				exit(EXIT_FAILURE);
			}
			break;
		case 'a':
			addr_inmat = strtol(optarg, (char **)NULL, 0);
			break;
			/* output data */
		case 'D':
			space = optarg;
			if (strcmp(space, "CARD_DRAM") == 0)
				type_out = SNAP_ADDRTYPE_CARD_DRAM;
			else if (strcmp(space, "HOST_DRAM") == 0)
				type_out = SNAP_ADDRTYPE_HOST_DRAM;
			else {
				usage(argv[0]);
				exit(EXIT_FAILURE);
			}
			break;
		case 'd':
			addr_outmatS = strtol(optarg, (char **)NULL, 0);
			break;
                case 's':
                        size_inmat = __str_to_num(optarg);
                        break;
                case 't':
                        timeout = strtol(optarg, (char **)NULL, 0);
                        break;		
                case 'X':
			verify++;
			break;
                case 'N':
                        action_irq = 0;
                        break;
			/* service */
		case 'v':
			printf("%s\n", version);
			exit(EXIT_SUCCESS);
		case 'b':
			verbose_flag = 1;
			break;
		case 'h':
			usage(argv[0]);
			exit(EXIT_SUCCESS);
			break;
		default:
			usage(argv[0]);
			exit(EXIT_FAILURE);
		}
	}

	if (optind != argc) {
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}
	if (argc == 1) {       // to provide help when program is called without argument
          usage(argv[0]);
          exit(EXIT_FAILURE);
        }

 /*if input file for matrix m is defined, use that as input */
	//if (input1 != NULL) {
	//size_inmat= __file_size(input1);
	size_inmat=(num_rows_inmat*num_cols_inmat*sizeof(inp_matrix_element_t)) + (verify ? sizeof(trailing_zeros) : 0);
		//if(size_inmat < 0)
		//	goto out_error;

		// Parse matrix elements from file into an array
		inp_matrix_element_t matrix_m[num_rows_inmat][num_cols_inmat];
		inp_matrix_element_t temp = 0;
		//FILE* input1_fp = fopen(input1, "r");
		//if (input1_fp) {
			for (int i = 0; i < num_rows_inmat; i++) {
				for (int j = 0; j < num_cols_inmat; j++) {
				  //if (!fscanf(input1_fp, "%f",&temp))
				  //break;
				  //matrix_m[i][j] = (inp_matrix_element_t) temp;
					//printf("size = %ld mat[%d] [%d] = %u \n",sizeof(matrix_m),i,j,matrix_m[i][j]);
				  matrix_m[i][j] = temp;
				  temp++;
				}
			}
			//}
			//	fclose(input1_fp);
		size_inmat = sizeof(matrix_m);

		// Allocate in host memory the place to put matrix m  ////
		ibuff_inmat = snap_malloc(size_inmat); //64Bytes aligned malloc
		//if (ibuff_inmat == NULL)
			//goto out_error;
		memset(ibuff_inmat, 0, size_inmat);

		//fprintf(stdout, "reading input data %u bytes from %s\n",
		//		(int) size_inmat, input1);

		// copy elements from matrix to shared host memory
		memcpy(ibuff_inmat, matrix_m, size_inmat);

		//__hexdump(stderr, ibuff_inmat, size_inmat);

		// prepare params to be written in MMIO registers for action
		type_in = SNAP_ADDRTYPE_HOST_DRAM;
		addr_inmat = (unsigned long) ibuff_inmat;
		//}
	///// 
	 /***
	size_inmat = (num_rows_inmat*num_rows_inmat*sizeof(matrix_element_t)) + (verify ? sizeof(trailing_zeros) : 0);

		// Parse matrix elements from file into an array
		matrix_element_t matrix_m[num_rows_inmat][num_rows_inmat];
		matrix_element_t inmat_value = -524288;
			for(int i=0; i < num_rows_inmat; i++)
			{
				for(int j=0; j < num_rows_inmat; j++)
				{
				  //if(!fscanf(input1_fp,"%f",&matrix_m[i][j])) break;
				//	printf("%u \n",matrix_m[i][j]);
				  matrix_m[i][j]=inmat_value;
				  inmat_value++;
				}
			}

		size_inmat = sizeof(matrix_m);

		// Allocate in host memory the place to put matrix m //
		ibuff_inmat = snap_malloc(size_inmat); //64Bytes aligned malloc
		//if (ibuff_inmat == NULL)
		//goto out_error;
		memset(ibuff_inmat, 0, size_inmat);

		// copy elements from matrix to shared host memory
		memcpy(ibuff_inmat, matrix_m, size_inmat);

                __hexdump(stderr, ibuff_inmat, size_inmat);

		// prepare params to be written in MMIO registers for action
		type_in = SNAP_ADDRTYPE_HOST_DRAM;
		addr_inmat = (unsigned long)ibuff_inmat;
		//}
//		matrix_m is filled from input1 file instead of filling it through code*/
///////////////////////////////
		size_outmatS = (num_rows_inmat*num_rows_inmat*sizeof(matrix_element_t)) + (verify ? sizeof(trailing_zeros) : 0);

		//size_outmatS = sizeof(matrix_m); --Amar commented: as cov_mod will write into obuff_matS
		/* Allocate in host memory the place to put the text processed */
		obuff_matS = snap_malloc(size_outmatS); //64Bytes aligned malloc
		//if (obuff_matS == NULL)
		//goto out_error;
		memset(obuff_matS, 0x0, size_outmatS);

		// copy elements from matrix to shared host memory
		//memcpy(obuff_matS, matrix_m, size_outmatS);--Amar commented

    //            __hexdump(stderr, obuff_matS, size_outmatS); --Amar commented
		// prepare params to be written in MMIO registers for action
		type_out = SNAP_ADDRTYPE_HOST_DRAM;
		addr_outmatS = (unsigned long)obuff_matS;


		size_outmatU = (num_rows_inmat*num_rows_inmat*sizeof(matrix_element_t)) + (verify ? sizeof(trailing_zeros) : 0);

		// Parse matrix elements from file into an array
		matrix_element_t matrix_u[num_rows_inmat][num_rows_inmat];
			for(int i=0; i < num_rows_inmat; i++)
			{
				for(int j=0; j < num_rows_inmat; j++)
				{
				  //	if(!fscanf(input1_fp,"%f",&matrix_m[i][j])) break;
				//	printf("%u \n",matrix_m[i][j]);
				  if(i==j){
				    matrix_u[i][j]=1;
				  }else{
				    matrix_u[i][j]=0;
				  }
				}
			}

		size_outmatU = sizeof(matrix_u);
		/* Allocate in host memory the place to put the text processed */
		obuff_matU = snap_malloc(size_outmatU); //64Bytes aligned malloc
		//if (obuff_matU == NULL)
		//goto out_error;
		memset(obuff_matU, 0x0, size_outmatU);

		// copy elements from matrix to shared host memory
		memcpy(obuff_matU, matrix_u, size_outmatU);

                //__hexdump(stderr, obuff_matU, size_outmatU);
		// prepare params to be written in MMIO registers for action
		type_out = SNAP_ADDRTYPE_HOST_DRAM;
		addr_outmatU = (unsigned long)obuff_matU;

		size_outmatV = (num_rows_inmat*num_rows_inmat*sizeof(matrix_element_t)) + (verify ? sizeof(trailing_zeros) : 0);
		// Parse matrix elements from file into an array
		matrix_element_t matrix_v[num_rows_inmat][num_rows_inmat];

			for(int i=0; i < num_rows_inmat; i++)
			{
				for(int j=0; j < num_rows_inmat; j++)
				{
				  //	if(!fscanf(input1_fp,"%f",&matrix_m[i][j])) break;
				//	printf("%u \n",matrix_m[i][j]);
				  if(i==j){
				    matrix_v[i][j]=1;
				  }else{
				    matrix_v[i][j]=0;
				  }
				}
			}


		/* Allocate in host memory the place to put the text processed */
		obuff_matV = snap_malloc(size_outmatV); //64Bytes aligned malloc
		//if (obuff_matV == NULL)
		//goto out_error;
		memset(obuff_matV, 0x0, size_outmatV);
		fprintf(stdout, "reading input data %u bytes from %s\n",
			(int)size_outmatV, outputV);

		// copy elements from matrix to shared host memory
		memcpy(obuff_matV, matrix_v, size_outmatV);

                //__hexdump(stderr, obuff_matV, size_outmatV);
		// prepare params to be written in MMIO registers for action
		type_out = SNAP_ADDRTYPE_HOST_DRAM;
		addr_outmatV = (unsigned long)obuff_matV;
	        
///////  Sorted_Matrix of Eigen vector Indices
		size_outSortedIndexMatS = (16*sizeof(uint32_t)) + (verify ? sizeof(trailing_zeros) : 0);

		// Parse matrix elements from file into an array
		uint32_t matrix_SortedEigenIndexS[16];
				for(int j=0; j < 16; j++)
				{
				  //	if(!fscanf(input1_fp,"%f",&matrix_m[i][j])) break;
				//	printf("%u \n",matrix_m[i][j]);
				    matrix_SortedEigenIndexS[j]=0;
				}

		size_outSortedIndexMatS= sizeof(matrix_SortedEigenIndexS);
		/* Allocate in host memory the place to put the text processed */
		obuff_SortedIndexMatS = snap_malloc(size_outSortedIndexMatS); //64Bytes aligned malloc
		//if (obuff_matU == NULL)
		//goto out_error;
		memset(obuff_SortedIndexMatS, 0x0, size_outSortedIndexMatS);

		// copy elements from matrix to shared host memory
		memcpy(obuff_SortedIndexMatS, matrix_SortedEigenIndexS, size_outSortedIndexMatS);

                //__hexdump(stderr, obuff_matU, size_outmatU);
		// prepare params to be written in MMIO registers for action
		type_out = SNAP_ADDRTYPE_HOST_DRAM;
		addr_outSortedIndexMatS= (unsigned long)obuff_SortedIndexMatS;
///////////////
	/* Display the parameters that will be used for the example
	printf("PARAMETERS:\n"
	       "  input1:       %s\n"
	       "  input2:       %s\n"
	       "  output:      %s\n"
	       "  type_in:     %x %s\n"
	       "  addr_mat1:     %016llx\n"
	       "  addr_mat2:     %016llx\n"
	       "  type_out:    %x %s\n"
	       "  addr_outmat:    %016llx\n"
	       "  size_mat1: %08lx\n"
	       "  size_mat2: %08lx\n"
	       "  size_outmat: %08lx\n",
	       input1  ? input1  : "unknown", input2 ? input2 : "unknown", output ? output : "unknown",
	       type_in, mem_tab[type_in],  
               (long long)addr_mat1, (long long)addr_mat2,
	       type_out, mem_tab[type_out], 
	       (long long)addr_outmat,
	       size_mat1,
	       size_mat2,
               size_outmat);*/
             

	// Allocate the card that will be used
	snprintf(device, sizeof(device)-1, "/dev/cxl/afu%d.0s", card_no);
	card = snap_card_alloc_dev(device, SNAP_VENDOR_ID_IBM,
				   SNAP_DEVICE_ID_SNAP);
	if (card == NULL) {
		fprintf(stderr, "err: failed to open card %u: %s\n",
			card_no, strerror(errno));
                fprintf(stderr, "Default mode is FPGA mode.\n");
                fprintf(stderr, "Did you want to run CPU mode ? => add SNAP_CONFIG=CPU before your command.\n");
                fprintf(stderr, "Otherwise make sure you ran snap_find_card and snap_maint for your selected card.\n");
		goto out_error;
	}

	// Attach the action that will be used on the allocated card
	action = snap_attach_action(card, PCA_ACTION_TYPE, action_irq, 60);
	if (action == NULL) {
		fprintf(stderr, "err: failed to attach action %u: %s\n",
			card_no, strerror(errno));
		goto out_error1;
	}

	// Fill the stucture of data exchanged with the action
	snap_prepare_pca(&cjob, &mjob,
			     (void *)addr_inmat,  size_inmat, type_in,
			     (void *)addr_outmatS, size_outmatS, 
			     (void *)addr_outmatU, size_outmatU, 
			     (void *)addr_outmatV, size_outmatV, 
			     (void *)addr_outSortedIndexMatS, size_outSortedIndexMatS, 
			     type_out, 
					 num_rows_inmat,
					 num_cols_inmat);

	// uncomment to dump the job structure
	//__hexdump(stderr, &mjob, sizeof(mjob));


	// Collect the timestamp BEFORE the call of the action
	gettimeofday(&stime, NULL);

	// Call the action will:
	//    write all the registers to the action (MMIO) 
	//  + start the action 
	//  + wait for completion
	//  + read all the registers from the action (MMIO) 
	rc = snap_action_sync_execute_job(action, &cjob, timeout);

	// Collect the timestamp AFTER the call of the action
	gettimeofday(&etime, NULL);
	if (rc != 0) {
		fprintf(stdout, "err: job execution %d: %s!\n", rc,
			strerror(errno));
		//goto out_error2;
	}

	/* If the output buffer is in host DRAM we can write it to a file */
	if (outputS != NULL) {
		fprintf(stdout, "writing output data %p %d bytes to %s\n",
			obuff_matS, (int)size_outmatS, outputS);

		FILE* out_file = fopen(outputS,"w+");
		if(out_file)
		{
			unsigned i=0;
			for(; i < num_rows_inmat; i++)
			{
				unsigned j=0;
				for(; j < num_rows_inmat; j++)
				{
					fprintf(out_file,"%f",obuff_matS[i*num_rows_inmat+j]);
					fprintf(out_file,"%s"," ");				
					//printf("%u\n",obuff[i*num_cols_n+j]);
				}
				fprintf(out_file,"%s","\n");
			}
                }
		fclose(out_file);	
         }
         if(outputU != NULL) {
		fprintf(stdout, "writing output data %p %d bytes to %s\n",
			obuff_matU, (int)size_outmatU, outputU);

		FILE* out_file = fopen(outputU,"w+");
                if(out_file) {
			unsigned i=0;
			for(; i < num_rows_inmat; i++)
			{
				unsigned j=0;
				for(; j < num_rows_inmat; j++)
				{
					fprintf(out_file,"%f",obuff_matU[i*num_rows_inmat+j]);
					fprintf(out_file,"%s"," ");				
					//printf("%u\n",obuff[i*num_cols_n+j]);
				}
				fprintf(out_file,"%s","\n");
			}
                 }
		fclose(out_file);	
          }
          if(outputV) {
		fprintf(stdout, "writing output data %p %d bytes to %s\n",
			obuff_matV, (int)size_outmatV, outputV);

		FILE* out_file = fopen(outputV,"w+");
                 if(out_file) {
			unsigned i=0;
			for(; i < num_rows_inmat; i++)
			{
				unsigned j=0;
				for(; j < num_rows_inmat; j++)
				{
					fprintf(out_file,"%f",obuff_matV[i*num_rows_inmat+j]);
					fprintf(out_file,"%s"," ");				
					//printf("%u\n",obuff[i*num_cols_n+j]);
				}
				fprintf(out_file,"%s","\n");
			}
                  }
		fclose(out_file);	
           }             
		
	if (outputSort!= NULL) {
		fprintf(stdout, "writing output data %p %d bytes to %s\n",
			obuff_SortedIndexMatS, (int)size_outSortedIndexMatS, outputSort);

		FILE* out_file = fopen(outputSort,"w+");
		if(out_file)
		{
			unsigned i=0;
			for(; i < SORT_SWEEP_DEPTH; i++)
			{
					fprintf(out_file,"%d",obuff_SortedIndexMatS[i]);
					fprintf(out_file,"%s"," ");				
					//printf("%u\n",obuff[i*num_cols_n+j]);
			}
			fprintf(out_file,"%s","\n");
		}
		fclose(out_file);	
	}
	  //__hexdump(stderr, obuff_matS, size_outmatS);
	  //__hexdump(stderr, obuff_matU, size_outmatU);
	  //__hexdump(stderr, obuff_matV, size_outmatV);

		//rc = __file_write(output, obuff, size_outmat);
		//if (rc < 0)
		//	goto out_error2;
	

	// test return code
	(cjob.retc == SNAP_RETC_SUCCESS) ? fprintf(stdout, "SUCCESS\n") : fprintf(stdout, "FAILED\n");
	if (cjob.retc != SNAP_RETC_SUCCESS) {
		fprintf(stdout, "err: Unexpected RETC=%x!\n", cjob.retc);
		//goto out_error2;
	}

	// Compare the input and output if verify option -X is enabled
//	if (verify) {
//		if ((type_in  == SNAP_ADDRTYPE_HOST_DRAM) &&
//		    (type_out == SNAP_ADDRTYPE_HOST_DRAM)) {
//			rc = memcmp(ibuff, obuff, size);
//			if (rc != 0)
//				exit_code = EX_ERR_VERIFY;
//
//			rc = memcmp(obuff + size, trailing_zeros, 1024);
//			if (rc != 0) {
//				fprintf(stderr, "err: trailing zero "
//					"verification failed!\n");
//				__hexdump(stderr, obuff + size, 1024);
//				exit_code = EX_ERR_VERIFY;
//			}
//
	//	} else
	//		fprintf(stderr, "warn: Verification works currently "
	//			"only with HOST_DRAM\n");
	//}
	// Display the time of the action call (MMIO registers filled + execution)
	fprintf(stdout, "SNAP matrix multiply took %lld usec\n",
		(long long)timediff_usec(&etime, &stime));

	// Detach action + disallocate the card
	snap_detach_action(action);
	snap_card_free(card);

	__free(obuff_matS);
	__free(obuff_matU);
	__free(obuff_matV);
	__free(ibuff_inmat);
	__free(obuff_SortedIndexMatS);
	exit(exit_code);

 //out_error2:
	snap_detach_action(action);
 out_error1:
	snap_card_free(card);
 out_error:
	__free(obuff_matS);
	__free(obuff_matU);
	__free(obuff_matV);
	__free(ibuff_inmat);
	__free(obuff_SortedIndexMatS);
	exit(EXIT_FAILURE);
}


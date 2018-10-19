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
 * Example to use the FPGA to find patterns in a byte-stream.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <endian.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <libsnap.h>
#include <linux/types.h>	/* __be64 */
#include <asm/byteorder.h>

#include <snap_internal.h>
#include <snap_tools.h>
#include <action_commonheader.h>

static int mmio_write32(struct snap_card *card,
			uint64_t offs, uint32_t data)
{
	act_trace("  %s(%p, %llx, %x)\n", __func__, card,
		  (long long)offs, data);
	return 0;
}

static int mmio_read32(struct snap_card *card,
		       uint64_t offs, uint32_t *data)
{
	act_trace("  %s(%p, %llx, %x)\n", __func__, card,
		  (long long)offs, *data);
	return 0;
}

/* Main program of the software action */
static int action_main(struct snap_sim_action *action,
		       void *job, unsigned int job_len)
{
	struct pca_job *js = (struct pca_job *)job;
	matrix_element_t *mat1, *mat2, *outmat;
	uint16_t num_rows_m;
        unsigned int blah = job_len;

	/* No error checking ... */
	//act_trace("%s(%p, %p, %d) type_mat1=%d type_mat2=%d type_out=%d jobsize %ld bytes\n",
	//	  __func__, action, job, job_len, js->inmat.type, js->outmatS.type,
	//	  sizeof(*js));

	// Uncomment to dump the action params
	//__hexdump(stderr, js, sizeof(*js));

	// get the parameters from the structure
	mat1 = (matrix_element_t *)(unsigned long)js->inmat.addr;
	mat2 = (matrix_element_t *)(unsigned long)js->inmat.addr;
	outmat = (matrix_element_t *)(unsigned long)js->outmatS.addr;
	num_rows_m = js->inmat_num_rows;

	for(unsigned i=0; i < num_rows_m; i++)
	{
		for(unsigned j=0; j < num_rows_m; j++)
		{
			for(unsigned k=0; k < num_rows_m; k++)
			{
				(*(outmat+(i*num_rows_m)+j)) += (blah+(*(mat1+(i*num_rows_m)+k))*(*(mat2+(k*num_rows_m)+j)));
			}
		}
	}

	// update the return code to the SNAP job manager
	action->job.retc = SNAP_RETC_SUCCESS;
	return 0;

}

/* This is the switch call when software action is called */
/* NO CHANGE TO BE APPLIED BELOW OTHER THAN ADAPTING THE ACTION_TYPE NAME */
static struct snap_sim_action action = {
	.vendor_id = SNAP_VENDOR_ID_ANY,
	.device_id = SNAP_DEVICE_ID_ANY,
	.action_type = PCA_ACTION_TYPE, // Adapt with your ACTION NAME

	.job = { .retc = SNAP_RETC_FAILURE, },
	.state = ACTION_IDLE,
	.main = action_main,
	.priv_data = NULL,	/* this is passed back as void *card */
	.mmio_write32 = mmio_write32,
	.mmio_read32 = mmio_read32,

	.next = NULL,
};

static void _init(void) __attribute__((constructor));

static void _init(void)
{
	snap_action_register(&action);
}

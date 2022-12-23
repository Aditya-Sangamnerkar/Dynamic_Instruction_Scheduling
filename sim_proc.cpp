#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sim_proc.h"
#include <iostream>



//--------------------------------------------------------------------------
// Simulator Global Variables 
//---------------------------------------------------------------------------

// global instruction count 
int instruction_global_index = 0;
// simulator clock cycle
int simulator_cycle = 0;

//--------------------------------------------------------------------------
// Advance Cycle
//---------------------------------------------------------------------------

bool Advance_Cycle(bool pipeline_empty)
{	
	simulator_cycle++;
	if(pipeline_empty)
		return false;
	
	return true;
}

bool Advance_Debug_Cycle()
{
	if(simulator_cycle < 15){
		simulator_cycle++;
		return true;
	}
	return false;
}

//-----------------------------------------------------------------------------
// Instruction : Methods
//-----------------------------------------------------------------------------

// instruction : constructor
Instruction :: Instruction(unsigned long int pc, int opcode, int src_register_one_index, int src_register_two_index, int dest_register_index)
{

		

		// source register one init
		this->src_register_one.arf_index = src_register_one_index;
		this->src_register_one.arf_valid = false;
		this->src_register_one.rob_tag = 0;
		this->src_register_one.rob_valid = false;
		this->src_register_one.rdy = false;

		// source register two init
		this->src_register_two.arf_index = src_register_two_index;
		this->src_register_two.arf_valid = false;
		this->src_register_two.rob_tag = 0;
		this->src_register_two.rob_valid = false;
		this->src_register_two.rdy = false;

		// destination register init
		this->dest_register.rob_tag = 0;
		this->dest_register.arf_index = dest_register_index;

		// timing info
		for(int i=0; i<9; i++){
			this->instruction_stage_timing[i].pipeline_start_cycle = 0;
			this->instruction_stage_timing[i].pipeline_cycle_count = 0;
		}

		// instruction metadata
		this->pc = pc;
		this->opcode = opcode;
		this->instruction_rob_tag = 0;
		this->instruction_rob_valid = false;
		this->global_index = 0;

		// execute cycles
		if(opcode == 0)
			this->total_execute_cycles = 1;
		if(opcode == 1)
			this->total_execute_cycles = 2;
		if(opcode == 2)
			this->total_execute_cycles = 5;

}
// source register : debug
void debug_source_register(SourceRegister reg)
{
	std::cout << "\nARF index : " << reg.arf_index;
	std::cout << " ARF valid : " << reg.arf_valid;
	std::cout << " ROB tag : " << reg.rob_tag;
	std::cout << " ROB valid : " << reg.rob_valid;
	std::cout << " rdy : " << reg.rdy;
}
// destination register : debug
void debug_dest_register(DestinationRegister reg)
{
	std::cout << "\nARF index : " << reg.arf_index;
	std::cout << " ROB tag : " << reg.rob_tag;
}
// timing info : debug
void debug_timing_info(TimingInfo stageTimingInfo)
{
	
	std::cout << "{" << stageTimingInfo.pipeline_start_cycle << "," << stageTimingInfo.pipeline_cycle_count << "} ";
}

void Instruction :: debug_instruction()
{
	// metadata
	std::cout << "\n------------------------------";
	printf("\npc : %lx", this->pc);
	std::cout << "\nopcode : " << this->opcode;
	std::cout << "\nglobal index : " << this->global_index;
	
	std::cout << "\nsrc 1  : ";
	debug_source_register(this->src_register_one);

	std::cout << "\nsrc 2  : ";
	debug_source_register(this->src_register_two);

	std::cout << "\ndest : ";
	debug_dest_register(this->dest_register);

	std::cout << "\nstage timing : ";
	for(int i=0; i<9; i++)
		debug_timing_info(this->instruction_stage_timing[i]);
	std::cout << "\n------------------------------";
}

//-----------------------------------------------------------------------------
// ReorderBufferEntry : Methods
//-----------------------------------------------------------------------------

// reorder buffer entry : constructor
ReorderBufferEntry :: ReorderBufferEntry()
{
		this->rob_tag = 0;
		this->rdy = false;
		this->rob_instruction = NULL;
}

//-----------------------------------------------------------------------------
// PipelineRegister : Methods
//-----------------------------------------------------------------------------

// pipeline register : constructor
PipelineRegister :: PipelineRegister(int width)
{
		this->instruction_bundle = new Instruction*[width];
		for(int i=0; i<width; i++)
			this->instruction_bundle[i] = NULL;
		this->bundle_empty = true;
		this->bundle_length	= 0;
}

//-----------------------------------------------------------------------------
// NonBlockingPipelineRegister : Methods
//-----------------------------------------------------------------------------

// non blocking pipeline register : constructor
NonBlockingPipelineRegister :: NonBlockingPipelineRegister()
{
	this->valid = false;
	this->pipe_instruction = NULL;
}

//-----------------------------------------------------------------------------
// IssueQueueEntry : Methods
//-----------------------------------------------------------------------------

// issue queue entry : constructor
IssueQueueEntry :: IssueQueueEntry()
{
		this->valid = false;
		this->queue_instruction = NULL;
}

//-----------------------------------------------------------------------------
// MicroProcessor : Methods
//-----------------------------------------------------------------------------

// microprocessor : constructor
MicroProcessor :: MicroProcessor(int width, int iq_size, int rob_size)
{
		// processor params init
		this->params.width = width;
		this->params.iq_size = iq_size;
		this->params.rob_size = rob_size;

		// RMT init
		for(int i=0 ;i<ARF_N; i++){
			this->RMT[i].arf_index = i;
			this->RMT[i].rob_tag = 0;
			this->RMT[i].valid = false;
		}

		// ROB init
		this->ROB = new ReorderBufferEntry[this->params.rob_size]();
		for(int i=0 ;i<this->params.rob_size; i++)
			this->ROB[i].rob_tag = i;
		this->head_pointer = -1;
		this->tail_pointer = -1;

		// Issue Queue init
		this->Issue_Queue = new IssueQueueEntry[this->params.iq_size]();

		// Pipeline Registers init
		this->DE = new PipelineRegister(this->params.width);
		this->RN = new PipelineRegister(this->params.width);
		this->RR = new PipelineRegister(this->params.width);
		this->DI = new PipelineRegister(this->params.width);

		// Non blocking pipeline registers init
		this->execute_list = new NonBlockingPipelineRegister[this->params.width * 5]();
		this->WB = new NonBlockingPipelineRegister[this->params.width * 5]();
		
}

//------------------------------
// microprocessor :: Retire
//------------------------------
void MicroProcessor	 :: Retire()
{
	/*Retire up to 
			WIDTH consecutive 
			“ready” instructions 
			from the head of the ROB.
	*/

	int width_count	= 0;
	while(width_count < this->params.width)
	{		
		// ready instruction at rob head
		if(this->ROB[this->head_pointer].rdy)
		{
			// remove instruction from ROB
			Instruction* popped_instruction	= this->dequeue_rob();

			// valid instruction
			if(popped_instruction != NULL)
			{
				// calculate cycle time
				this->calculate_cycle_time(popped_instruction);
				// invalidate rmt 
				int dest_arf_index = popped_instruction->dest_register.arf_index;
				int dest_rob_tag  = popped_instruction->dest_register.rob_tag;
				this->invalidate_rmt(dest_arf_index, dest_rob_tag);
				// print instruction
				this->print_popped_instruction(popped_instruction);

			}
			// no valid instruction
			else
			{
				break;
			}
		}
		// no ready instruction at rob head
		else
		{		
			break;
		}
		width_count++;
	}
}

// microprocessor : print popped instruction
void MicroProcessor :: print_popped_instruction(Instruction* inst)
{
	printf("\n%d fu{%d} src{%d,%d} dst{%d} FE{%d,%d} DE{%d,%d} RN{%d,%d} RR{%d,%d} DI{%d,%d} IS{%d,%d} EX{%d,%d} WB{%d,%d} RT{%d,%d}"
		, inst->global_index
		, inst->opcode
		, inst->src_register_one.arf_index
		, inst->src_register_two.arf_index
		, inst->dest_register.arf_index
		, inst->instruction_stage_timing[FE_S].pipeline_start_cycle
		, inst->instruction_stage_timing[FE_S].pipeline_cycle_count
		, inst->instruction_stage_timing[DE_S].pipeline_start_cycle
		, inst->instruction_stage_timing[DE_S].pipeline_cycle_count
		, inst->instruction_stage_timing[RN_S].pipeline_start_cycle
		, inst->instruction_stage_timing[RN_S].pipeline_cycle_count
		, inst->instruction_stage_timing[RR_S].pipeline_start_cycle
		, inst->instruction_stage_timing[RR_S].pipeline_cycle_count
		, inst->instruction_stage_timing[DI_S].pipeline_start_cycle
		, inst->instruction_stage_timing[DI_S].pipeline_cycle_count
		, inst->instruction_stage_timing[IS_S].pipeline_start_cycle
		, inst->instruction_stage_timing[IS_S].pipeline_cycle_count
		, inst->instruction_stage_timing[EX_S].pipeline_start_cycle
		, inst->instruction_stage_timing[EX_S].pipeline_cycle_count
		, inst->instruction_stage_timing[WB_S].pipeline_start_cycle
		, inst->instruction_stage_timing[WB_S].pipeline_cycle_count
		, inst->instruction_stage_timing[RT_S].pipeline_start_cycle
		, inst->instruction_stage_timing[RT_S].pipeline_cycle_count
		);
}

// microprocessor : invalidate rmt entry
void MicroProcessor :: invalidate_rmt(int dest_arf_index, int dest_rob_tag)
{
	if(dest_arf_index>=0){
		if(this->RMT[dest_arf_index].valid)
		{
			if(this->RMT[dest_arf_index].rob_tag == dest_rob_tag)
			this->RMT[dest_arf_index].valid = false;
		}
	}
}

// microprocessor : calculate cycle time for all stages
void MicroProcessor	:: calculate_cycle_time(Instruction* popped_instruction)
{
	popped_instruction->instruction_stage_timing[RT_S].pipeline_cycle_count = simulator_cycle - popped_instruction->instruction_stage_timing[RT_S].pipeline_start_cycle + 1;
	popped_instruction->instruction_stage_timing[WB_S].pipeline_cycle_count = popped_instruction->instruction_stage_timing[RT_S].pipeline_start_cycle - popped_instruction->instruction_stage_timing[WB_S].pipeline_start_cycle;
	popped_instruction->instruction_stage_timing[EX_S].pipeline_cycle_count = popped_instruction->instruction_stage_timing[WB_S].pipeline_start_cycle - popped_instruction->instruction_stage_timing[EX_S].pipeline_start_cycle;
	popped_instruction->instruction_stage_timing[IS_S].pipeline_cycle_count = popped_instruction->instruction_stage_timing[EX_S].pipeline_start_cycle - popped_instruction->instruction_stage_timing[IS_S].pipeline_start_cycle;
	popped_instruction->instruction_stage_timing[DI_S].pipeline_cycle_count = popped_instruction->instruction_stage_timing[IS_S].pipeline_start_cycle - popped_instruction->instruction_stage_timing[DI_S].pipeline_start_cycle;
	popped_instruction->instruction_stage_timing[RR_S].pipeline_cycle_count = popped_instruction->instruction_stage_timing[DI_S].pipeline_start_cycle - popped_instruction->instruction_stage_timing[RR_S].pipeline_start_cycle;
	popped_instruction->instruction_stage_timing[RN_S].pipeline_cycle_count = popped_instruction->instruction_stage_timing[RR_S].pipeline_start_cycle - popped_instruction->instruction_stage_timing[RN_S].pipeline_start_cycle;
	popped_instruction->instruction_stage_timing[DE_S].pipeline_cycle_count = popped_instruction->instruction_stage_timing[RN_S].pipeline_start_cycle - popped_instruction->instruction_stage_timing[DE_S].pipeline_start_cycle;
	popped_instruction->instruction_stage_timing[FE_S].pipeline_cycle_count = popped_instruction->instruction_stage_timing[DE_S].pipeline_start_cycle - popped_instruction->instruction_stage_timing[FE_S].pipeline_start_cycle;
}

// microrprocessor : remove instruction from rob
Instruction* MicroProcessor	:: dequeue_rob()
{
	Instruction* inst;

	// rob empty
	if(this->head_pointer==-1 && tail_pointer==-1)
	{
		inst =  NULL;
	}
	// rob contains only one entry
	else if(this->head_pointer == this->tail_pointer)
	{
		inst = this->ROB[this->head_pointer].rob_instruction;
		this->head_pointer = -1;
		this->tail_pointer = -1;
	}
	else
	{
		inst = this->ROB[this->head_pointer].rob_instruction;
		this->head_pointer = (this->head_pointer + 1) % this->params.rob_size;
	}
	return inst;		
}

//------------------------------
// microprocessor :: Writeback
//------------------------------
void MicroProcessor :: Writeback()
{
	/*
	Process the writeback bundle in WB:
		- For each instruction in WB, mark the instruction as “ready” in its entry in the ROB
	*/
	for(int i=0 ;i<this->params.width*5; i++)
	{
		if(this->WB[i].valid)
		{	
			
			// copy the instruction(destination) rob tag
			int destination_rob_tag = this->WB[i].pipe_instruction->dest_register.rob_tag;

			// mark the entry as ready in ROB
			this->ROB[destination_rob_tag].rdy = true;

			// update the start cycle of ready entry of ROB for Retire stage
			this->ROB[destination_rob_tag].rob_instruction->instruction_stage_timing[RT_S].pipeline_start_cycle = simulator_cycle + 1;

			// invalidate the entry in Writeback Register
			this->WB[i].valid=false;
		}
	}
}

// microprocessor : display ROB
void MicroProcessor	:: debug_ROB()
{
	std::cout << "\n######## ROB ########";
	for(int i=0 ;i< this->params.rob_size; i++){
		std::cout << "\n ROB : " << i  << " rdy : " << this->ROB[i].rdy;
		if(this->ROB[i].rob_instruction	!= NULL){
			this->ROB[i].rob_instruction->debug_instruction();
		}
		else
		   std::cout << "\n NULL";
	}
	std::cout << "\n###########################";
}
//------------------------------
// microprocessor :: Execute
//------------------------------

void MicroProcessor :: Execute()
{
	
	/*

	From the execute_list, check for instructions that are finishing execution this cycle :
	 1) Remove the instruction from the execute_list.
	 2) Add the instruction to WB.
	 3) Wakeup dependent instructions (set their source operand ready flags)
		- IQ 
		- DI (the dispatch bundle)
		- RR (the register-read bundle) 

	note: there is no need to check if there will be empty slots in WB because
	every cycle instructions will retire from WB. And in the code WB stage execution
	preempts Execute for the same clock cycle.

	*/

	this->update_ex_cycle();
	this->copy_EX_List_to_WB();

}

// microprocessor : copy completed instruction from ex list to wb
void MicroProcessor :: copy_EX_List_to_WB()
{
	for(int i=0; i<this->params.width*5; i++)
	{	
		if(this->execute_list[i].valid){
			// instruction finishes execution this cycle
			if(this->execute_list[i].pipe_instruction->total_execute_cycles == 0)
			{	
				// dest register exists
				if(this->execute_list[i].pipe_instruction->dest_register.arf_index >= 0)
				{
					// wakeup dependent instructions
					int dest_rob_tag = this->execute_list[i].pipe_instruction->dest_register.rob_tag;
					this->IQ_wakeup(dest_rob_tag);
					this->DI_wakeup(dest_rob_tag);
					this->RR_wakeup(dest_rob_tag);
				}

				// copy EX List to WB bundle
			
				int wb_empty_index = this->WB_bundle_empty_index();

				// copy instruction to WB bundle 
				this->WB[wb_empty_index].pipe_instruction = this->execute_list[i].pipe_instruction;
				// set the start cycle for WB stage in the WB register
				this->WB[wb_empty_index].pipe_instruction->instruction_stage_timing[WB_S].pipeline_start_cycle = simulator_cycle + 1;
				// update valid tag of the WB index
				this->WB[wb_empty_index].valid = true;
				// invalidate the instruction in execute list
				this->execute_list[i].valid = false;
			}
		}
	}
}

// microprocessor : update execute cycles of instructions in execute list
void MicroProcessor :: update_ex_cycle()
{
	for(int i=0; i<this->params.width*5; i++)
	{
		if(this->execute_list[i].valid)
			this->execute_list[i].pipe_instruction->total_execute_cycles--;
	}
}

// microprocessor : wake up dependent inst in IQ
void MicroProcessor :: IQ_wakeup(int dest_rob_tag)
{
	for(int i=0; i<this->params.iq_size; i++)
	{
		if(this->Issue_Queue[i].valid)
		{
			
			// source one wakeup

			// source one has a valid rob tag
			if(this->Issue_Queue[i].queue_instruction->src_register_one.rob_valid)
			{
				// wake up when rob tags match
				if(this->Issue_Queue[i].queue_instruction->src_register_one.rob_tag == dest_rob_tag)
				{		
					
					this->Issue_Queue[i].queue_instruction->src_register_one.rdy = true;
				}
			}

			// source two wakeup
			if(this->Issue_Queue[i].queue_instruction->src_register_two.rob_valid)
			{
				// wake up when rob tags match
				if(this->Issue_Queue[i].queue_instruction->src_register_two.rob_tag == dest_rob_tag)
				{	
					
					
					this->Issue_Queue[i].queue_instruction->src_register_two.rdy = true;
				}
			}

		}
	}
}

// microprocessor : wake up dependent inst in DI
void MicroProcessor :: DI_wakeup(int dest_rob_tag)
{
	for(int i=0; i<this->DI->bundle_length; i++)
	{
		// source one wake up

		// src one has valid rob tag
		if(this->DI->instruction_bundle[i]->src_register_one.rob_valid)
		{
			// wake up when rob tag match
			if(this->DI->instruction_bundle[i]->src_register_one.rob_tag == dest_rob_tag)
			{
				this->DI->instruction_bundle[i]->src_register_one.rdy = true;
			}
		}


		// source two wake up 

		// src two has valid rob tag
		if(this->DI->instruction_bundle[i]->src_register_two.rob_valid)
		{
			// wake up when rob tag match
			if(this->DI->instruction_bundle[i]->src_register_two.rob_tag == dest_rob_tag)
			{
				this->DI->instruction_bundle[i]->src_register_two.rdy = true;
			}
		}
	}
}

// microprocessor : wake up dependent inst in RR
void MicroProcessor :: RR_wakeup(int dest_rob_tag)
{
	for(int i=0; i<this->RR->bundle_length; i++)
	{
		// source one wake up

		// src one has valid rob tag
		if(this->RR->instruction_bundle[i]->src_register_one.rob_valid)
		{
			// wake up when rob tag match
			if(this->RR->instruction_bundle[i]->src_register_one.rob_tag == dest_rob_tag)
			{
				this->RR->instruction_bundle[i]->src_register_one.rdy = true;
			}
		}


		// source two wake up 

		// src two has valid rob tag
		if(this->RR->instruction_bundle[i]->src_register_two.rob_valid)
		{
			// wake up when rob tag match

			if(this->RR->instruction_bundle[i]->src_register_two.rob_tag == dest_rob_tag)
			{
				this->RR->instruction_bundle[i]->src_register_two.rdy = true;
			}
		}
	}
}

// microprocessor : empty WB index
int MicroProcessor :: WB_bundle_empty_index()
{
	int index = -1;
	for(int i=0; i<this->params.width*5; i++)
	{
		if(!this->WB[i].valid)
		{
			index = i;
			break;
		}
	}

	return index;

}

// microprocessor : display WB bundle
void MicroProcessor :: debug_WB()
{
	std::cout << "\n######## WB Bundle ########";
	for(int i=0 ;i< this->params.width*5; i++){
		std::cout << "\n WB : " << i << " valid : " << this->WB[i].valid;
		if(this->WB[i].valid){
			this->WB[i].pipe_instruction->debug_instruction();
		}
	}
	std::cout << "\n###########################";
}


//------------------------------
// microprocessor :: Issue
//------------------------------

void MicroProcessor :: Issue()
{

	/*
		Issue up to WIDTH oldest ready instructions from the IQ. 
		1) Remove the instruction from the IQ.
		2) Add the instruction to the execute_list. 
		3) Set a timer for the instruction in the execute_list that will allow you to model its execution latency.
	*/

	this->copy_IQ_to_EX_List();

	
}

// microprocessor :  copy iq instruction to ex list
void MicroProcessor :: copy_IQ_to_EX_List()
{
	int iq_oldest_ready_index, ex_empty_index;
	int width_count = 0;
	while(width_count < this->params.width)
	{
		iq_oldest_ready_index = this->iq_oldest_ready_instruction();
		ex_empty_index = this->ex_list_empty_index();

		// no oldest ready instruction or no space in ex list
		if(iq_oldest_ready_index == -1 || ex_empty_index == -1)
			break;
		// oldest ready instruction and empty ex list
		else
		{
			// copy instruction to ex list
			this->execute_list[ex_empty_index].pipe_instruction = this->Issue_Queue[iq_oldest_ready_index].queue_instruction;
			// update valid tag for instruction copied to execute list
			this->execute_list[ex_empty_index].valid = true;
			// update start cycle for instruction in ex_list
			this->execute_list[ex_empty_index].pipe_instruction->instruction_stage_timing[EX_S].pipeline_start_cycle = simulator_cycle + 1;
			// invalidate the issue queue entry
			this->Issue_Queue[iq_oldest_ready_index].valid = false;

		}	

		width_count++;
	}
}

// microprocessor : return index of oldest ready inst in iq
int MicroProcessor :: iq_oldest_ready_instruction()
{
	int iq_index = -1;
	int global_index = -1;

	for(int i=0; i<this->params.iq_size; i++)
	{	
		// valid iq entry
		if(this->Issue_Queue[i].valid)
		{
			// ready instruction 
			if(this->Issue_Queue[i].queue_instruction->src_register_one.rdy &&
			   this->Issue_Queue[i].queue_instruction->src_register_two.rdy)
			{
				// check for oldest ready instruction with no entry
				if(global_index == -1)
				{
					global_index = this->Issue_Queue[i].queue_instruction->global_index;
					iq_index = i;
				}
				// compare instruction in multiple passes
				else
				{
					if(global_index > this->Issue_Queue[i].queue_instruction->global_index)
					{
						global_index = this->Issue_Queue[i].queue_instruction->global_index;
						iq_index = i;
					}
				}
			}
		}
	}
	return iq_index;
}

// microprocessor : return empty index of ex list
int MicroProcessor :: ex_list_empty_index()
{
	int ex_list_index = -1;

	for(int i=0; i<this->params.width*5; i++)
	{
		if(!this->execute_list[i].valid)
		{
			ex_list_index = i;
			break;
		}
	}
	return ex_list_index;
}

// microprocessor : display ex list
void MicroProcessor :: debug_execute_list()
{
	std::cout << "\n######## EXECUTE LIST ########";
	for(int i=0 ;i< this->params.width*5; i++){
		std::cout << "\n execute_list : " << i << " valid : " << this->execute_list[i].valid;
		if(this->execute_list[i].valid){
			this->execute_list[i].pipe_instruction->debug_instruction();
			//std::cout << "\n Execute Cycles Left : " << this->execute_list[i].pipe_instruction->total_execute_cycles;
		}
	}
	std::cout << "\n###########################";
}

//------------------------------
// microprocessor :: Dispatch
//------------------------------
void MicroProcessor :: Dispatch()
{
	/*

	(1)If DI contains a dispatch bundle &&
	(2)If the number of free IQ entries is less than the size of the dispatch bundle DI 
		 - do nothing. 
	(2)If the number of free IQ entries is greater than or equal to the size of the dispatch bundle in DI,
		- dispatch all instructions from DI to the IQ

	*/

	if(!this->DI->bundle_empty &&
	   this->DI->bundle_length <= this->iq_empty_spaces())
	{
		this->copy_DI_to_IQ();
	}

}

// microprocessor : copy DI bundle to Issue Queue
void MicroProcessor :: copy_DI_to_IQ()
{
	int issue_queue_iterate, dispatch_bundle_iterate;
	
	issue_queue_iterate = 0;
	dispatch_bundle_iterate = 0;

	while(dispatch_bundle_iterate <= this->DI->bundle_length-1){

		if(!this->Issue_Queue[issue_queue_iterate].valid){
			// make the Issue Queue entry valid
			this->Issue_Queue[issue_queue_iterate].valid = true;
			// copy the instruction to the Issue Queue
			this->Issue_Queue[issue_queue_iterate].queue_instruction = this->DI->instruction_bundle[dispatch_bundle_iterate];
			// update the start cycle of the instruction in the Issue Queue : Issue Stage
			this->Issue_Queue[issue_queue_iterate].queue_instruction->instruction_stage_timing[IS_S].pipeline_start_cycle = simulator_cycle + 1;
			dispatch_bundle_iterate++; // update DI counter when data copied
		}
		issue_queue_iterate++; // update Issue_Queue counter in each iteration
	}
	// update bundle length for DI register
	this->DI->bundle_length = 0;
	// update bundle empty flag for DI register
	this->DI->bundle_empty = true;

}

// microprocessor : count empty slots in issue queue
int MicroProcessor :: iq_empty_spaces()
{
	int count = 0;
	for(int i=0; i<this->params.iq_size; i++)
	{
		if(!this->Issue_Queue[i].valid)
			count++;
	}
	return count;
}

// microprocessor : display issue queue
void MicroProcessor :: debug_IQ()
{
	std::cout << "\n######## ISSUE QUEUE ########";
	for(int i=0 ;i< this->params.iq_size; i++){
		std::cout << "\n IQ : " << i << " valid : " << this->Issue_Queue[i].valid;
		if(this->Issue_Queue[i].valid)
			this->Issue_Queue[i].queue_instruction->debug_instruction();
	}
	std::cout << "\n###########################";
}

//------------------------------
// microprocessor :: RegRead
//------------------------------
void MicroProcessor :: RegRead()
{
	/*
	(1) If RR contains a register-read bundle AND
	(2) If DI is not empty (cannot accept a new dispatch bundle)
		- do nothing.

	(2) If DI is empty (can accept a new dispatch bundle) 
		- process (see below) the register-read bundle 
			- ascertain the readyness of the renamed source operands
		- advance it from RR to DI.
	*/

	if(!this->RR->bundle_empty && this->DI->bundle_empty)
	{
		this->process_reg_read_bundle();
		this->copy_RR_to_DI();
	}

}

// microprocessor : ready asscertain source regs
void MicroProcessor :: process_reg_read_bundle()
{
	for(int i=0 ;i<this->RR->bundle_length;i++)
	{
		// ascertain readyness of src 1

		// source register one value taken from arf
		if(this->RR->instruction_bundle[i]->src_register_one.arf_valid)
		{
			this->RR->instruction_bundle[i]->src_register_one.rdy = true;
		}
		// source register one value taken from rob
		else if(this->RR->instruction_bundle[i]->src_register_one.rob_valid)
		{
			// change readyness if not prev woken up
			if(!this->RR->instruction_bundle[i]->src_register_one.rdy)
				this->RR->instruction_bundle[i]->src_register_one.rdy = this->ROB[this->RR->instruction_bundle[i]->src_register_one.rob_tag].rdy;
		}
		// no source register one
		else
		{
			this->RR->instruction_bundle[i]->src_register_one.rdy = true; 	
		}


		// ascertain readyness of src 2 

		// source register two value taken from arf
		if(this->RR->instruction_bundle[i]->src_register_two.arf_valid)
		{
			this->RR->instruction_bundle[i]->src_register_two.rdy = true;
		}
		// source register two value taken from rob
		else if(this->RR->instruction_bundle[i]->src_register_two.rob_valid)
		{
			// change readyness if not previously woken up
			if(!this->RR->instruction_bundle[i]->src_register_two.rdy)
				this->RR->instruction_bundle[i]->src_register_two.rdy = this->ROB[this->RR->instruction_bundle[i]->src_register_two.rob_tag].rdy;
		}
		// no source register two
		else
		{
			this->RR->instruction_bundle[i]->src_register_two.rdy = true; 	
		}

	}
}

// microprocessor : copy RR bundle to DI
void MicroProcessor :: copy_RR_to_DI()
{
	for(int i=0 ;i<this->RR->bundle_length; i++){
		// copy instruction pointer from RR to DI bundle
		this->DI->instruction_bundle[i] = this->RR->instruction_bundle[i];
		// update start cycle of instruction in DI bundle : Dispatch Stage
		this->DI->instruction_bundle[i]->instruction_stage_timing[DI_S].pipeline_start_cycle = simulator_cycle + 1;
		// update bundle length for DI
		this->DI->bundle_length++;
			
	}
	// update DI bundle empty flag
	if(this->DI->bundle_length > 0)
		this->DI->bundle_empty = false;
	// update bundle length for RR
	this->RR->bundle_length = 0;
	// updat RR bundle empty flag
	this->RR->bundle_empty = true;
}

// microprocessor : display DI bundle
void MicroProcessor :: debug_DI_bundle()
{
	std::cout << "\n######## DI BUNDLE ########";
	for(int i=0 ;i< this->DI->bundle_length; i++){
		this->DI->instruction_bundle[i]->debug_instruction();
	}
	std::cout << "\n###########################";
}

//------------------------------
// microprocessor : Rename
//------------------------------
void MicroProcessor :: Rename()
{
	/*
	(1)If RN contains a rename bundle:
		(1)If either RR is not empty (cannot accept a new register-read bundle) or 
		(2) the ROB does not have enough free entries to accept the entire rename bundle
			- do nothing.

		(1)If RR is empty (can accept a new register-read bundle) and 
		(2) the ROB has enough free entries to accept the entire rename bundle 
					- process the rename bundle
					(1) allocate an entry in the ROB for the instruction 
					(2) rename its source registers 
					(3) rename its destination register (if it has one). Note that the rename bundle must be renamed in program
						order (fortunately the instructions in the rename bundle are in program order).
					- advance it from RN to RR.
	*/

	
	if(!this->RN->bundle_empty &&
	    this->RR->bundle_empty &&
	    this->RN->bundle_length <= this->rob_empty_spaces())
	{

		this->process_rename_bundle();
		this->copy_RN_to_RR();
	}

}

// microprocessor : empty spaces in ROB
int MicroProcessor :: rob_empty_spaces()
{
	int empty = 0;
	int occupied = 0;
	if(this->head_pointer == this->tail_pointer)
	{
		if(this->head_pointer == -1)
		{
			empty = this->params.rob_size;
			occupied = 0;
		}
		else
		{
			empty = this->params.rob_size - 1;
			occupied = 1;
		}
	}
	else if(this->head_pointer < this->tail_pointer)
	{
		occupied = (this->tail_pointer - this->head_pointer) + 1;
		empty = this->params.rob_size - occupied;
	}
	else
	{
			empty = (this->head_pointer - this->tail_pointer) - 1;
			occupied = this->params.rob_size - empty;
		
	}

	return empty;

}

// microprocessor : circular queue enqueue
void MicroProcessor :: enqueue_rob(Instruction* inst)
{
	if(this->head_pointer == -1 && this->tail_pointer == -1){
		this->head_pointer = 0;
		this->tail_pointer = 0;
		this->ROB[this->tail_pointer].rob_instruction = inst;
	}
	else if(this->head_pointer == (this->tail_pointer + 1)%this->params.rob_size)
	{
		/*
			this case wont be true because the call to enqueue in 
			register rename stage is done after checking with
			method rob_empty_spaces
		*/

		std::cout << "\n rob full";
	}
	else
	{
		this->tail_pointer = (this->tail_pointer + 1)%this->params.rob_size;
		this->ROB[this->tail_pointer].rob_instruction = inst;
	}
}


// microprocesor : rename source and destination registers
void MicroProcessor :: process_rename_bundle()
{

	for(int i=0; i<this->RN->bundle_length; i++)
	{
		// allocate an entry in the rob for the instruction

		// enqueue instruction
		this->enqueue_rob(this->RN->instruction_bundle[i]);
		// allocate the rob tag value to instruction rob tag variable
		this->RN->instruction_bundle[i]->instruction_rob_tag = this->tail_pointer;
		// make the tail pointer rdy as false
		this->ROB[this->tail_pointer].rdy = false;

		// rename source register one

		int src_one_arf_index = this->RN->instruction_bundle[i]->src_register_one.arf_index;
		// no source register one
		if(src_one_arf_index < 0){     
			// arf_valid and rob_valid will remain false
		}
		else{
			// source register one from rob
			if(this->RMT[src_one_arf_index].valid){ 
				// update rob tag
				this->RN->instruction_bundle[i]->src_register_one.rob_tag = this->RMT[src_one_arf_index].rob_tag;
				// update rob valid
				this->RN->instruction_bundle[i]->src_register_one.rob_valid = true;
				// arf valid will remain false
				
					
			}

			else{	// source register from arf
				this->RN->instruction_bundle[i]->src_register_one.arf_valid = true;
			}
				
		}

			
		// rename source register two

		int src_two_arf_index = this->RN->instruction_bundle[i]->src_register_two.arf_index;
		// no source register two
		if(src_two_arf_index < 0){   
			// arf_valid and rob_valid will remain false
		}
		else{
			// source register two  from rob
			if(this->RMT[src_two_arf_index].valid){ 
				// update rob tag and rob valid
				this->RN->instruction_bundle[i]->src_register_two.rob_tag = this->RMT[src_two_arf_index].rob_tag;
				this->RN->instruction_bundle[i]->src_register_two.rob_valid = true;
				// arf valid will remain false
				
					
			}
			// source register from arf
			else{	
				this->RN->instruction_bundle[i]->src_register_two.arf_valid = true;
				}

				
		}


		// rename destination register

		this->RN->instruction_bundle[i]->dest_register.rob_tag = this->tail_pointer;
		// update the rmt entry for the destination register arf index
		int dest_arf_index = this->RN->instruction_bundle[i]->dest_register.arf_index;
		if( dest_arf_index >= 0){
			this->RMT[dest_arf_index].valid = true;
			this->RMT[dest_arf_index].rob_tag = this->tail_pointer;
		}

	}
}

// microprocessor : copy RN bundle to RR 
void MicroProcessor :: copy_RN_to_RR()
{
	for(int i=0 ;i<this->RN->bundle_length; i++){
		// copy instruction pointer from RN to RR
		this->RR->instruction_bundle[i] = this->RN->instruction_bundle[i];
		// update start cycle timing of instruction in RR bundle : register read stage
		this->RR->instruction_bundle[i]->instruction_stage_timing[RR_S].pipeline_start_cycle = simulator_cycle + 1;
		// update the bundle length of RR bundle
		this->RR->bundle_length++;
	}

	// update empty flag of RR bundle
	if(this->RR->bundle_length > 0)
		this->RR->bundle_empty = false;
	// update bundle length of RN bundle
	this->RN->bundle_length = 0;
	// update empty flag of RN bundle
	this->RN->bundle_empty = true;
}

// microprocessor : display RR bundle
void MicroProcessor :: debug_RR_bundle()
{
	std::cout << "\n######## RR BUNDLE ########";
	for(int i=0 ;i< this->RR->bundle_length; i++){
		this->RR->instruction_bundle[i]->debug_instruction();
	}
	std::cout << "\n###########################";
}


//------------------------------
// microprocessor : Decode
//------------------------------

void MicroProcessor :: Decode()
{
	
	/*
	(1)If DE contains a decode bundle:
		(2)If RN is not empty (cannot accept a new rename bundle)
				- do nothing.
		(3)If RN is empty (can accept a new rename bundle)
				- advance the decode bundle from DE to RN.
	*/
	this->copy_DE_to_RN();

}

// microprocessor : display_RN_bundle
void MicroProcessor :: debug_RN_bundle()
{
	std::cout << "\n######## RN BUNDLE ########";
	for(int i=0 ;i< this->RN->bundle_length; i++){
		this->RN->instruction_bundle[i]->debug_instruction();
	}
	std::cout << "\n###########################";
}

// muicroprocessor : copy DE bundle to RN bundle
void MicroProcessor :: copy_DE_to_RN()
{	
	// DE non empty and RN empty
	if(!this->DE->bundle_empty && this->RN->bundle_empty){ 			
		for(int i=0 ;i<this->DE->bundle_length; i++){
			// copy instruction pointer from DE to RN bundle
			this->RN->instruction_bundle[i] = this->DE->instruction_bundle[i];
			// update start cycle timing info instruction in RN bundle : Rename Stage
			this->RN->instruction_bundle[i]->instruction_stage_timing[RN_S].pipeline_start_cycle = simulator_cycle + 1;
			// update bundle length for RN
			this->RN->bundle_length++;
			
		}
		// update RN bundle empty flag
		if(this->RN->bundle_length > 0)
			this->RN->bundle_empty = false;
		// update bundle length for DE
		this->DE->bundle_length = 0;
		// updat DE bundle empty flag
		this->DE->bundle_empty = true;
	}
}


//------------------------------
// microprocessor : fetch
//------------------------------
void MicroProcessor :: Fetch(FILE* FP)
{
	
	/*Do nothing if either 
		(1) there are no more instructions in the trace file or	
		(2) DE is not empty (cannot accept a newdecode bundle).

	(1) If there are more instructions in the trace file 
	AND
	(2) if DE is empty (can accept a new decode bundle), 
	   - then fetch up to WIDTH instructions from the trace file into DE. 
	   - Fewer than WIDTH instructions will be fetched only if the trace file has fewer than WIDTH instructions left.
	*/ 

	this->copy_FE_to_DE(FP);
	

}
// microprocessor : copy FE bundle to DE bundle
void MicroProcessor :: copy_FE_to_DE(FILE* FP)
{
	int   op,src1, src2, dest;
	unsigned long int pc;
	// DE bundle empty
	if(this->DE->bundle_empty){											
		for(int i=0; i < this->params.width; i++){
			
			// read instruction from trace
			int read_output = fscanf(FP, "%lx %d %d %d %d", &pc, &op, &dest, &src1, &src2);
			// trace file not empty
			if(read_output != EOF){ 										
				// fetch new instruction
				Instruction* inst = new Instruction(pc,op,src1,src2,dest);
				// update the instruction global index
				inst->global_index = instruction_global_index;
				instruction_global_index++;
				// update the start cycle timing info for the instruction : Fetch Stage
				inst->instruction_stage_timing[FE_S].pipeline_start_cycle = simulator_cycle;
				// store the instruction in DE bundle
				this->DE->instruction_bundle[i] = inst;
				// update DE bundle length
				this->DE->bundle_length++;
				// update the start cycle timing info for the instrution in decode bundle  : Decode Stage
				this->DE->instruction_bundle[i]->instruction_stage_timing[DE_S].pipeline_start_cycle = simulator_cycle + 1;
			}
			// trace file empty
			else			
			{											
				break;
			}

		}
		if(this->DE->bundle_length >0){										// DE bundle not empty
			this->DE->bundle_empty = false;
		}	
	}
}

// microprocessor : display_DE_bundle
void MicroProcessor :: debug_DE_bundle()
{
	std::cout << "\n######## DE BUNDLE ########";
	for(int i=0 ;i< this->DE->bundle_length; i++){
		this->DE->instruction_bundle[i]->debug_instruction();
	}
	std::cout << "\n###########################";
}

bool MicroProcessor	:: pipeline_empty()
{
	// empty DE bundle
	bool DE_empty = (this->DE->bundle_length == 0) ? true : false;
	// empty RN bundle
	bool RN_empty = (this->RN->bundle_length == 0) ? true : false;
	// empty RR bundle
	bool RR_empty = (this->RR->bundle_length == 0) ? true : false;
	// empty DI bundle
	bool DI_empty = (this->DI->bundle_length == 0) ? true : false;


	// empty Issue queue
	int iq_empty_count = 0;
	for(int i=0;i <this->params.iq_size; i++)
	{
		if(!this->Issue_Queue[i].valid)
			iq_empty_count++;
	}
	bool iq_empty = (this->params.iq_size == iq_empty_count) ? true : false;

	// empty execute list
	int ex_empty_count = 0;
	for(int i=0;i <this->params.width*5; i++)
	{
		if(!this->execute_list[i].valid)
			ex_empty_count++;
	}
	bool ex_empty = (this->params.width*5 == ex_empty_count) ? true : false;

	// WB bundle empty
	int wb_empty_count = 0;
	for(int i=0;i <this->params.width*5; i++)
	{
		if(!this->WB[i].valid)
			wb_empty_count++;
	}
	bool wb_empty = (this->params.width*5 == wb_empty_count) ? true : false;

	// ROB empty
	bool rob_empty = (this->head_pointer==-1 && this->tail_pointer) ? true : false;


	return (rob_empty & wb_empty & ex_empty & iq_empty & DI_empty & RR_empty & RN_empty	& DE_empty);
}






int main (int argc, char* argv[])
{
    FILE *FP;               // File handler
    char *trace_file;       // Variable that holds trace file name;
    proc_params params;     

    
    if (argc != 5)
    {
        printf("Error: Wrong number of inputs:%d\n", argc-1);
        exit(EXIT_FAILURE);
    }
    
    params.rob_size     = strtoul(argv[1], NULL, 10);
    params.iq_size      = strtoul(argv[2], NULL, 10);
    params.width        = strtoul(argv[3], NULL, 10);
    trace_file          = argv[4];

    // Open trace_file in read mode
    FP = fopen(trace_file, "r");
    if(FP == NULL)
    {
        // Throw error and exit if fopen() failed
        printf("Error: Unable to open file %s\n", trace_file);
        exit(EXIT_FAILURE);
    }
    
    
    
    MicroProcessor* up = new MicroProcessor(params.width, params.iq_size, params.rob_size);
    do
    {	//std::cout << "\n$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ SIM CYCLE : " << simulator_cycle << " $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$"; 
    	up->Retire();
    	up->Writeback();
    	//up->debug_ROB();
    	up->Execute();
    	//up->debug_WB();
    	up->Issue();
    	//up->debug_execute_list();
    	up->Dispatch();
    	//up->debug_IQ();
    	up->RegRead();
    	//up->debug_DI_bundle();
    	up->Rename();
    	//up->debug_RR_bundle();
    	up->Decode();
    	//up->debug_RN_bundle();
    	up->Fetch(FP);
    	//up->debug_DE_bundle();
    	//std::cout << "\n$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$";
    	
    }while(Advance_Cycle(up->pipeline_empty()));
   	

   	// simulator result
   	std::cout << "\n# === Simulator Command =========";
   	printf("\n# ./ sim %d %d %d %s", params.rob_size, params.iq_size, params.width, trace_file);
   	std::cout << "\n# === Processor Configuration ===";
	std::cout << "\n# ROB_SIZE 	= " << params.rob_size;
	std::cout << "\n# IQ_SIZE  	= " << params.iq_size;
	std::cout << "\n# WIDTH    	= " << params.width;
	std::cout << "\n# === Simulation Results ========";
	std::cout << "\n# Dynamic Instruction Count      = " << instruction_global_index;
	std::cout << "\n# Cycles                         = " << simulator_cycle; 
	printf("\n# Instructions Per Cycle (IPC)   = %.2f", (float)instruction_global_index/simulator_cycle); 

    return 0;
}
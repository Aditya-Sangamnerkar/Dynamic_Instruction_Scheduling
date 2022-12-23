typedef struct proc_params{
    int rob_size;
    int iq_size;
    int width;
}proc_params;


#define ARF_N 67

// simulator cycle
bool Advance_Cycle(bool trace_depleted, bool rob_empty);

//------------------------------------------------------------------------------
// Instruction
//------------------------------------------------------------------------------

// instruction : source register
struct SourceRegister
{
	int arf_index;
	bool arf_valid;

	int rob_tag;
	bool rob_valid;
	bool rdy;
};
// source register : debug
void debug_source_register(SourceRegister reg);


// instruction : destination register
struct DestinationRegister
{
	int arf_index;
	int rob_tag;
};
// destination register : debug
void debug_dest_register(DestinationRegister reg);

// instruction : Cycle Timing
struct TimingInfo
{
	int pipeline_cycle_count;
	int pipeline_start_cycle;
};

// instruction stage : enum
enum pipeline_stage {FE_S, DE_S, RN_S, RR_S, DI_S, IS_S, EX_S, WB_S, RT_S};

// timing info : debug
void debug_timing_info(TimingInfo stageTimingInfo);

// instruction : instruction
class Instruction
{
public:
	// instruction meta data
	unsigned long int pc;
	int opcode;

	// instruction global index
	int global_index;

	int instruction_rob_tag;
	bool instruction_rob_valid;

	// source register
	SourceRegister src_register_one, src_register_two;

	// destination register
	DestinationRegister dest_register;

	// Timing Information
	TimingInfo instruction_stage_timing[9];

	// opcode decoded execute cycles
	int total_execute_cycles;

	// constructor
	Instruction(unsigned long int pc, int opcode, int src_register_one_index, int src_register_two_index, int dest_register_index);

	// debug
	void debug_instruction();

};


//------------------------------------------------------------------------------
// Rename Map Table
//------------------------------------------------------------------------------

// Rename Map Table : Rename Map Table Entry
struct RenameMapTableEntry
{
	bool valid;
	int rob_tag;
	int arf_index;  
};


//------------------------------------------------------------------------------
// Reorder Buffer
//------------------------------------------------------------------------------

// Reorder Buffer : Reorder Buffer Entry
class ReorderBufferEntry
{
public:
	int rob_tag;
	bool rdy;
	Instruction* rob_instruction; // pointer to pointer of single instruction

	// constructor
	ReorderBufferEntry();

	// debug
	void debug_rob_entry(int rob_tag);

};

//------------------------------------------------------------------------------
// Pipeline Registers 
//------------------------------------------------------------------------------
class PipelineRegister
{
public:
	Instruction** instruction_bundle; // pointer to pointer of #WIDTH instruction
	bool bundle_empty;
	int bundle_length;

	PipelineRegister(int width);
};

//------------------------------------------------------------------------------
// Non Blocking Pipeline Register
//------------------------------------------------------------------------------
class NonBlockingPipelineRegister
{
public:
	Instruction* pipe_instruction;
	bool valid;

	NonBlockingPipelineRegister();
};

//------------------------------------------------------------------------------
// Issue Queue : Issue Queue Entry
//------------------------------------------------------------------------------

class IssueQueueEntry
{
public:
	bool valid;
	Instruction* queue_instruction; // pointer to pointer of a single instruction

	IssueQueueEntry();

};

//------------------------------------------------------------------------------
// Microprocessor
//------------------------------------------------------------------------------

class MicroProcessor
{
public:

	// Microprocessor Meta Data
	proc_params params;

	// Rename Map Table
	RenameMapTableEntry RMT[ARF_N];

	// Reorder Buffer
	ReorderBufferEntry* ROB; //input to RT stage from WB
	int head_pointer;
	int tail_pointer;

	// Issue Queue
	IssueQueueEntry* Issue_Queue; // input to IS stage from DI

	// Pipeline Registers
	PipelineRegister* DE; // input to DE stage from FE
	PipelineRegister* RN; // input to RN stage from DE
	PipelineRegister* RR; // input to RR stage from RN
	PipelineRegister* DI; // input to DI stage from RR
	NonBlockingPipelineRegister* execute_list; // input to EX stage from IS
	NonBlockingPipelineRegister* WB; // input to WB stage from EX


	// constructor
	MicroProcessor(int width, int iq_size, int rob_size);

	// fetch stage
	void Fetch(FILE* FP);
	void copy_FE_to_DE(FILE* FP);
	
	// decode stage
	void Decode();
	void copy_DE_to_RN();

	// rename stage
	void Rename();
	int rob_empty_spaces();
	void process_rename_bundle();
	void enqueue_rob(Instruction* inst);
	void copy_RN_to_RR();

	// reg read stage
	void RegRead();
	void process_reg_read_bundle();
	void copy_RR_to_DI();

	// dispatch
	void Dispatch();
	void copy_DI_to_IQ();
	int iq_empty_spaces();

	// issue
	void Issue();
	void copy_IQ_to_EX_List();
	int iq_oldest_ready_instruction();
	int ex_list_empty_index();

	// execute
	void Execute();
	void copy_EX_List_to_WB();
	void update_ex_cycle();
	void IQ_wakeup(int dest_rob_tag);
	void DI_wakeup(int dest_rob_tag);
	void RR_wakeup(int dest_rob_tag);
	int WB_bundle_empty_index();

	// writeback
	void Writeback();

	// retire
	void Retire();
	Instruction* dequeue_rob();
	void calculate_cycle_time(Instruction* popped_instruction);
	void invalidate_rmt(int arf_index, int rob_tag);
	void print_popped_instruction(Instruction* inst);

	
	// advance cycle
	bool pipeline_empty();



	// debug
	void debug_DE_bundle();
	void debug_RN_bundle();
	void debug_RR_bundle();
	void debug_DI_bundle();
	void debug_IQ();
	void debug_execute_list();
	void debug_WB();
	void debug_ROB();

		
};



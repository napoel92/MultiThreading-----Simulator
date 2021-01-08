/* 046267 Computer Architecture - Spring 2020 - HW #4 */

#include "core_api.h"
#include "sim_api.h"

#include <stdio.h>
#include <vector>
#include <cassert>

using std::vector;

const int NO_PC = -1;
const int IDLE = -2;
const int TOTAL_HALT = -1;

static int fineGrainedCycles = 0;
static int blockedCycles = 0;

static int blockedInstructions = 0;
static int fineGrainedInstructions = 0;

static vector<tcontext> RegisterFileBlocked;
static vector<tcontext> RegisterFileFineGrained;






struct SingleThread{

	int id;
	vector<Instruction> program;
	tcontext registersFile;
	int pc;
	int wait;

	explicit SingleThread(int id) : id(id),pc(NO_PC),wait(0) {

		for(int i = 0; i< REGS_COUNT; i++){
			registersFile.reg[i] = 0;
		}
		
		uint32_t line = -1;
		Instruction newInstruction;
		do{
			SIM_MemInstRead( ++line, &newInstruction, id);
			program.push_back(newInstruction);
			assert(newInstruction.opcode == program[line].opcode);
		} while (program[line].opcode != CMD_HALT);
	}

	cmd_opcode execute(){
		Instruction instruction =  program[(++pc)++];

		if ( instruction.opcode==CMD_SUBI ){
			registersFile.reg[instruction.dst_index] = 
			registersFile.reg[instruction.src1_index] - instruction.src2_index_imm;

		}else if( instruction.opcode==CMD_ADDI  ){
			registersFile.reg[instruction.dst_index] = 
			registersFile.reg[instruction.src1_index] + instruction.src2_index_imm;

		}else if( instruction.opcode==CMD_SUB ){
			registersFile.reg[instruction.dst_index] = 
			registersFile.reg[instruction.src1_index] - registersFile.reg[instruction.src2_index_imm];

		}else if( instruction.opcode==CMD_ADD ){
			registersFile.reg[instruction.dst_index] = 
			registersFile.reg[instruction.src1_index] + registersFile.reg[instruction.src2_index_imm];

		}else if( instruction.opcode==CMD_LOAD) {
			int32_t  source2 = (instruction.isSrc2Imm )?( instruction.src2_index_imm):(registersFile.reg[instruction.src2_index_imm]);
			uint32_t address = registersFile.reg[instruction.src1_index] + source2; 
			SIM_MemDataRead( address, &(registersFile.reg[instruction.dst_index]));
			assert( wait==0 );
			++wait = (SIM_GetLoadLat());

		}else if( instruction.opcode==CMD_STORE ){
			int32_t  source2 = (instruction.isSrc2Imm )?( instruction.src2_index_imm):(registersFile.reg[instruction.src2_index_imm]);
			uint32_t address = registersFile.reg[instruction.dst_index] + source2;
			SIM_MemDataWrite( address, registersFile.reg[instruction.dst_index]);
			assert( wait==0 );
			++wait = SIM_GetStoreLat();

		}else if( instruction.opcode==CMD_HALT ){
			--pc;
		
		}else assert( instruction.opcode==CMD_NOP );

		return instruction.opcode;
	}
};



struct Core {

	enum mtType{BLOCKED, FINE_GRAINED};

	mtType type;
	vector<SingleThread> threads;

	Core(mtType type) : type(type){
		for (int i = 0; i < SIM_GetThreadsNum(); i++){
			threads.push_back(SingleThread(i));
		}
	} 




	 int IncrementThread(int current){
		unsigned int countHalt=0, countWait=0, active=0;
		for(SingleThread t : threads) active = (t.wait && t.program[t.pc].opcode==CMD_HALT ) ? (active) : (active+1) ;
		bool isHalt = true;
		bool isWait = true;
		

		while( (countHalt!=threads.size())  &&  (countWait!=active)  &&  ((isHalt)||(isWait)) ){
			
			if(type==FINE_GRAINED) (++current)%(threads.size());
			SingleThread& thread = threads[current];
			isHalt = (thread.program[thread.pc].opcode==CMD_HALT);
			isWait = ( thread.wait > 0);
			
			assert( ((isHalt)&&(isWait))==false );
			if( (type==BLOCKED) && (isWait || isHalt ) )  (current+1)%(threads.size());
			
			countHalt += (int)isHalt;
			countWait += (int)isWait;
		}

	//fixme - not a sure assertion--------------------------------
	assert( (countHalt!=threads.size())  ||  (countWait!=active) );
	//fixme - not a sure assertion---------------------------------
	current = (countHalt==threads.size()) ? (TOTAL_HALT) : (current);
	current = (countWait==active) ? (IDLE) : (current);
	return current;
	}
};



struct waitFunctor{
		Core* core;
		waitFunctor(Core* core):core(core){}
		void operator()(int cycles=1){
			for(SingleThread i : core->threads) if( i.wait ) i.wait-=cycles;
		}
};
	



void CORE_BlockedMT() {

	Core core(Core::BLOCKED);
	RegisterFileBlocked.resize(SIM_GetThreadsNum());
	

	waitFunctor decrementWait(&core);
	int currentThread = 0;
	while ( currentThread!=TOTAL_HALT ){
	// performs another command every iteration 

		++blockedCycles;
		decrementWait();
		cmd_opcode command = core.threads[currentThread].execute();
		++blockedInstructions;

		//FIXME------------------------------------------------------------------------------------------------
		if( command==CMD_HALT ) RegisterFileBlocked[currentThread] = core.threads[currentThread].registersFile ;
		assert( &RegisterFileBlocked[currentThread] != &core.threads[currentThread].registersFile );
		//FIXME------------------------------------------------------------------------------------------------
		bool contextSwitchPoissible = (command==CMD_HALT || command==CMD_LOAD || command==CMD_STORE);
		
		if( contextSwitchPoissible ){
			int previous = currentThread;
			currentThread = core.IncrementThread(currentThread);

			while (currentThread==IDLE){
				++blockedCycles;
				decrementWait();
				currentThread = core.IncrementThread(previous);
			}


			if( (previous!=currentThread) && (currentThread!=TOTAL_HALT) ){
				int contextSwitchFine = SIM_GetSwitchCycles();
				blockedCycles += contextSwitchFine;
				decrementWait(contextSwitchFine);
			}
		}		
	}	
}



void CORE_FinegrainedMT() {
	Core core(Core::FINE_GRAINED);
	RegisterFileFineGrained.resize(SIM_GetThreadsNum());

	waitFunctor decrementWait(&core);
	int currentThread = 0;
	while ( currentThread!=TOTAL_HALT ){
		// performs another command every iteration 

		++fineGrainedCycles;
		decrementWait();
		cmd_opcode command = core.threads[currentThread].execute();
		++fineGrainedInstructions;
		
		//FIXME----------------------------------------------------------------------------------------------------
		if( command==CMD_HALT ) RegisterFileFineGrained[currentThread] = core.threads[currentThread].registersFile ;
		assert( &RegisterFileFineGrained[currentThread] != &core.threads[currentThread].registersFile );
		//FIXME----------------------------------------------------------------------------------------------------
		int previous = currentThread;
		currentThread = core.IncrementThread(currentThread);

		while (currentThread==IDLE){
				++fineGrainedCycles;
				decrementWait();
				currentThread = core.IncrementThread(previous);
		}
	}
}



double CORE_BlockedMT_CPI(){
	return (double)blockedCycles/blockedInstructions;
}


double CORE_FinegrainedMT_CPI(){
	return (double)fineGrainedCycles/fineGrainedInstructions;
}


void CORE_BlockedMT_CTX(tcontext* context, int threadid) {
	//fixme----------------------------------------------
	context[threadid] = RegisterFileBlocked[threadid];
	//fixme----------------------------------------------
}


void CORE_FinegrainedMT_CTX(tcontext* context, int threadid) {
	//fixme----------------------------------------------
	context[threadid] = RegisterFileFineGrained[threadid];
	//fixme----------------------------------------------

}

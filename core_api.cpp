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





/* an auxiliary struct for representing one 
   thread of some process in a curtain core  */
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


	// a method that simulates the execution unit of a processor 
	cmd_opcode execute(){
		Instruction instruction =  program[(++pc)];

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
			SIM_MemDataWrite( address, registersFile.reg[instruction.src1_index]);
			assert( wait==0 );
			++wait = SIM_GetStoreLat();
		}
		//else{
			//instruction is neigter NOP or HALT
		//}
		
		return instruction.opcode;
	}
};


/* an auxiliary struct for representing the core of 
   either a blocked or fineGrained processor  */
struct MultiThread {

	enum mtType{BLOCKED, FINE_GRAINED};
	mtType type;
	vector<SingleThread> threads;

	MultiThread(mtType type) : type(type){
		for (int i = 0; i < SIM_GetThreadsNum(); i++){
			threads.push_back(SingleThread(i));
		}
	} 

	// a method for finding the next thread to feed the core
	 int IncrementThread(int current){

		unsigned int countHalt=0, countWait=0, active=0;
		for( unsigned int i=0 ; i<threads.size() ; i++ ){
			if( -1 == threads[i].pc ){
				++active;
				continue;
			}
			if ( (threads[i]).program[threads[i].pc].opcode!=CMD_HALT ) ++active;	
		}
		
		bool isHalted = true;
		bool isWaiting = true;
		while( (countHalt!=threads.size())  &&  (countWait!=active)  &&  ((isHalted)||(isWaiting)) ){

			if(type==FINE_GRAINED) current = (current+1)%(threads.size());
			SingleThread& thread = threads[current];
			
			if( -1 != thread.pc ){
				isHalted = (thread.program[thread.pc].opcode==CMD_HALT);
			}else{ isHalted = false; }
			isWaiting = ( thread.wait > 0);
			
			assert( ((isHalted)&&(isWaiting))==false );
			if( (type==MultiThread::BLOCKED) && (isWaiting || isHalted ) )  current = (current+1)%(threads.size());
			
			countHalt += (int)isHalted;
			countWait += (int)isWaiting;
		}
	assert( (countHalt!=threads.size())  ||  (countWait!=active) );


	current = (countHalt==threads.size() || active==0) ? (TOTAL_HALT) : (current);
	current = (countWait==active && current!=TOTAL_HALT) ? (IDLE) : (current);
	return current;
	}
};


/* 		functionObject for decreasing the 
   		wait-factor of all busy-Threads			 */
//-------------------------------------------------------------------------------------------
struct waitFunctor{
	MultiThread* core;
	waitFunctor(MultiThread* core):core(core){}
	void operator()(int cycles=1){
		for( unsigned int i=0 ; i<core->threads.size() ; ++i ){
			if(core->threads[i].wait){
				core->threads[i].wait -= cycles;
				core->threads[i].wait = (core->threads[i].wait<0)?(0):(core->threads[i].wait);
			}
		}
	}
}; //-----------------------------------------------------------------------------------------
	





void CORE_BlockedMT() {

	MultiThread core(MultiThread::BLOCKED);
	RegisterFileBlocked.resize(SIM_GetThreadsNum());
	waitFunctor decrementWait(&core);
	
	int currentThread = 0;
	while ( currentThread!=TOTAL_HALT ){ // performs another command every iteration 

		++blockedCycles;
		decrementWait();
		cmd_opcode command = core.threads[currentThread].execute();
		++blockedInstructions;

		if( command==CMD_HALT ) RegisterFileBlocked[currentThread] = core.threads[currentThread].registersFile ;
		assert( &RegisterFileBlocked[currentThread] != &core.threads[currentThread].registersFile );
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
	MultiThread core(MultiThread::FINE_GRAINED);
	RegisterFileFineGrained.resize(SIM_GetThreadsNum());

	waitFunctor decrementWait(&core);
	int currentThread = 0;
	while ( currentThread!=TOTAL_HALT ){ // performs another command every iteration 

		++fineGrainedCycles;
		decrementWait();
		cmd_opcode command = core.threads[currentThread].execute();
		++fineGrainedInstructions;
		
		if( command==CMD_HALT ) RegisterFileFineGrained[currentThread] = core.threads[currentThread].registersFile ;
		assert( &RegisterFileFineGrained[currentThread] != &core.threads[currentThread].registersFile );
		
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
	context[threadid] = RegisterFileBlocked[threadid];
}


void CORE_FinegrainedMT_CTX(tcontext* context, int threadid) {
	context[threadid] = RegisterFileFineGrained[threadid];

}

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

static vector<tcontext> RegisterFileBlocked
static vector<tcontext> RegFileFineGrained;



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
		//todo
		return CMD_ADD;
		//FIXME
	}
};



struct Core {

	enum mtType{BLOCKED, FINE_GRAINED};

	mtType type;
	vector<SingleThread> threads;
	//vector<bool> isWait;

	Core(mtType type) : type(type){
		for (int i = 0; i < SIM_GetThreadsNum(); i++){
			threads.push_back(SingleThread(i));
			//isWait.push_back(false);
		}
	} 




	 int IncrementThread(int current){
		unsigned int countHalt=0, countWait=0, active=0;
		for(SingleThread i : threads) active = (i.wait) ? (active) : (active+1) ;
		bool isHalt = true;
		bool isWait = true;
		

		while( (countHalt!=threads.size())  &&  (countWait!=active)  &&  ((isHalt)||(isWait)) ){
			
			if(type==FINE_GRAINED) (++current)%(threads.size());
			SingleThread& thread = threads[current];
			isHalt = (thread.program[thread.pc].opcode==CMD_HALT);
			isWait = ( thread.wait > 0);
			assert( ((isHalt)&&(isWait))==false );
			
			countHalt = ( isHalt ) ? ( countHalt+1 ) : ( countHalt );
			countWait = ( isWait ) ? ( countWait+1 ) : ( countWait );
			if( (type==BLOCKED) && (isWait || isHalt ) )  (current+1)%(threads.size());

		}

	assert( (countHalt!=threads.size())  ||  (countWait!=active) );
	current = (countHalt==threads.size()) ? (TOTAL_HALT) : (current);
	current = (countWait==active) ? (IDLE) : (current);
	return current;
	}
};




void CORE_BlockedMT() {

	Core core(Core::BLOCKED);
	struct Functor{
		Core* core;
		RegisterFileBlocked.resize(SIM_GetThreadsNum());
		Functor(Core* core):core(core){}
		void operator()(int cycles){
			for(SingleThread i : core->threads) if( i.wait ) i.wait-=cycles;
		}
	};
	

	Functor decrementWait(&core);
	int currentThred = 0;
	while ( currentThred!=TOTAL_HALT ){
	// performs another command every iteration 

		++blockedCycles;
		decrementWait(1);
		cmd_opcode command = core.threads[currentThred].execute();
		if( command==CMD_HALT ) RegisterFileBlocked[currentThred] = core.threads[currentThred].registersFile ;
		
		if( command==CMD_HALT || command==CMD_LOAD || command==CMD_STORE ){
			int previous = currentThred;
			currentThred = core.IncrementThread(currentThred);

			while (currentThred==IDLE){
				++blockedCycles;
				decrementWait(1);
				currentThred = core.IncrementThread(previous);
			}


			if( (previous!=currentThred) && (currentThred!=TOTAL_HALT) ){
				int contextSwitchFine = SIM_GetSwitchCycles();
				blockedCycles += contextSwitchFine;
				decrementWait(contextSwitchFine);
			}
		}		
	}	
}

void CORE_FinegrainedMT() {
}

double CORE_BlockedMT_CPI(){
	return 0;
}

double CORE_FinegrainedMT_CPI(){
	return 0;
}

void CORE_BlockedMT_CTX(tcontext* context, int threadid) {
}

void CORE_FinegrainedMT_CTX(tcontext* context, int threadid) {
}

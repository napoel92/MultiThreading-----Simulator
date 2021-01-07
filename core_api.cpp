/* 046267 Computer Architecture - Spring 2020 - HW #4 */

#include "core_api.h"
#include "sim_api.h"

#include <stdio.h>
#include <vector>
#include <cassert>

using std::vector;



struct SingleThread{

	int id;
	vector<Instruction> program;
	tcontext registersFile;
	int pc;

	explicit SingleThread(int id) : id(id) {

		for(int i = 0; i< REGS_COUNT; i++){
			registersFile.reg[i] = 0;
		}
		pc = 0;
		uint32_t line = -1;
		Instruction newInstruction;
		do{
			SIM_MemInstRead( ++line, &newInstruction, id);
			program.push_back(newInstruction);
			assert(newInstruction.opcode == program[line].opcode);
		} while (program[line].opcode != CMD_HALT);
	}
};


struct Core {

	enum mtType{BLOCKED, FINE_GRAINED};
	vector<SingleThread> threads;
int cyclesNumber;
int instructionsNumber;

	Core(){} 
};




void CORE_BlockedMT() {

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

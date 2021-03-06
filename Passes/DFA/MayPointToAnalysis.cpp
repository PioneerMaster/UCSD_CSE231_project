#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Type.h"
#include <cstdint>
#include "231DFA.h"
#include <vector>
#include <map>
#include <set>
#include <string>


using namespace llvm;
using namespace std;

namespace llvm {
	class MayPointToInfo: public Info {
	public:
		MayPointToInfo() {}
		~MayPointToInfo() {}

		void print() {
			std::map<unsigned, set<string> > R_list;
			std::map<unsigned, set<string> > M_list;
			for(auto info: info_list) {
				string flag = info.first.substr(0, 1);
				string sub = info.first.substr(1);
				unsigned long lnum = std::stoul(sub, 0, 10);
    			unsigned num = lnum;
    			if(flag == "R") {
					R_list[num].insert(info.second.begin(), info.second.end());
				}
				else if(flag == "M") {
					M_list[num].insert(info.second.begin(), info.second.end());
				}
			}
			for(auto info : R_list) {
				errs()<< "R" + to_string(info.first) << "->(" ;
				for(auto pointee : info.second) {
					errs()<< pointee << "/" ;
				}
				errs()<< ")|" ;	
			}
			for(auto info : M_list) {
				errs()<< "M" + to_string(info.first) << "->(" ;
				for(auto pointee : info.second) {
					errs()<< pointee << "/" ;
				}
				errs()<< ")|" ;	
			}
			errs()<< "\n";	
		}

		static bool equals(MayPointToInfo * info1, MayPointToInfo * info2) {
			if(info1->info_list.size()!= info2->info_list.size()) return false;
			for(auto in: info1->info_list) {
				if(info2->info_list[in.first] != in.second)
					return false;
			}
			return true;
		}

		static void join(MayPointToInfo * info1, MayPointToInfo * info2, MayPointToInfo * result) {
			for(auto in : info1->info_list) {		
				result->info_list[in.first].insert(in.second.begin(), in.second.end());	
			}
			for(auto in : info2->info_list) {		
				result->info_list[in.first].insert(in.second.begin(), in.second.end());	
			}
			return;
		}

		std::map<string, set<string> > info_list;
	};


	class MayPointToAnalysis : public DataFlowAnalysis<MayPointToInfo, true> { 
	public:
		MayPointToAnalysis(MayPointToInfo &InitialStates, MayPointToInfo &Bottom):
								   DataFlowAnalysis<MayPointToInfo, true>(InitialStates, Bottom) {}

		~MayPointToAnalysis() {}

	    /*
     	* The flow function.
     	*   Instruction I: the IR instruction to be processed.
     	*   std::vector<unsigned> & IncomingEdges: the vector of the indices of the source instructions of the incoming edges.
     	*   std::vector<unsigned> & OutgoingEdges: the vector of indices of the source instructions of the outgoing edges.
     	*   std::vector<Info *> & Infos: the vector of the newly computed information for each outgoing eages.
     	*
     	* Direction:
     	* 	 Implement this function in subclasses.
     	*/
		void flowfunction(Instruction * I,
    													std::vector<unsigned> & IncomingEdges,
															std::vector<unsigned> & OutgoingEdges,
															std::vector<MayPointToInfo *> & Infos)
		{
			unsigned index = InstrToIndex[I];
			MayPointToInfo *info_in = new MayPointToInfo();
			for(unsigned src : IncomingEdges) {
				Edge in_edge = std::make_pair(src, index);
				MayPointToInfo * edge_info = EdgeToInfo[in_edge];
				MayPointToInfo::join(info_in, edge_info, info_in);
			}

			unsigned op = (*I).getOpcode();
			MayPointToInfo* info_index = new MayPointToInfo();
			string index_str = std::to_string(index);

			if(op == 29) {
				info_index->info_list["R"+index_str].insert("M"+index_str);
			}
			
			if(op == 47) {
				Value* v = (*I).getOperand(0);
				Instruction* v_instr = dyn_cast<Instruction>(v);
				unsigned v_index = InstrToIndex[v_instr];
				string v_str = std::to_string(v_index);
				for(auto in: info_in->info_list) {
					if(in.first == "R"+v_str) {
						info_index->info_list["R"+index_str].insert(in.second.begin(), in.second.end());
					}
				}
			}
			
			if(op == 32) {
				Value* v = ((GetElementPtrInst*)I)->getPointerOperand();
				Instruction* v_instr = dyn_cast<Instruction>(v);
				unsigned v_index = InstrToIndex[v_instr];
				string v_str = std::to_string(v_index);
				for(auto in: info_in->info_list) {
					if(in.first == "R"+v_str) {
						info_index->info_list["R"+index_str].insert(in.second.begin(), in.second.end());
					}
				}
			}
			
			// load
			if(op == 30) {
				Value* p = ((LoadInst*)I)->getPointerOperand();
				Instruction* Rp_instr = dyn_cast<Instruction>(p);
				unsigned Rp_index = InstrToIndex[Rp_instr];
				string Rp_str = "R" + std::to_string(Rp_index);
				if(info_in->info_list.count(Rp_str)) {
					std::set<string> tmp_Rp = info_in->info_list[Rp_str];
					for (auto x: tmp_Rp) {
						if(info_in->info_list.count(x) && isa<PointerType>(I->getType())) {
							std::set<string> tmp = info_in->info_list[x];
							info_index->info_list["R"+index_str].insert(tmp.begin(), tmp.end());
						}
					}
				}
			}

			if(op == 31) {
				Value* v = ((StoreInst*)I)->getValueOperand(); 
				Value* v_ptr = ((StoreInst*)I)->getPointerOperand();
				Instruction* Rv_instr = dyn_cast<Instruction>(v);
				Instruction* Rp_instr = dyn_cast<Instruction>(v_ptr);
				unsigned Rv_idx = InstrToIndex[Rv_instr];
				unsigned Rp_idx = InstrToIndex[Rp_instr];
				string Rv_str = "R" + std::to_string(Rv_idx);
				string Rp_str = "R" + std::to_string(Rp_idx);

				if(info_in->info_list.count(Rv_str) && info_in->info_list.count(Rp_str) ) {
					std::set<string> tmp_Rv = info_in->info_list[Rv_str];
					std::set<string> tmp_Rp = info_in->info_list[Rp_str];

					for (auto y: tmp_Rp) {
						for(auto x: tmp_Rv) {
							info_index->info_list[y].insert(x);
						}
					}
				}

			}

			if (op == 55) {
				Value* v1 = ((SelectInst*)I)->getTrueValue(); 
				Value* v2 = ((SelectInst*)I)->getFalseValue();
				Instruction* R1_instr = dyn_cast<Instruction>(v1);
				Instruction* R2_instr = dyn_cast<Instruction>(v2);
				unsigned R1_idx = InstrToIndex[R1_instr];
				unsigned R2_idx = InstrToIndex[R2_instr];
				string R1_str = "R" + std::to_string(R1_idx);
				string R2_str = "R" + std::to_string(R2_idx);

				if(info_in->info_list.count(R1_str)) {
					set<string> tmp_list = info_in->info_list[R1_str];
					info_index->info_list["R"+index_str].insert(tmp_list.begin(), tmp_list.end());
            	}	
            	if(info_in->info_list.count(R2_str)) {
					set<string> tmp_list = info_in->info_list[R2_str];
					info_index->info_list["R"+index_str].insert(tmp_list.begin(), tmp_list.end());
            	}
				
			}

			if(op == 53) {
				BasicBlock* block = I->getParent();
				for (auto ii = block->begin(), ie = block->end(); ii != ie; ++ii) {
            		Instruction * instr = &*ii;
            		if (isa<PHINode>(instr)){
            			for(unsigned i = 0; i < ((PHINode*)instr)->getNumIncomingValues(); i++) {
            				Value* v = ((PHINode*)instr)->getIncomingValue(i); 
            				Instruction* R_instr = dyn_cast<Instruction>(v);
            				unsigned R_idx = InstrToIndex[R_instr];
            				string R_str = "R" + std::to_string(R_idx);
            				if(info_in->info_list.count(R_str)) {
            					set<string> tmp_list = info_in->info_list[R_str];
            					info_index->info_list["R"+index_str].insert(tmp_list.begin(), tmp_list.end());
            				}	            			
            			}
            		}
				}
			}

			for(unsigned dst : OutgoingEdges) {
	  			Edge out_edge = std::make_pair(index, dst);
	  			MayPointToInfo* tmp_info = new MayPointToInfo();
				MayPointToInfo::join(info_in, info_index, tmp_info);
				Infos.push_back(tmp_info);
			}

			return;
		}

	};


	struct MayPointToAnalysisPass : public FunctionPass {
	  static char ID;
	  MayPointToAnalysisPass() : FunctionPass(ID) {}

	  bool runOnFunction(Function &F) override {
	  	MayPointToInfo InitialStates;
	  	MayPointToInfo Bottom;
	  	MayPointToAnalysis MPTA(InitialStates, Bottom);

	  	MPTA.runWorklistAlgorithm(&F);
	  	MPTA.print();
	  	
	    return false;
	  }
	}; // end of struct MayPointToAnalysisPass
}

char MayPointToAnalysisPass::ID = 0;
static RegisterPass<MayPointToAnalysisPass> X("cse231-maypointto", "May Point to Analysis Pass",
                             						  false /* Only looks at CFG */,
                             						  false /* Analysis Pass */);

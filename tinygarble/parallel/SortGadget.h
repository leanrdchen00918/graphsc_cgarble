#ifndef SORT_GADGET
#define SORT_GADGET
#include <chrono>
#include <iostream>
#include "Gadget.h"
#include "GraphNodeBitonicSortLib.h"
class SortGadget: public Gadget {
    public:
	vector<GraphNode*> nodes;
	NodeComparator* comp;

	SortGadget(Machine *machine, TinyGarblePI_SH* TGPI_SH): Gadget(machine, TGPI_SH){}

	SortGadget* setInputs(vector<GraphNode*> nodes, NodeComparator* comp) {
		this->nodes = nodes;
		this->comp = comp;
		return this;
	}

	virtual void secureCompute() override {
		long totalComm = 0, totalComp = 0, totalSort = 0;
		auto startSort = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count();
		GraphNodeBitonicSortLib* lib = new GraphNodeBitonicSortLib(comp, TGPI_SH);
        auto dir = TGPI_SH->TG_int(1);
        TGPI_SH->assign(dir, (int64_t)((machine->garblerId % 2 == 0) ? 1 : 0), 1);
		auto startComp = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count();
		lib->sort(nodes, dir);
		totalComp += chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count() - startComp;

		// if (machine.getGarblerId() == 0 && Party.Alice.equals(env.getParty())) {
		// 	System.out.println("SG sc called once" + machine.getLogMachines());
		// }
        auto mergeDir = TGPI_SH->TG_int(1);
		for (int k = 0; k < machine->logMachines; k++) {
			// cout << "k: " << k << endl;
			int diff = (1 << k);
            
            TGPI_SH->assign(mergeDir, (int64_t)(((machine->garblerId / (2 * (1 << k))) % 2 == 0) ? 1 : 0), 1); 
			while (diff != 0) {
				bool up = (machine->garblerId / diff) % 2 == 1 ? true : false;
				NetIO* channel;
				int commMachine = (int)log2(diff);
				channel = up ? machine->peersUp[commMachine] : machine->peersDown[commMachine];

				// long startCommunicate = System.nanoTime();
				auto startComm = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count();
				auto receivedNodes = sendReceive(channel, nodes, nodes.size(), up);
				totalComm += chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count() - startComm;
				// long endCommunicate = System.nanoTime(), startConcatenate = System.nanoTime();

				auto concatenatedNodes = up ? concatenate(receivedNodes, nodes) : concatenate(nodes, receivedNodes);

				// long endConcatenate = System.nanoTime();
				auto startComp = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count();
				lib->compareAndSwapFirst(concatenatedNodes, 0, concatenatedNodes.size(), mergeDir);
				totalComp += chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count() - startComp;

				// long startConcatenate2 = System.nanoTime();
				// int srcPos = up ? concatenatedNodes.size() / 2 : 0;
				// System.arraycopy(concatenatedNodes, srcPos, nodes, 0, concatenatedNodes.length / 2);// TODO: seems unnecessary due to usage of GraphNode*
				// long endConcatenate2 = System.nanoTime();
				// communicate += (endCommunicate - startCommunicate);
				// System.out.println(
				// "party: " +  (machine.isGen() ? "A" : "B") + ", node: " 
				// + machine.getGarblerId() + " , k: " + k + ", diff: " + diff + " , comm: " + (endCommunicate - startCommunicate) / 1000000.0 + " ms");

				// concatenate += (endConcatenate2 - startConcatenate2) + (endConcatenate - startConcatenate);
				diff /= 2;
                for(int i = 0; i < receivedNodes.size(); i++){
                    receivedNodes[i]->free_obj();
                }
			}
			auto startComp = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count();
			lib->bitonicMerge(nodes, 0, nodes.size(), mergeDir);
			totalComp += chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count() - startComp;
		}
		// if (machine.getGarblerId() == 0 && Party.Alice.equals(env.getParty())) {
		// 	System.out.println("commRoundCnt: " + machine.getCommRoundCnt());
		// }
		totalSort += chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count() - startSort;
		std::cout << "			totalSort: " << totalSort << "ms, totalComp: " << totalComp << "ms, totalComm: " << totalComm << "ms" << std::endl;
		delete lib;
        TGPI_SH->clear_TG_int(dir);
        TGPI_SH->clear_TG_int(mergeDir);

		// long finalTimer = System.nanoTime();
		// compute = finalTimer - initTimer - (communicate + concatenate);
		// long total = finalTimer	- initTimer;
		// machine.commTime += communicate / 1000000000.0;
		// machine.computeTime += compute / 1000000000.0;
		// machine.times[1] += communicate;
		// machine.times[2] += compute;
		// System.out.println(
		// 	"party: " +  (machine.isGen() ? "A" : "B") + ", node: " 
		// 	+ machine.getGarblerId() + " , total: " + total / 1000000.0 + "ms, compute: " + compute / 1000000.0 + " ms, comm: " + communicate / 1000000.0 + " ms");
		// return communicate;
        
	}

	vector<GraphNode*> sendReceive(NetIO* channel, const vector<GraphNode*>& nodes, int arrayLength, bool up){
	    // cout << "enter sendReceive" << endl;
		auto a = vector<GraphNode*>(arrayLength);
        for(int i = 0; i < a.size(); i++){
            a[i] = nodes[0]->alloc_obj();
        }
		int toTransfer = nodes.size();
		int i = 0, j = 0;
		while (toTransfer > 0) {
			int curTransfer = min(toTransfer, 8);
			toTransfer -= curTransfer;
			if(up){
				for (int k = 0; k < curTransfer; k++, i++) {
					nodes[i]->send(channel, TGPI_SH);
				}
				channel->flush();
				// cout << "up done send(one round)" << endl;
				for (int k = 0; k < curTransfer; k++, j++) {
					a[j]->read(channel, TGPI_SH);
				}
				// cout << "up done read(one round)" << endl;
			}
			else{
				for (int k = 0; k < curTransfer; k++, j++) {
					a[j]->read(channel, TGPI_SH);
				}
				// cout << "down done read(one round)" << endl;
				for (int k = 0; k < curTransfer; k++, i++) {
					nodes[i]->send(channel, TGPI_SH);
				}
				channel->flush();
				// cout << "down done send(one round)" << endl;
			}
			
		}
		// machine.commCnt += nodes.length;
		// System.out.println("commCnt: " + machine.getCommCnt());
		
		// machine.commRoundCnt += 1;
		// cout << "done sendReceive" << endl;
		return a;
	}

	vector<GraphNode*> concatenate(const vector<GraphNode*>& A, const vector<GraphNode*>& B) {
        auto C = vector<GraphNode*>();
        C.insert(C.end(), A.begin(), A.end());
        C.insert(C.end(), B.begin(), B.end());
        // TGPI_SH->assign(C, A, aLen);
        // auto tmp = C + aLen;
        // TGPI_SH->assign(tmp, B, bLen);
	    return C;
	}
};
#endif

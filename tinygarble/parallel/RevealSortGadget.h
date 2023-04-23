#ifndef REVEAL_SORT_GADGET
#define REVEAL_SORT_GADGET
#include <chrono>
#include <iostream>
#include "Gadget.h"
#include "RevealSortLib.h"
#include "Utils.h"
class RevealSortGadget: public Gadget {
	private:
	long commTime;
	vector<int> indexes;
    public:
	NodeComparator* comp;
	bool isRestore;

	RevealSortGadget(Machine *machine, TinyGarblePI_SH* TGPI_SH): Gadget(machine, TGPI_SH){}

	RevealSortGadget* setInputs(NodeComparator* comp, bool isRestore) {
		this->comp = comp;
		this->isRestore = isRestore;
		return this;
	}

	void setInitialIndexes(int nodesLen){
		indexes = vector<int>(nodesLen);
		int base = machine->garblerId * nodesLen;
		for(int i = 0; i < indexes.size(); i++){
			indexes[i] = base + i;
		}
	}

	virtual void secureCompute() override {}
	virtual void secureCompute(vector<GraphNode*>& nodes) override {
		if(isRestore){
			restoreByIndex(nodes);
			return;
		}
		// sort case
		if(!isFirstSort()){
			sortByIndex(nodes);
			return;
		}
		// first sort case
		setInitialIndexes(nodes.size());
		long totalComm = 0, totalComp = 0, totalSort = 0;
		auto startSort = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count();
		RevealSortLib* lib = new RevealSortLib(comp, TGPI_SH);
        auto dir = TGPI_SH->TG_int(1);
        TGPI_SH->assign(dir, (int64_t)((machine->garblerId % 2 == 0) ? 1 : 0), 1);
		auto startComp = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count();
		lib->sort(nodes, indexes, dir);
		totalComp += chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count() - startComp;

        auto mergeDir = TGPI_SH->TG_int(1);
		for (int k = 0; k < machine->logMachines; k++) {
			int diff = (1 << k);
            
            TGPI_SH->assign(mergeDir, (int64_t)(((machine->garblerId / (2 * (1 << k))) % 2 == 0) ? 1 : 0), 1); 
			while (diff != 0) {
				bool up = (machine->garblerId / diff) % 2 == 1 ? true : false;
				NetIO* channel;
				int commMachine = (int)log2(diff);
				channel = up ? machine->logPeersUp[commMachine] : machine->logPeersDown[commMachine];

				auto startComm = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count();
				auto receivedNodes = sendReceive(channel, nodes, up, TGPI_SH);
				auto receivedIndexes = sendReceive(channel, indexes, up, TGPI_SH);
				totalComm += chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count() - startComm;

				auto concatenatedNodes = up ? concatenate(receivedNodes, nodes) : concatenate(nodes, receivedNodes);
				auto concatenatedIndexes = up ? concatenate(receivedIndexes, indexes) : concatenate(indexes, receivedIndexes);

				auto startComp = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count();
				lib->compareAndSwapFirst(concatenatedNodes, concatenatedIndexes, 0, concatenatedNodes.size(), mergeDir);
				totalComp += chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count() - startComp;

				int startPos = up ? concatenatedIndexes.size() / 2 : 0;
				auto startIter = concatenatedIndexes.begin() + startPos;
				auto endIter = startIter + indexes.size();
				indexes.assign(startIter, endIter);

				diff /= 2;
                for(int i = 0; i < receivedNodes.size(); i++){
                    receivedNodes[i]->free_obj();
                }
			}
			auto startComp = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count();
			lib->bitonicMerge(nodes, indexes, 0, nodes.size(), mergeDir);
			totalComp += chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count() - startComp;
		}

		totalSort += chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count() - startSort;
		std::cout << "			(reveal) totalSort: " << totalSort << "ms, totalComp: " << totalComp << "ms, totalComm: " << totalComm << "ms" << std::endl;
		delete lib;
        TGPI_SH->clear_TG_int(dir);
        TGPI_SH->clear_TG_int(mergeDir);
	}

	bool isFirstSort(){
		return (indexes.size() == 0);
	}

	void sortInterNode(vector<GraphNode*>& nodes, vector<GraphNode*>& tmpNodes, int peerId){
		// cout << "peerId: " << peerId << endl;
		bool up = (machine->garblerId > peerId) ? true : false;
		auto channel = up ? machine->fullPeersUp[machine->garblerId - peerId - 1] : machine->fullPeersDown[peerId - machine->garblerId - 1];

		auto startComm = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count();
		auto receivedNodes = sendReceive(channel, nodes, up, TGPI_SH);
		commTime += chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count() - startComm;

		int receivedMinIndex = peerId * nodes.size(), receivedMaxIndex = receivedMinIndex + nodes.size() - 1;
		for(int j = 0; j < indexes.size(); j++){
			if(receivedMinIndex <= indexes[j] && indexes[j] <= receivedMaxIndex){
				// tmpNodes[j] = receivedNodes[indexes[j] - receivedMinIndex]
				auto src = receivedNodes[indexes[j] - receivedMinIndex]->flatten(TGPI_SH);
				tmpNodes[j]->unflatten(src, TGPI_SH);
				delete[] src;
			}
		}
		for(int j = 0; j < receivedNodes.size(); j++){
			receivedNodes[j]->free_obj();
		}
		// cout << "done peerId: " << peerId << endl;
	}

	void sortByIndex(vector<GraphNode*>& nodes){
		commTime = 0;
		// alloc tmpNodes
		auto tmpNodes = vector<GraphNode*>(nodes.size());
		for(int i = 0; i < tmpNodes.size(); i++){
			tmpNodes[i] = nodes[0]->alloc_obj();
		}
		// sort by indexes locally
		int localMinIndex = machine->garblerId * nodes.size(), localMaxIndex = localMinIndex + nodes.size() - 1;
		for(int j = 0; j < indexes.size(); j++){
			if(localMinIndex <= indexes[j] && indexes[j] <= localMaxIndex){
				// tmpNodes[j] = nodes[indexes[j] - localMinIndex]
				auto src = nodes[indexes[j] - localMinIndex]->flatten(TGPI_SH);
				tmpNodes[j]->unflatten(src, TGPI_SH);
				delete[] src;
			}
		}
		// inter-node sort
		for(int step = 1; step <= machine->totalMachines / 2; step++){
			int peerId, upPeerId = ((machine->garblerId + step) % machine->totalMachines), downPeerId = ((machine->garblerId - step + machine->totalMachines) % machine->totalMachines);
			int factor2k = 1, tmpStep = step;
			while((tmpStep & 1) == 0){ // end with 0
				factor2k *= 2;
				tmpStep /= 2;
			}
			bool upPeerFirst = ((machine->garblerId % (2 * factor2k)) < factor2k) ? true : false;
			// cout << "factor2k " << factor2k << ", upPeerFirst" << upPeerFirst << endl; 
			if(step < machine->totalMachines / 2){
				peerId = (upPeerFirst) ? upPeerId : downPeerId;
				sortInterNode(nodes, tmpNodes, peerId);
				peerId = (upPeerFirst) ? downPeerId : upPeerId;
				sortInterNode(nodes, tmpNodes, peerId);
			}
			else{
				peerId = (upPeerFirst) ? upPeerId : downPeerId;
				sortInterNode(nodes, tmpNodes, peerId);
			}	
		}
		// copy tmpNodes to nodes, and free tmpNodes
		for(int i = 0; i < nodes.size(); i++){
			nodes[i]->unflatten(tmpNodes[i]->flatten(TGPI_SH), TGPI_SH);
			tmpNodes[i]->free_obj();
		}

		cout << "			in sortByIndex, commTime: " << commTime << "ms" << endl;
	}

	void restoreInterNode(vector<GraphNode*>& nodes, vector<GraphNode*>& tmpNodes, int peerId){
		// cout << "peerId: " << peerId << endl;
		int localMinIndex = machine->garblerId * nodes.size(), localMaxIndex = localMinIndex + nodes.size() - 1;
		bool up = (machine->garblerId > peerId) ? true : false;
		auto channel = up ? machine->fullPeersUp[machine->garblerId - peerId - 1] : machine->fullPeersDown[peerId - machine->garblerId - 1];

		auto receivedNodes = sendReceive(channel, nodes, up, TGPI_SH);
		auto receivedIndexes = sendReceive(channel, indexes, up, TGPI_SH);
		for(int j = 0; j < indexes.size(); j++){
			if(localMinIndex <= indexes[j] && indexes[j] <= localMaxIndex){
				// tmpNodes[indexes[j] - localMinIndex] = receivedNodes[j]
				auto src = receivedNodes[j]->flatten(TGPI_SH);
				tmpNodes[indexes[j] - localMinIndex]->unflatten(src, TGPI_SH);
				delete[] src;
			}
		}
		for(int j = 0; j < receivedNodes.size(); j++){
			receivedNodes[j]->free_obj();
		}
		// cout << "done peerId: " << peerId << endl;
	}

	void restoreByIndex(vector<GraphNode*>& nodes){
		// alloc tmpNodes
		auto tmpNodes = vector<GraphNode*>(nodes.size());
		for(int i = 0; i < tmpNodes.size(); i++){
			tmpNodes[i] = nodes[0]->alloc_obj();
		}
		// restore by indexes locally
		int localMinIndex = machine->garblerId * nodes.size(), localMaxIndex = localMinIndex + nodes.size() - 1;
		for(int j = 0; j < indexes.size(); j++){
			// cout << "indexes[" << j << "]: " << indexes[j] << endl;
			if(localMinIndex <= indexes[j] && indexes[j] <= localMaxIndex){
				// tmpNodes[indexes[j] - localMinIndex] = nodes[j]
				auto src = nodes[j]->flatten(TGPI_SH);
				tmpNodes[indexes[j] - localMinIndex]->unflatten(src, TGPI_SH);
				delete[] src;
			}
		}
		// inter-node restore
		for(int step = 1; step <= machine->totalMachines / 2; step++){
			int peerId, upPeerId = ((machine->garblerId + step) % machine->totalMachines), downPeerId = ((machine->garblerId - step + machine->totalMachines) % machine->totalMachines);
			int factor2k = 1, tmpStep = step;
			while((tmpStep & 1) == 0){ // end with 0
				factor2k *= 2;
				tmpStep /= 2;
			}
			bool upPeerFirst = ((machine->garblerId % (2 * factor2k)) < factor2k) ? true : false;
			// cout << "factor2k " << factor2k << ", upPeerFirst" << upPeerFirst << endl; 
			if(step < machine->totalMachines / 2){
				peerId = (upPeerFirst) ? upPeerId : downPeerId;
				restoreInterNode(nodes, tmpNodes, peerId);
				peerId = (upPeerFirst) ? downPeerId : upPeerId;
				restoreInterNode(nodes, tmpNodes, peerId);
			}
			else{
				peerId = (upPeerFirst) ? upPeerId : downPeerId;
				restoreInterNode(nodes, tmpNodes, peerId);
			}	
		}


		// copy tmpNodes to nodes, and free tmpNodes
		for(int i = 0; i < nodes.size(); i++){
			nodes[i]->unflatten(tmpNodes[i]->flatten(TGPI_SH), TGPI_SH);
			tmpNodes[i]->free_obj();
		}
	}
};
#endif

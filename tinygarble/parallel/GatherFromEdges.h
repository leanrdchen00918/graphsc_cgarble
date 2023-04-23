#ifndef GATHER_FROM_EDGES
#define GATHER_FROM_EDGES
#include <time.h>
#include <vector>
#include <chrono>
#include "Gadget.h"
#include "GraphNode.h"
#include "SortGadget.h"
#include "RevealSortGadget.h"
using namespace std;
class GatherFromEdges: public Gadget{
    public:
	bool isEdgeIncoming;
	bool useRevealSort;
	GraphNode* identityNode;
	Gadget* sortGadget;

    GraphNode* (*aggFunc)(GraphNode* agg, GraphNode* b, TinyGarblePI_SH* TGPI_SH);
    void (*writeToVertex)(GraphNode* agg, GraphNode* vertex, TinyGarblePI_SH* TGPI_SH);

	GatherFromEdges(TinyGarblePI_SH* TGPI_SH, Machine* machine, bool isEdgeIncoming, bool useRevealSort, GraphNode* identityNode, auto aggFunc, auto writeToVertex)
    : Gadget(machine, TGPI_SH), isEdgeIncoming(isEdgeIncoming), useRevealSort(useRevealSort), identityNode(identityNode), aggFunc(aggFunc), writeToVertex(writeToVertex){
		this->sortGadget = (useRevealSort) ? (new RevealSortGadget(machine, TGPI_SH)) : nullptr;
	}
	~GatherFromEdges(){
		delete sortGadget;
	}

	GatherFromEdges* setInputs() {
		return this;
	}
	virtual void secureCompute() override {}
    virtual void secureCompute(vector<GraphNode*>& nodes) override {
		// auto start = clock();
		auto start = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count();
		if (isEdgeIncoming) {
			for (auto node: nodes) {
				node->swapEdgeDirections();
			}
		}

		if(!useRevealSort){
			(new SortGadget(machine, TGPI_SH))
			->setInputs(nodes[0]->getComparator(TGPI_SH, true))
			->secureCompute(nodes);
		}
		else{
			// shuffle B -> sorted dst list
			(dynamic_cast<RevealSortGadget*>(sortGadget))
			->setInputs(nodes[0]->getComparator(TGPI_SH, true), false)
			->secureCompute(nodes);
		}
		auto sortTime = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count() - start;
		cout << "		done shuffle B -> sorted dst list in: " << sortTime << "ms" << endl;
		// cout << "		done gather-sort in: " << double(clock() - start) / CLOCKS_PER_SEC << "s" << endl;
		// cout << "done sort in gather: " << endl;
		// for(auto node: nodes){
		// 	node->printNode(TGPI_SH);
		// }
		// exit(-1);

        auto graphNodeVal = identityNode->getCopy(TGPI_SH);
        auto zeroNode = identityNode->alloc_obj()->set_zero(TGPI_SH); // initialized as ZERO

		for (auto node: nodes) {
			auto tempAgg = aggFunc(graphNodeVal, node, TGPI_SH);
			graphNodeVal->mux(tempAgg, zeroNode, node->isVertex, TGPI_SH);
            tempAgg->free_obj();
		}

		// cout << "reach here" << endl;
		

		auto nodeValForLaterComp = identityNode->getCopy(TGPI_SH);

		int noOfIncomingConnections = machine->logPeersDown.size();
		int noOfOutgoingConnections = machine->logPeersUp.size();
		for (int k = 0; k < machine->logMachines; k++) {
			if (noOfIncomingConnections > 0) {
				machine->logPeersDown[k]->send_data(nodes[nodes.size() - 1]->u, sizeof(block) * 16);
				graphNodeVal->send(machine->logPeersDown[k], TGPI_SH);
				machine->logPeersDown[k]->flush();
				noOfIncomingConnections--;
			}
			if (noOfOutgoingConnections > 0) {
                auto prevU = TGPI_SH->TG_int(16);
				machine->logPeersUp[k]->recv_data(prevU, sizeof(block) * 16);
				auto graphNodeRead = identityNode->alloc_obj();
				graphNodeRead->read(machine->logPeersUp[k], TGPI_SH);

                auto sameU = TGPI_SH->TG_int(1);
				TGPI_SH->eq(sameU, prevU, nodes[0]->u, 16);
				auto tempAgg = aggFunc(nodeValForLaterComp, graphNodeRead, TGPI_SH);
				nodeValForLaterComp->mux(nodeValForLaterComp, tempAgg, sameU, TGPI_SH);
				noOfOutgoingConnections--;

				TGPI_SH->clear_TG_int(prevU);
				graphNodeRead->free_obj();
				TGPI_SH->clear_TG_int(sameU);
				tempAgg->free_obj();
			}
		}

		for (auto node: nodes) {
			auto tempAgg = aggFunc(nodeValForLaterComp, node, TGPI_SH);
			writeToVertex(nodeValForLaterComp, node, TGPI_SH);
			nodeValForLaterComp->mux(tempAgg, zeroNode, node->isVertex, TGPI_SH);
			tempAgg->free_obj();
		}

		auto compTime = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count() - start - sortTime;
		cout << "		done compute in: " << compTime << "ms" << endl;
		
		if(useRevealSort){
			// sorted dst list -> shuffle B
			(dynamic_cast<RevealSortGadget*>(sortGadget))
			->setInputs(nodes[0]->getComparator(TGPI_SH, true), true)
			->secureCompute(nodes);
		}
		sortTime = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count() - start - sortTime - compTime;
		cout << "		done sorted dst list -> shuffle B in: " << sortTime << "ms" << endl;


		if (isEdgeIncoming) {
			for (auto node: nodes) {
				node->swapEdgeDirections();
			}
		}
		
        graphNodeVal->free_obj();
        zeroNode->free_obj();
        nodeValForLaterComp->free_obj();

		auto totalTime = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count() - start;
		cout << "	done gather in: " << totalTime << "ms" << endl;
		// cout << "	done gather in: " << double(clock() - start) / CLOCKS_PER_SEC << "s" << endl;
	}
};
#endif

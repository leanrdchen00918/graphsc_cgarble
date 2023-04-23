#ifndef GATHER_FROM_EDGES
#define GATHER_FROM_EDGES
#include <time.h>
#include <vector>
#include <chrono>
#include "Gadget.h"
#include "GraphNode.h"
#include "SortGadget.h"
using namespace std;
class GatherFromEdges: public Gadget{
    public:
	vector<GraphNode*> nodes;
	bool isEdgeIncoming;
	GraphNode* identityNode;

    GraphNode* (*aggFunc)(GraphNode* agg, GraphNode* b, TinyGarblePI_SH* TGPI_SH);
    void (*writeToVertex)(GraphNode* agg, GraphNode* vertex, TinyGarblePI_SH* TGPI_SH);

	GatherFromEdges(TinyGarblePI_SH* TGPI_SH, Machine* machine, bool isEdgeIncoming, GraphNode* identityNode, auto aggFunc, auto writeToVertex)
    : Gadget(machine, TGPI_SH), isEdgeIncoming(isEdgeIncoming), identityNode(identityNode), aggFunc(aggFunc), writeToVertex(writeToVertex){}

	GatherFromEdges* setInputs(vector<GraphNode*> nodes) {
		this->nodes = nodes;
		return this;
	}

    virtual void secureCompute() override {
		auto start = clock();
		// long startInit = System.nanoTime();
		if (isEdgeIncoming) {
			for (auto node: nodes) {
				node->swapEdgeDirections();
			}
		}

		// long startSort = System.nanoTime();
		// long communicate = (long) new SortGadget<T>(env, machine)
		// 	.setInputs(nodes, nodes[0].getComparator(env, true /* isVertexLast */))
		// 	.secureCompute();
		// long sortTime = (System.nanoTime() - startSort);
		// machine.times[4] += sortTime;

		(new SortGadget(machine, TGPI_SH))
		->setInputs(nodes, nodes[0]->getComparator(TGPI_SH, true))
		->secureCompute();

		cout << "		done gather-sort in: " << double(clock() - start) / CLOCKS_PER_SEC << "s" << endl;
		// cout << "done sort in gather: " << endl;
		// for(auto node: nodes){
		// 	node->printNode(TGPI_SH);
		// }
		// exit(-1);

        auto graphNodeVal = identityNode->getCopy(TGPI_SH);
        auto zeroNode = identityNode->alloc_obj()->set_zero(TGPI_SH); // initialized as ZERO
		// zeroNode->free_obj();

		for (auto node: nodes) {
			auto tempAgg = aggFunc(graphNodeVal, node, TGPI_SH);
			graphNodeVal->mux(tempAgg, zeroNode, node->isVertex, TGPI_SH);
            tempAgg->free_obj();
		}

		// cout << "reach here" << endl;
		

		auto nodeValForLaterComp = identityNode->getCopy(TGPI_SH);

		int noOfIncomingConnections = machine->numberOfIncomingConnections;
		int noOfOutgoingConnections = machine->numberOfOutgoingConnections;
		for (int k = 0; k < machine->logMachines; k++) {
			if (noOfIncomingConnections > 0) {
				// long one = System.nanoTime();

				machine->peersDown[k]->send_data(nodes[nodes.size() - 1]->u, sizeof(block) * 16);
				graphNodeVal->send(machine->peersDown[k], TGPI_SH);
				machine->peersDown[k]->flush();
				noOfIncomingConnections--;

				// long two = System.nanoTime();
				// communicate += (two - one);
			}
			if (noOfOutgoingConnections > 0) {
				// long one = System.nanoTime();
                auto prevU = TGPI_SH->TG_int(16);
				machine->peersUp[k]->recv_data(prevU, sizeof(block) * 16);
				auto graphNodeRead = identityNode->alloc_obj();
				graphNodeRead->read(machine->peersUp[k], TGPI_SH);

				// long two = System.nanoTime();
				// communicate += (two - one);

                auto sameU = TGPI_SH->TG_int(1);
				TGPI_SH->eq(sameU, prevU, nodes[0]->u, 16);
				// T sameU = lib.eq(prevU, nodes[0].u);
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

		if (isEdgeIncoming) {
			for (auto node: nodes) {
				node->swapEdgeDirections();
			}
		}
		// machine.times[3] += (System.nanoTime() - startInit - sortTime);
		
        graphNodeVal->free_obj();
        zeroNode->free_obj();
        nodeValForLaterComp->free_obj();
		// return communicate;
		cout << "	done gather in: " << double(clock() - start) / CLOCKS_PER_SEC << "s" << endl;
	}
};
#endif

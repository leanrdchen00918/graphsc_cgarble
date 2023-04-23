#ifndef SCATTER_TO_EDGES
#define SCATTER_TO_EDGES
#include <vector>
#include "Gadget.h"
#include "GraphNode.h"
#include "SortGadget.h"
using namespace std;
class ScatterToEdges: public Gadget{
    public:
	vector<GraphNode*> nodes;
	bool isEdgeIncoming;

    void (*writeToEdge)(GraphNode* vertexNode, GraphNode* edgeNode, block* isVertex, TinyGarblePI_SH* TGPI_SH);

	ScatterToEdges(TinyGarblePI_SH* TGPI_SH, Machine* machine, bool isEdgeIncoming, auto writeToEdge)
    : Gadget(machine, TGPI_SH), isEdgeIncoming(isEdgeIncoming), writeToEdge(writeToEdge){}

	ScatterToEdges* setInputs(vector<GraphNode*> prNodes) {
		this->nodes = prNodes;
		return this;
	}

    virtual void secureCompute() override {
		// long startInit = System.nanoTime();
		if (isEdgeIncoming) {
			for (auto node: nodes) {
				node->swapEdgeDirections();
			}
		}

		// long startSort = System.nanoTime();
		// long communicate = (long) new SortGadget<T>(env, machine)
		// 	.setInputs(nodes, nodes[0].getComparator(env, false /* isVertexLast */))
		// 	.secureCompute();
		// long sortTime = (System.nanoTime() - startSort);
		// machine.times[4] += sortTime;

		(new SortGadget(machine, TGPI_SH))
		->setInputs(nodes, nodes[0]->getComparator(TGPI_SH, false))
		->secureCompute();

		auto _true = TGPI_SH->TG_int(1);
		TGPI_SH->assign(_true, (int64_t)1, 1);

		auto foundToSend = TGPI_SH->TG_int(1);
		TGPI_SH->assign(foundToSend, (int64_t)0, 1);
		auto graphNode = nodes[0]->alloc_obj();

		for (auto node: nodes) {
			TGPI_SH->ifelse(foundToSend, node->isVertex, _true, foundToSend, 1); // always sent
			graphNode->mux(graphNode, node, node->isVertex, TGPI_SH);
		}

		auto found = TGPI_SH->TG_int(1);
		TGPI_SH->assign(foundToSend, (int64_t)0, 1);
		auto graphNodeLast = nodes[0]->alloc_obj();

		int noOfIncomingConnections = machine->numberOfIncomingConnections;
		int noOfOutgoingConnections = machine->numberOfOutgoingConnections;
		
		for (int k = 0; k < machine->logMachines; k++) {
			if (noOfIncomingConnections > 0) {
				// long one = System.nanoTime();

				machine->peersDown[k]->send_data(foundToSend, sizeof(block));
				graphNode->send(machine->peersDown[k], TGPI_SH);
				machine->peersDown[k]->flush();
				noOfIncomingConnections--;

				// long two = System.nanoTime();
				// communicate += (two - one);
			}
			if (noOfOutgoingConnections > 0) {
				// long one = System.nanoTime();
                auto foundRead = TGPI_SH->TG_int(1);
				machine->peersUp[k]->recv_data(foundRead, sizeof(block));
				auto graphNodeRead = nodes[0]->alloc_obj();
				graphNodeRead->read(machine->peersUp[k], TGPI_SH);

				// long two = System.nanoTime();
				// communicate += (two - one);

                // compute the value for the last vertex
				graphNodeLast->mux(graphNodeRead, graphNodeLast, found, TGPI_SH);
				TGPI_SH->ifelse(found, foundRead, _true, found, 1);
				noOfOutgoingConnections--;

				TGPI_SH->clear_TG_int(foundRead);
				graphNodeRead->free_obj();
			}
		}

		// found will always be true in the end!
		for (auto node: nodes) {
			writeToEdge(graphNodeLast, node, node->isVertex, TGPI_SH);
			graphNodeLast->mux(graphNodeLast, node, node->isVertex, TGPI_SH);
		}
		
		if (isEdgeIncoming) {
			for (auto node: nodes) {
				node->swapEdgeDirections();
			}
		}
		// machine.times[3] += (System.nanoTime() - startInit - sortTime);
		
        TGPI_SH->clear_TG_int(_true);
		TGPI_SH->clear_TG_int(foundToSend);
		graphNode->free_obj();
		TGPI_SH->clear_TG_int(found);
		graphNodeLast->free_obj();
		// return communicate;
	}
};
#endif

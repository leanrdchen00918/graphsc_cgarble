#ifndef SCATTER_TO_EDGES
#define SCATTER_TO_EDGES
#include <vector>
#include "Gadget.h"
#include "GraphNode.h"
#include "SortGadget.h"
#include "RevealSortGadget.h"
using namespace std;
class ScatterToEdges: public Gadget{
    public:
	bool isEdgeIncoming;
	bool useRevealSort;
	Gadget* sortGadget;

    void (*writeToEdge)(GraphNode* vertexNode, GraphNode* edgeNode, block* isVertex, TinyGarblePI_SH* TGPI_SH);

	ScatterToEdges(TinyGarblePI_SH* TGPI_SH, Machine* machine, bool isEdgeIncoming, bool useRevealSort, auto writeToEdge)
    : Gadget(machine, TGPI_SH), isEdgeIncoming(isEdgeIncoming), useRevealSort(useRevealSort), writeToEdge(writeToEdge){
		this->sortGadget = (useRevealSort) ? (new RevealSortGadget(machine, TGPI_SH)) : nullptr;
	}
	~ScatterToEdges(){
		delete sortGadget;
	}

	ScatterToEdges* setInputs() {
		return this;
	}
	virtual void secureCompute() override {}
    virtual void secureCompute(vector<GraphNode*>& nodes) override {
		auto start = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count();
		if (isEdgeIncoming) {
			for (auto node: nodes) {
				node->swapEdgeDirections();
			}
		}

		if(!useRevealSort){
			(new SortGadget(machine, TGPI_SH))
			->setInputs(nodes[0]->getComparator(TGPI_SH, false))
			->secureCompute(nodes);
		}
		else{
			// shuffle A -> sorted src list
			(dynamic_cast<RevealSortGadget*>(sortGadget))
			->setInputs(nodes[0]->getComparator(TGPI_SH, false), false)
			->secureCompute(nodes);
		}
		auto sortTime = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count() - start;
		cout << "		done shuffle A -> sorted src list in: " << sortTime << "ms" << endl;
		

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

		int noOfIncomingConnections = machine->logPeersDown.size();
		int noOfOutgoingConnections = machine->logPeersUp.size();
		
		for (int k = 0; k < machine->logMachines; k++) {
			if (noOfIncomingConnections > 0) {
				machine->logPeersDown[k]->send_data(foundToSend, sizeof(block));
				graphNode->send(machine->logPeersDown[k], TGPI_SH);
				machine->logPeersDown[k]->flush();
				noOfIncomingConnections--;
			}
			if (noOfOutgoingConnections > 0) {
                auto foundRead = TGPI_SH->TG_int(1);
				machine->logPeersUp[k]->recv_data(foundRead, sizeof(block));
				auto graphNodeRead = nodes[0]->alloc_obj();
				graphNodeRead->read(machine->logPeersUp[k], TGPI_SH);

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

		auto compTime = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count() - start - sortTime;
		cout << "		done compute in: " << compTime << "ms" << endl;

		if(useRevealSort){
			// sorted src list -> shuffle A
			(dynamic_cast<RevealSortGadget*>(sortGadget))
			->setInputs(nodes[0]->getComparator(TGPI_SH, false), true)
			->secureCompute(nodes);
		}
		sortTime = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count() - start - sortTime - compTime;
		cout << "		done sorted src list -> shuffle A in: " << sortTime << "ms" << endl;

		if (isEdgeIncoming) {
			for (auto node: nodes) {
				node->swapEdgeDirections();
			}
		}
		
        TGPI_SH->clear_TG_int(_true);
		TGPI_SH->clear_TG_int(foundToSend);
		graphNode->free_obj();
		TGPI_SH->clear_TG_int(found);
		graphNodeLast->free_obj();

		auto totalTime = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count() - start;
		cout << "	done scatter in: " << totalTime << "ms" << endl;
	}
};
#endif

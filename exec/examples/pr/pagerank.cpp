#include <vector>
#include <time.h>
#include "PageRankNode.h"
#include "SetInitialPageRankGadget.h"
#include "tinygarble/parallel/GatherFromEdges.h"
#include "tinygarble/parallel/ScatterToEdges.h"
#include "tinygarble/parallel/PermuteGadget.h"
#include "tinygarble/parallel/RevealSortGadget.h"
using namespace std;
class PageRank: public Gadget {
    public:
	const static int ITERATIONS = 5;

	PageRank(TinyGarblePI_SH* TGPI_SH, Machine* machine): Gadget(machine, TGPI_SH) {}
    virtual void secureCompute(vector<GraphNode*>& nodes) override {}
	virtual void secureCompute() override {
		auto start = clock();
        cout << "start compute" << endl;
		int inputLength = machine->inputLength / machine->totalMachines;
        cout << "inputLen: " << inputLength << endl;
        string inputFile = machine->isGen ? "./bin/in/garb.in" : "./bin/in/eval.in";
        ifstream fin(inputFile.c_str(), std::ios::in);

        auto nodes = vector<GraphNode*>(inputLength);
        auto tmpPR = new PageRankNode();
        for(int i = 0; i < nodes.size(); i++){
            nodes[i] = tmpPR->alloc_obj();
        }
        delete tmpPR;

        if(machine->isGen){ // ALICE
            int64_t tmp = 0;
            for(int i = 0; i < inputLength / 2; i++){
                fin >> tmp;
                TGPI_SH->register_input(ALICE, GraphNode::VERTEX_LEN, tmp); // u
                fin >> tmp;
                TGPI_SH->register_input(ALICE, GraphNode::VERTEX_LEN, tmp); // v
                fin >> tmp;
                TGPI_SH->register_input(ALICE, 1, tmp); // isV
            }
            for(int i = inputLength / 2; i < inputLength; i++){
                TGPI_SH->register_input(BOB, GraphNode::VERTEX_LEN); // u
                TGPI_SH->register_input(BOB, GraphNode::VERTEX_LEN); // v
                TGPI_SH->register_input(BOB, 1); // isV
            }    
        }
        else{ // BOB
            int64_t tmp = 0;
            for(int i = 0; i < inputLength / 2; i++){
                TGPI_SH->register_input(ALICE, GraphNode::VERTEX_LEN); // u
                TGPI_SH->register_input(ALICE, GraphNode::VERTEX_LEN); // v
                TGPI_SH->register_input(ALICE, 1); // isV
            }
            for(int i = inputLength / 2; i < inputLength; i++){
                fin >> tmp;
                // cout << tmp;
                TGPI_SH->register_input(BOB, GraphNode::VERTEX_LEN, tmp); // u
                fin >> tmp;
                // cout << tmp;
                TGPI_SH->register_input(BOB, GraphNode::VERTEX_LEN, tmp); // v
                fin >> tmp;
                // cout << tmp;
                TGPI_SH->register_input(BOB, 1, tmp); // isV
            }
        }
        TGPI_SH->gen_input_labels();
        for(int i = 0; i < inputLength; i++){
            TGPI_SH->retrieve_input_labels(nodes[i]->u, (i < inputLength / 2) ? ALICE : BOB, GraphNode::VERTEX_LEN);
            TGPI_SH->retrieve_input_labels(nodes[i]->v, (i < inputLength / 2) ? ALICE : BOB, GraphNode::VERTEX_LEN);
            TGPI_SH->retrieve_input_labels(nodes[i]->isVertex, (i < inputLength / 2) ? ALICE : BOB, 1);
        }
        cout << "done read in" << endl;

		// business logic
		// set initial pagerank
		(new SetInitialPageRankGadget(TGPI_SH, machine))
            ->setInputs()
            ->secureCompute(nodes);
        cout << "done set initial pagerank" << endl;
        // print(machine->garblerId, TGPI_SH, nodes);

        auto identityNode = nodes[0]->alloc_obj()->set_zero(TGPI_SH);
		// 1. Compute number of neighbors for each vertex
		(new GatherFromEdges(TGPI_SH, machine, false/* isEdgeIncoming */, false/* useRevealSort */, identityNode, 
                [](GraphNode* agg, GraphNode* b, TinyGarblePI_SH* TGPI_SH){ // aggFunc
                    auto prAgg = dynamic_cast<PageRankNode*>(agg);
                    auto prB = dynamic_cast<PageRankNode*>(b);
                    auto ret = dynamic_cast<PageRankNode*>(prAgg->alloc_obj()->set_zero(TGPI_SH));
                    TGPI_SH->add(ret->l, prAgg->l, prB->l, PageRankNode::WIDTH);
                    // cout << " agg ret u: " << TGPI_SH->reveal(ret->u, GraphNode::VERTEX_LEN) << ", v: " << TGPI_SH->reveal(ret->v, GraphNode::VERTEX_LEN) << ", isV: " << TGPI_SH->reveal(ret->isVertex, 1, false) << ", l: " << TGPI_SH->reveal(ret->l, PageRankNode::WIDTH) << endl;
                    return (GraphNode*) ret;
                },
                [](GraphNode* agg, GraphNode* b, TinyGarblePI_SH* TGPI_SH){ // writeToVertex
                    auto prAgg = dynamic_cast<PageRankNode*>(agg);
                    auto prB = dynamic_cast<PageRankNode*>(b);
                    TGPI_SH->ifelse(prB->l, prB->isVertex, prAgg->l, prB->l, PageRankNode::WIDTH);
                }))
        ->setInputs()
        ->secureCompute(nodes);
        cout << "done step 1" << endl;
        
        auto randomShuffle = TGPI_SH->TG_int(1, PermuteGadget::requiredSwitchControlsLen(nodes.size(), machine));
        auto permuteGadget = (new PermuteGadget(machine, TGPI_SH));
        // input -> shuffle A
        PermuteGadget::genRandomShuffle(randomShuffle, TGPI_SH);
        permuteGadget->setInputs(randomShuffle)->secureCompute(nodes);
        PermuteGadget::genRandomShuffle(randomShuffle, TGPI_SH);

        auto gatherGadget = (new GatherFromEdges(TGPI_SH, machine, true /* isEdgeIncoming */, true/* useRevealSort */, identityNode,
                                [](GraphNode* agg, GraphNode* b, TinyGarblePI_SH* TGPI_SH){ // aggFunc
                                    auto prAgg = dynamic_cast<PageRankNode*>(agg);
                                    auto prB = dynamic_cast<PageRankNode*>(b);
                                    auto ret = dynamic_cast<PageRankNode*>(prAgg->alloc_obj()->set_zero(TGPI_SH));
                                    TGPI_SH->add(ret->pr, prAgg->pr, prB->pr, PageRankNode::WIDTH);
                                    return (GraphNode*)ret;
                                },
                                [](GraphNode* agg, GraphNode* b, TinyGarblePI_SH* TGPI_SH){ // writeToVertex
                                    auto prAgg = dynamic_cast<PageRankNode*>(agg);
                                    auto prB = dynamic_cast<PageRankNode*>(b);
                                    TGPI_SH->ifelse(prB->pr, prB->isVertex, prAgg->pr, prB->pr, PageRankNode::WIDTH);
                                }));
        auto scatterGadget = (new ScatterToEdges(TGPI_SH, machine, false /* isEdgeIncoming */, true/* useRevealSort */,
                                [](GraphNode* vertexNode, GraphNode* edgeNode, block* isVertex, TinyGarblePI_SH* TGPI_SH){ // writeToEdge
                                    auto prV = dynamic_cast<PageRankNode*>(vertexNode);
                                    auto prE = dynamic_cast<PageRankNode*>(edgeNode);
                                    auto div = TGPI_SH->TG_int(PageRankNode::WIDTH);
                                    TGPI_SH->div(div, prV->pr, prV->pr, PageRankNode::WIDTH);
                                    TGPI_SH->ifelse(prE->pr, isVertex, prE->pr, div, PageRankNode::WIDTH);
                                    TGPI_SH->clear_TG_int(div);
                                }));
		for (int i = 0; i < ITERATIONS; i++){
            // auto start_iter = clock();
            auto startIter = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count();
		

			// 2. Write weighted PR to edges
            // input: shuffle A
            // output: shuffle A
			scatterGadget->setInputs()->secureCompute(nodes);
            auto scatterTime = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count() - startIter;
            
            // shuffle A -> shuffle B
            permuteGadget->setInputs(randomShuffle)->secureCompute(nodes);
            auto permuteTime = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count() - startIter - scatterTime;

			// 3. Compute PR based on edges
            // input: shuffle B
            // output: shuffle B
			gatherGadget->setInputs()->secureCompute(nodes);

            // shuffle B -> shuffle A
            permuteGadget->setInputs(randomShuffle, true/* isReverse */)->secureCompute(nodes);

            // auto end_iter = clock();
            auto iterTime = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count() - startIter;
            // cout << "done iter " << i << " in: " << double(end_iter - start_iter) / CLOCKS_PER_SEC << "s" << endl;
            cout << "done iter " << i << " in: " << iterTime << "ms, scatterTime: " << scatterTime << "ms, permuteTime: " << permuteTime << "ms\n" << endl;
        }
        
		(new SortGadget(machine, TGPI_SH))
        ->setInputs(PageRankNode::allVerticesFirst(TGPI_SH))
        ->secureCompute(nodes);

        auto end = clock();
        cout << "time = " << double(end - start) / CLOCKS_PER_SEC << "s" << endl;
    // TODO: free prNodes
	}

    private:
    void print(int garblerId, TinyGarblePI_SH* TGPI_SH, const vector<GraphNode*>& nodes, int iterations=-1) {
        // if (garblerId == 0 && TGPI_SH->party == ALICE) {
        if (true) {
            cout << "PageRank of vertices after " << iterations <<" iteration(s):" << endl;
            cout << "total nodes: " << nodes.size() << endl;
            for (int i = 0; i < nodes.size(); i++) {
                auto prNode = dynamic_cast<PageRankNode*>(nodes[i]);
                // cout << "gId: " << machine->garblerId << ", u: " << TGPI_SH->reveal(prNode->u, GraphNode::VERTEX_LEN) << ", v: " << TGPI_SH->reveal(prNode->v, GraphNode::VERTEX_LEN) << ", isV: " << TGPI_SH->reveal(prNode->isVertex, 1, false) << endl;
                cout << "gId: " << machine->garblerId << ", u: " << TGPI_SH->reveal(prNode->u, GraphNode::VERTEX_LEN) << ", v: " << TGPI_SH->reveal(prNode->v, GraphNode::VERTEX_LEN) << ", isV: " << TGPI_SH->reveal(prNode->isVertex, 1, false) << ", pr: " << TGPI_SH->reveal(prNode->pr, PageRankNode::WIDTH) << ", l: " << TGPI_SH->reveal(prNode->l, PageRankNode::WIDTH) << endl;
            }
		}
	}

};
int main(int argc, char** argv){
    Machine* machine = new Machine(argc, argv);
    auto gadget = new PageRank(machine->TGPI_SH, machine);
    gadget->secureCompute();
    delete gadget;
    delete machine;
    cout << "all deleted" << endl;
    // auto node = new PageRankNode();
    // auto g = new SetInitialPageRankGadget(machine->TGPI_SH, machine);
    return 0;
}

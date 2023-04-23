#ifndef UTILS_H
#define UTILS_H
#include <vector>
#include <emp-tool/emp-tool.h>
using namespace std;

template <class T>
vector<T> sendReceive(NetIO* channel, const vector<T>& data, bool up, TinyGarblePI_SH* TGPI_SH){
    auto a = vector<T>(data.size());
    int toTransfer = data.size();
    int i = 0, j = 0;
    while (toTransfer > 0) {
        int curTransfer = min(toTransfer, 8);
        toTransfer -= curTransfer;
        if(up){
            for (int k = 0; k < curTransfer; k++, i++) {
                channel->send_data(&data[i], sizeof(T));
            }
            channel->flush();
            for (int k = 0; k < curTransfer; k++, j++) {
                channel->recv_data(&a[j], sizeof(T));
            }
        }
        else{
            for (int k = 0; k < curTransfer; k++, j++) {
                channel->recv_data(&a[j], sizeof(T));
            }
            for (int k = 0; k < curTransfer; k++, i++) {
                channel->send_data(&data[i], sizeof(T));
            }
            channel->flush();
        }
        
    }
    return a;
}

template <>
vector<GraphNode*> sendReceive<GraphNode*>(NetIO* channel, const vector<GraphNode*>& nodes, bool up, TinyGarblePI_SH* TGPI_SH){
    auto a = vector<GraphNode*>(nodes.size());
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
            for (int k = 0; k < curTransfer; k++, j++) {
                a[j]->read(channel, TGPI_SH);
            }
        }
        else{
            for (int k = 0; k < curTransfer; k++, j++) {
                a[j]->read(channel, TGPI_SH);
            }
            for (int k = 0; k < curTransfer; k++, i++) {
                nodes[i]->send(channel, TGPI_SH);
            }
            channel->flush();
        }
        
    }
    return a;
}

template <class T>
vector<T> concatenate(const vector<T>& A, const vector<T>& B) {
    auto C = vector<T>();
    C.insert(C.end(), A.begin(), A.end());
    C.insert(C.end(), B.begin(), B.end());
    return C;
}

#endif

#include <emp-tool/emp-tool.h>
#include <boost/program_options.hpp>
#include <boost/format.hpp>
#include <string>
#include <unistd.h>
using namespace std;
namespace po = boost::program_options;

int main(int argc, char **argv){
    int id;
    
    po::options_description desc{"Test options"};
    desc.add_options()  //
    ("id", po::value<int>(&id)->default_value(0), "thread id");
    
    po::variables_map vm;
    try {
        po::parsed_options parsed = po::command_line_parser(argc, argv).options(desc).allow_unregistered().run();
        po::store(parsed, vm);
        po::notify(vm);
    }catch (po::error& e) {
        cout << "ERROR: " << e.what() << endl << endl;
        cout << desc << endl;
        exit(-1);
    }

    int dstPort = 50000;
    string dstIP = "10.0.0.1";
    if(id == 1){
        for (int i = 0; i < 2; i++) {
            cout << "listening on port: " << dstPort << endl;
            NetIO* io = new NetIO(nullptr, dstPort, false); // connect as server(char *addr=nullptr)
            io->set_nodelay();
            io->send_data(&id, sizeof(int));
            io->flush();
            cout << "id sent: " << id << endl;
            int recv;
            io->recv_data(&recv, sizeof(int));
            cout << "id received: " << recv << endl;
            sleep(5);
            delete io;
        }
    }
    else if (id == 2 || id == 3){
        cout << "connecting to " << dstIP << " on port: " << dstPort << endl;
        NetIO* io = new NetIO(dstIP.c_str(), dstPort, false); // connect to dstIP as client
        io->set_nodelay();
        int recv;
        io->recv_data(&recv, sizeof(int));
        cout << "id received: " << recv << endl;
        io->send_data(&id, sizeof(int));
        io->flush();
        cout << "id sent: " << id << endl;
        sleep(5); // compute
        delete io;
    }

    return 0;
}

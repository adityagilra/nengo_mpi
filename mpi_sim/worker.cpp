#include <iostream>
#include <string>
#include <memory>

#include <mpi.h>

#include "optionparser.h"

#include "chunk.hpp"
#include "simulator.hpp"
#include "utils.hpp"

using namespace std;

// This file can be used in two ways. First, if using nengo_mpi from python, this
// code is the entry point for the workers that are spawned by the initial process.
// If using nengo_mpi straight from C++, then this file is the entry point for ALL
// processes in the simulation. In that case, the process with rank 0 will create an
// MpiSimulator object and load a built nengo network from a file specified on the
// command line, and the rest of the processes will jump straight into start_worker.

// comm: The communicator for the worker to communicate on. Must
// be an intracommunicator involving all processes, with the master
// process having rank 0.
void start_worker(MPI_Comm comm){

    int my_id, num_procs;
    MPI_Comm_rank(comm, &my_id);
    MPI_Comm_size(comm, &num_procs);

    int buflen = 512;
    char name[buflen];
    MPI_Get_processor_name(name, &buflen);

    dbg("Hello world! I'm a nengo_mpi worker process with "
        "rank "<< my_id << " on host " << name << "." << endl);

    MPI_Status status;

    string chunk_label = recv_string(0, setup_tag, comm);

    dtype dt = recv_dtype(0, setup_tag, comm);

    MpiSimulatorChunk chunk(my_id, chunk_label, dt);

    int s = 0;
    string op_string;

    key_type probe_key;
    string signal_string;
    dtype period;

    dbg("Worker " << my_id  << " receiving network...");

    while(1){
        s = recv_int(0, setup_tag, comm);

        if(s == add_signal_flag){
            dbg("Worker " << my_id  << " receiving signal.");

            key_type key = recv_key(0, setup_tag, comm);

            string label = recv_string(0, setup_tag, comm);

            unique_ptr<BaseSignal> data = recv_matrix(0, setup_tag, comm);

            dbg("Worker " << my_id  << " done receiving signal.");

            dbg("key; " << key);
            dbg("label; " << key);
            dbg("data; " << *data);

            chunk.add_base_signal(key, label, move(data));

        }else if(s == add_op_flag){
            dbg("Worker " << my_id  << " receiving operator.");

            string op_string = recv_string(0, setup_tag, comm);

            dbg("Worker " << my_id  << " done receiving operator.");

            chunk.add_op(op_string);

        }else if(s == add_probe_flag){
            dbg("Worker " << my_id  << " receiving probe.");

            key_type probe_key = recv_key(0, setup_tag, comm);

            string signal_string = recv_string(0, setup_tag, comm);

            dtype period = recv_dtype(0, setup_tag, comm);

            dbg("Worker " << my_id  << " done receiving probe.");

            chunk.add_probe(probe_key, signal_string, period);

        }else if(s == stop_flag){
            dbg("Worker " << my_id  << " done receiving network.");
            break;

        }else{
            throw runtime_error("Worker received invalid flag from master.");
        }
    }

    dbg("Worker " << my_id << " setting up simulation log...");
    auto sim_log = unique_ptr<SimulationLog>(
        new ParallelSimulationLog(num_procs, my_id, dt, comm));

    chunk.set_simulation_log(move(sim_log));

    dbg("Worker " << my_id << " setting up MPI operators...");
    chunk.set_communicator(comm);
    chunk.add_op(unique_ptr<Operator>(new MPIBarrier(comm)));

    dbg("Worker " << my_id << " waiting for signal to start simulation...");

    int steps;
    MPI_Bcast(&steps, 1, MPI_INT, 0, comm);

    dbg("Worker " << my_id << " got the signal to start simulation: "
        << steps << " steps." << endl);

    chunk.run_n_steps(steps, false);

    MPI_Barrier(comm);

    if(!chunk.is_logging()){
        for(auto& pair : chunk.probe_map){
            key_type key = pair.first;
            shared_ptr<Probe>& probe = pair.second;

            send_key(key, 0, probe_tag, comm);

            vector<unique_ptr<BaseSignal>> probe_data = probe->harvest_data();

            send_int(probe_data.size(), 0, probe_tag, comm);

            for(auto& pd : probe_data){
                send_matrix(move(pd), 0, probe_tag, comm);
            }
        }
    }

    MPI_Barrier(comm);

    chunk.close_simulation_log();

    MPI_Finalize();
}

enum  optionIndex { UNKNOWN, HELP, PROC, LOG, NET, PROGRESS, TIME};

const option::Descriptor usage[] =
{
 {UNKNOWN, 0, "" , "", option::Arg::None,           "USAGE: example [options]\n\n"
                                                    "Options:" },
 {HELP,    0, "" , "help", option::Arg::None,       "  --help  \tPrint usage and exit." },
 {PROC,    0, "p", "proc", option::Arg::None, "  --proc, -p  \tNumber of processors to use." },
 {LOG,    0, "", "log", option::Arg::None, "  --log,  \tName of file to log results to." },
 {NET,    0, "", "net", option::Arg::None, "  --net,  \tName of network to simulate." },
 {PROGRESS,    0, "", "progress", option::Arg::None, "  --progress, \tSupply to show progress bar." },
 {TIME,    0, "t", "time", option::Arg::None, "  --time, -t  \tTime to simulate for, in seconds." },
 {UNKNOWN, 0, "" ,  ""   , option::Arg::None,       "\nExamples:\n"
                                                    "  example --unknown -- --this_is_no_option\n"
                                                    "  example -unk --plus -ppp file1 file2\n" },
 {0,0,0,0,0,0}
};

void start_master(int argc, char **argv){

    argc -= (argc > 0); argv += (argc > 0); // skip program name argv[0] if present
    option::Stats  stats(usage, argc, argv);
    option::Option options[stats.options_max], buffer[stats.buffer_max];
    option::Parser parse(usage, argc, argv, options, buffer);

    if (parse.error()){
        return;
    }

    if (options[HELP] || argc == 0) {
      option::printUsage(std::cout, usage);
        return;
    }

    if(!options[NET]){
        cout << "Please network to simulate." << endl;
        return;
    }

    string net_filename = options[NET].arg;
    cout << "Loading network from file: " << net_filename << "." << endl;

    if(!options[TIME]){
        cout << "Please specify a simulation length" << endl;
        return;
    }

    float sim_length = boost::lexical_cast<float>(options[TIME].arg);
    cout << "Will run simulation for " << sim_length << " second(s)." << endl;

    bool show_progress = bool(options[PROGRESS]);

    string log_filename = options[LOG] ? options[LOG].arg : "";

    int num_processors = options[PROC] ? boost::lexical_cast<int>(options[PROC].arg) : 0;

    cout << "Building network..." << endl;
    unique_ptr<Simulator> sim = create_simulator_from_file(net_filename);
    cout << "Done building network..." << endl;

    cout << "dt: " << sim->dt << endl;
    int num_steps = int(sim_length / sim->dt);

    cout << "Num steps: " << num_steps << endl;
    cout << "Running simulation..." << endl;
    if(log_filename.size() != 0){
        cout << "Logging simulation results to " << log_filename << endl;
    }

    sim->run_n_steps(num_steps, show_progress, log_filename);

    if(log_filename.size() == 0){
        for(auto& key : sim->get_probe_keys()){
            vector<unique_ptr<BaseSignal>> probe_data = sim->get_probe_data(key);

            cout << "Probe data for key: " << key << endl;

            for(auto& pd : probe_data){
                cout << *pd << endl;
            }
        }
    }
}

int main(int argc, char **argv){

    MPI_Init(&argc, &argv);

    MPI_Comm parent;
    MPI_Comm_get_parent(&parent);

    if (parent != MPI_COMM_NULL){
        MPI_Comm everyone;

        // We have a parent, so get a communicator that includes the
        // parent and all children
        MPI_Intercomm_merge(parent, true, &everyone);
        start_worker(everyone);
    }else{
        int mpi_size;
        MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);

        if(mpi_size == 1){
            // No parent, and only one process. Corresponds to a serial
            // simulation using C++ directly (skipping python).
            MPI_Finalize();

            start_master(argc, argv);
            return 0;
        }

        int rank;
        MPI_Comm_rank(MPI_COMM_WORLD, &rank);

        if(rank == 0){
            start_master(argc, argv);
        }else{
            start_worker(MPI_COMM_WORLD);
        }
    }

    return 0;
}
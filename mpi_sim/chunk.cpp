#include "chunk.hpp"

MpiSimulatorChunk::MpiSimulatorChunk()
    :time(0.0), dt(0.001), n_steps(0) {
}

MpiSimulatorChunk::MpiSimulatorChunk(double dt)
    :time(0.0), dt(dt), n_steps(0){
}

void MpiSimulatorChunk::run_n_steps(int steps){

    for(unsigned step = 0; step < steps; ++step){
        list<Operator*>::const_iterator it;
        for(it = operator_list.begin(); it != operator_list.end(); ++it){
            //Call the operator
            (**it)();
        }

        map<key_type, Probe<Vector>*>::iterator probe_it;
        for(probe_it = probe_map.begin(); probe_it != probe_map.end(); ++probe_it){
            //Call the operator
            probe_it->second->gather(n_steps);
        }

        time += dt;
        n_steps++;
    }
}

void MpiSimulatorChunk::add_operator(Operator *op){
    operator_list.push_back(op);
}

void MpiSimulatorChunk::add_mpi_send(MPISend* mpi_send){
    operator_list.push_back(mpi_send);
    mpi_send->set_waiter(find_wait(mpi_send->tag));
}

void MpiSimulatorChunk::add_mpi_recv(MPIRecv* mpi_recv){
    operator_list.push_back(mpi_recv);
    mpi_recv->set_waiter(find_wait(mpi_recv->tag));
}

void MpiSimulatorChunk::add_wait(MPIWait* mpi_wait){
    operator_list.push_back(mpi_wait);
    mpi_waits.push_back(mpi_wait);
}

void MpiSimulatorChunk::add_probe(key_type key, Probe<Vector>* probe){
    probe_map[key] = probe;
}

void MpiSimulatorChunk::add_vector_signal(key_type key, Vector* sig){
    vector_signal_map[key] = sig;
}

void MpiSimulatorChunk::add_matrix_signal(key_type key, Matrix* sig){
    matrix_signal_map[key] = sig;
}

MPIWait* MpiSimulatorChunk::find_wait(int tag){
    list<MPIWait*>::const_iterator it;
    for(it = mpi_waits.begin(); it != mpi_waits.end(); ++it){
        if ((*it)->tag == tag){
            return *it;
        }
    }

    stringstream error;
    error << "MPIWait object with tag " << tag << " does not exist.";
    throw invalid_argument(error.str());
}

Probe<Vector>* MpiSimulatorChunk::get_probe(key_type key){
    try{
        Probe<Vector>* probe = probe_map.at(key);
        return probe;
    }catch(const out_of_range& e){
        cerr << "Error accessing MpiSimulatorChunk :: probe with key " << key << endl;
        throw e;
    }
}

Vector* MpiSimulatorChunk::get_vector_signal(key_type key){
    try{
        Vector* vec = vector_signal_map.at(key);
        return vec;
    }catch(const out_of_range& e){
        cerr << "Error accessing MpiSimulatorChunk :: vector signal with key " << key << endl;
        throw e;
    }
}

Matrix* MpiSimulatorChunk::get_matrix_signal(key_type key){
    try{
        Matrix* mat = matrix_signal_map.at(key);
        return mat;
    }catch(const out_of_range& e){
        cerr << "Error accessing MpiSimulatorChunk :: matrix signal with key " << key << endl;
        throw e;
    }
}

string MpiSimulatorChunk::to_string() const{
    stringstream out;

    out << "<MpiSimulatorChunk" << endl;

    map<key_type, Vector*>::const_iterator vector_it = vector_signal_map.begin();

    out << "** Vectors: **" << endl;
    for(; vector_it != vector_signal_map.end(); vector_it++){
        out << "Key: " << vector_it->first << endl;
        out << "Vector: " << *(vector_it->second) << endl;
    }

    out << endl;

    map<key_type, Matrix*>::const_iterator matrix_it = matrix_signal_map.begin();

    out << "** Matrices: **" << endl;
    for(; matrix_it != matrix_signal_map.end(); matrix_it++){
        out << "Key: " << matrix_it->first << endl;
        out << "Matrix: " << *(matrix_it->second) << endl;
    }
    out << endl;

    map<key_type, Probe<Vector>*>::const_iterator probe_it = probe_map.begin();

    out << "** Probes: **" << endl;
    for(; probe_it != probe_map.end(); probe_it++){
        out << "Key: " << probe_it->first << endl;
        out << "Probe: " << *(probe_it->second) << endl;
    }
    out << endl;

    list<Operator*>::const_iterator it;

    out << "** Operators: **" << endl;
    for(it = operator_list.begin(); it != operator_list.end(); ++it){
        out << (**it) << endl;
    }
    out << endl;

    return out.str();
}

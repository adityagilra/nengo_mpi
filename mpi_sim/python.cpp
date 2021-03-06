#include "python.hpp"

// Note: on the python side, these are renamed to have `python` removed.
void python_mpi_init(){
    mpi_init();
}

void python_mpi_finalize(){
    mpi_finalize();
}

int python_get_mpi_rank(){
    return get_mpi_rank();
}

int python_get_mpi_n_procs(){
    return get_mpi_n_procs();
}

void python_kill_workers(){
    return kill_workers();
}

void python_worker_start(){
    worker_start();
}

bool hasattr(bpy::object obj, string const &attrName) {
      return PyObject_HasAttrString(obj.ptr(), attrName.c_str());
}

unique_ptr<BaseSignal> ndarray_to_matrix(bpyn::array a){

    int ndim = bpy::extract<int>(a.attr("ndim"));

    if (ndim == 1){

        int size = bpy::extract<int>(a.attr("size"));
        auto ret = unique_ptr<BaseSignal>(new BaseSignal(size, 1));
        for(unsigned i = 0; i < size; i++){
            (*ret)(i, 0) = bpy::extract<dtype>(a[i]);
        }

        return ret;

    }else{
        int size = bpy::extract<int>(a.attr("size"));
        vector<int> shape(ndim);
        vector<int> strides(ndim);
        bpy::object python_shape = a.attr("shape");
        bpy::object python_strides = a.attr("strides");

        for(unsigned i = 0; i < ndim; i++){
            shape[i] = bpy::extract<int>(python_shape[i]);
            strides[i] = bpy::extract<int>(python_strides[i]);
        }

        auto ret = unique_ptr<BaseSignal>(new BaseSignal(shape[0], shape[1]));
        for(unsigned i = 0; i < shape[0]; i++){
            for(unsigned j = 0; j < shape[1]; j++){
                (*ret)(i, j) = bpy::extract<dtype>(a[i][j]);
            }
        }

        return ret;
    }
}

unique_ptr<BaseSignal> list_to_matrix(bpy::list l){

    int length = bpy::len(l);
    auto ret = unique_ptr<BaseSignal>(new BaseSignal(length, 1));
    for(unsigned i = 0; i < length; i++){
        (*ret)(i, 0) = bpy::extract<dtype>(l[i]);
    }

    return ret;
}

PythonMpiSimulator::PythonMpiSimulator(){
    if(n_processors_available == 1){
        sim = unique_ptr<Simulator>(new Simulator(false));
    }else{
        sim = unique_ptr<Simulator>(new MpiSimulator(false, false));
    }
}

void PythonMpiSimulator::load_network(bpy::object filename){
    string c_filename = bpy::extract<string>(filename);
    sim->from_file(c_filename);
}

void PythonMpiSimulator::finalize_build(){
    sim->finalize_build();
}

void PythonMpiSimulator::run_n_steps(
        bpy::object pysteps, bpy::object progress, bpy::object log_filename){

    int c_steps = bpy::extract<int>(pysteps);
    bool c_progress = bpy::extract<bool>(progress);
    string c_log_filename = bpy::extract<string>(log_filename);

    sim->run_n_steps(c_steps, c_progress, c_log_filename);
}

bpy::object PythonMpiSimulator::get_probe_data(
        bpy::object probe_key, bpy::object make_array){

    key_type c_probe_key = bpy::extract<key_type>(probe_key);
    vector<unique_ptr<BaseSignal>> data = sim->get_probe_data(c_probe_key);

    bpy::list py_list;

    for(auto& d: data){

        bpy::object a;

        if(d->size1() == 1){
            a = make_array(d->size2());
            for(unsigned i=0; i < d->size2(); ++i){
                a[i] = (*d)(0, i);
            }

        }else if(d->size2() == 1){
            a = make_array(d->size1());
            for(unsigned i=0; i < d->size1(); ++i){
                a[i] = (*d)(i, 0);
            }

        }else{
            a = make_array(bpy::make_tuple(d->size1(), d->size2()));
            for(unsigned i=0; i < d->size1(); ++i){
                for(unsigned j=0; j < d->size2(); ++j){
                    a[i][j] = (*d)(i, j);
                }
            }
        }

        py_list.append(a);
    }

    return py_list;
}

void PythonMpiSimulator::reset(bpy::object seed){
    unsigned c_seed = bpy::extract<unsigned>(seed);
    sim->reset(c_seed);
}

void PythonMpiSimulator::close(){
    sim->close();
}

void PythonMpiSimulator::create_PyFunc(
        bpy::object py_fn, bpy::object t_in, bpy::object index){

    bool c_t_in = bpy::extract<bool>(t_in);
    dtype* time_pointer = c_t_in ? sim->get_time_pointer() : NULL;

    auto pyfunc = unique_ptr<Operator>(new PyFunc(py_fn, time_pointer));

    float c_index = bpy::extract<float>(index);
    pyfunc->set_index(c_index);

    sim->add_pyfunc(move(pyfunc));
}

void PythonMpiSimulator::create_PyFuncI(
        bpy::object py_fn, bpy::object t_in, bpy::object input,
        bpyn::array py_input, bpy::object index){

    string input_signal = bpy::extract<string>(input);

    build_dbg("Creating PyFuncI. Input signal: " << input_signal);
    SignalView input_mat = sim->get_signal(input_signal);

    bool c_t_in = bpy::extract<bool>(t_in);
    dtype* time_pointer = c_t_in ? sim->get_time_pointer() : NULL;

    auto pyfunc = unique_ptr<Operator>(
        new PyFunc(py_fn, time_pointer, input_mat, py_input));

    float c_index = bpy::extract<float>(index);
    pyfunc->set_index(c_index);

    sim->add_pyfunc(move(pyfunc));
}

void PythonMpiSimulator::create_PyFuncO(
        bpy::object py_fn, bpy::object t_in,
        bpy::object output, bpy::object index){

    string output_signal = bpy::extract<string>(output);

    build_dbg("Creating PyFuncO. Output signal: " << output_signal);
    SignalView output_mat = sim->get_signal(output_signal);

    bool c_t_in = bpy::extract<bool>(t_in);
    dtype* time_pointer = c_t_in ? sim->get_time_pointer() : NULL;

    auto pyfunc = unique_ptr<Operator>(new PyFunc(py_fn, time_pointer, output_mat));

    float c_index = bpy::extract<float>(index);
    pyfunc->set_index(c_index);

    sim->add_pyfunc(move(pyfunc));
}


void PythonMpiSimulator::create_PyFuncIO(
        bpy::object py_fn, bpy::object t_in, bpy::object input,
        bpyn::array py_input, bpy::object output, bpy::object index){

    build_dbg("Creating PyFuncIO.");

    string input_signal = bpy::extract<string>(input);
    build_dbg("Input signal: " << input_signal);
    SignalView input_mat = sim->get_signal(input_signal);

    string output_signal = bpy::extract<string>(output);
    build_dbg("Output signal: " << output_signal);
    SignalView output_mat = sim->get_signal(output_signal);

    bool c_t_in = bpy::extract<bool>(t_in);
    dtype* time_pointer = c_t_in ? sim->get_time_pointer() : NULL;

    auto pyfunc = unique_ptr<Operator>(
        new PyFunc(py_fn, time_pointer, input_mat, py_input, output_mat));

    float c_index = bpy::extract<float>(index);
    pyfunc->set_index(c_index);

    sim->add_pyfunc(move(pyfunc));
}

string PythonMpiSimulator::to_string() const{
    return sim->to_string();
}

PyFunc::PyFunc(bpy::object py_fn, dtype* time)
:py_fn(py_fn), time(time), supply_time(time!=NULL), supply_input(false),
get_output(false), input(null_matrix, null_slice, null_slice), py_input(0.0),
output(null_matrix, null_slice, null_slice){}

PyFunc::PyFunc(bpy::object py_fn, dtype* time, SignalView output)
:py_fn(py_fn), time(time), supply_time(time!=NULL), supply_input(false),
get_output(true), input(null_matrix, null_slice, null_slice),
py_input(0.0), output(output){}

PyFunc::PyFunc(bpy::object py_fn, dtype* time, SignalView input, bpyn::array py_input)
:py_fn(py_fn), time(time), supply_time(time!=NULL),
supply_input(true), get_output(false), input(input),
py_input(py_input), output(null_matrix, null_slice, null_slice){}

PyFunc::PyFunc(
    bpy::object py_fn, dtype* time, SignalView input,
    bpyn::array py_input, SignalView output)
:py_fn(py_fn), time(time), supply_time(time!=NULL), supply_input(true),
get_output(true), input(input), py_input(py_input), output(output){}

void PyFunc::operator() (){

    bpy::object py_output;
    if(supply_input){

        // TODO: currently assuming pyfunc only operate on vectors.
        for(unsigned i = 0; i < input.size1(); ++i){
            py_input[i] = input(i, 0);
        }

        if(supply_time){
            py_output = py_fn(*time, py_input);
        }else{
            py_output = py_fn(py_input);
        }
    }else{
        if(supply_time){
            py_output = py_fn(*time);
        }else{
            py_output = py_fn();
        }
    }

    if(get_output){
        if(hasattr(py_output, "ndim")){
            // TODO: currently assuming pyfunc only operate on vectors.
            for(unsigned i = 0; i < output.size1(); ++i){
                output(i, 0) = bpy::extract<dtype>(py_output[i]);
            }
        }else{
            output(0, 0) = bpy::extract<dtype>(py_output);
        }
    }

    run_dbg(*this);
}

string PyFunc::to_string() const{
    stringstream out;

    out << "PyFunc: " << endl;
    out << "Output: " << endl;
    out << output << endl << endl;

    return out.str();
}

BOOST_PYTHON_MODULE(mpi_sim)
{
    bpy::def("mpi_init", python_mpi_init);
    bpy::def("mpi_finalize", python_mpi_finalize);
    bpy::def("get_mpi_rank", python_get_mpi_rank);
    bpy::def("get_mpi_n_procs", python_get_mpi_n_procs);
    bpy::def("kill_workers", python_kill_workers);
    bpy::def("worker_start", python_worker_start);

    bpy::numeric::array::set_module_and_type("numpy", "ndarray");

    bpy::class_<PythonMpiSimulator, boost::noncopyable>(
            "MpiSimulator", bpy::init<>())
        .def("load_network", &PythonMpiSimulator::load_network)
        .def("finalize_build", &PythonMpiSimulator::finalize_build)
        .def("run_n_steps", &PythonMpiSimulator::run_n_steps)
        .def("get_probe_data", &PythonMpiSimulator::get_probe_data)
        .def("reset", &PythonMpiSimulator::reset)
        .def("close", &PythonMpiSimulator::close)
        .def("create_PyFunc", &PythonMpiSimulator::create_PyFunc)
        .def("create_PyFuncO", &PythonMpiSimulator::create_PyFuncO)
        .def("create_PyFuncI", &PythonMpiSimulator::create_PyFuncI)
        .def("create_PyFuncIO", &PythonMpiSimulator::create_PyFuncIO)
        .def("to_string", &PythonMpiSimulator::to_string);
}

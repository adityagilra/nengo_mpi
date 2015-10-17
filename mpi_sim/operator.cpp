#include "operator.hpp"

// Constructors
Reset::Reset(SignalView dst, dtype value)
:dst(dst), value(value){

    dummy = ScalarSignal(dst.size1(), dst.size2(), value);
}

Copy::Copy(SignalView dst, SignalView src)
:dst(dst), src(src){}

SlicedCopy::SlicedCopy(
    SignalView B, SignalView A, bool inc,
    int start_A, int stop_A, int step_A,
    int start_B, int stop_B, int step_B,
    vector<int> seq_A, vector<int> seq_B)
:B(B), A(A), inc(inc),
start_A(start_A), stop_A(stop_A), step_A(step_A),
start_B(start_B), stop_B(stop_B), step_B(step_B),
seq_A(seq_A), seq_B(seq_B){

    length_A = A.size1();
    length_B = B.size1();

    if(seq_A.size() > 0 && (start_A != 0 || stop_A != 0 || step_A != 0)){
        throw runtime_error(
            "While creating SlicedCopy, seq_A was non-empty, "
            "but one of start/step/stop was non-zero.");
    }

    if(seq_B.size() > 0 && (start_B != 0 || stop_B != 0 || step_B != 0)){
        throw runtime_error(
            "While creating SlicedCopy, seq_B was non-empty, "
            "but one of start/step/stop was non-zero.");
    }

    int n_assignments_A = 0;
    if(seq_A.size() > 0){
        n_assignments_A = seq_A.size();
    }else{
        if(step_A > 0 || step_A < 0){
            n_assignments_A = int(ceil(max((stop_A - start_A) / float(step_A), 0.0f)));
        }else{
            throw runtime_error("While creating SlicedCopy, step_A equal to 0.");
        }
    }

    int n_assignments_B = 0;
    if(seq_B.size() > 0){
        n_assignments_B = seq_B.size();
    }else{
        if(step_B > 0 || step_B < 0){
            n_assignments_B = int(ceil(max((stop_B - start_B) / float(step_B), 0.0f)));
        }else{
            throw runtime_error("While creating SlicedCopy, step_B equal to 0.");
        }
    }

    if(n_assignments_A != n_assignments_B){
        stringstream ss;
        ss << "While creating SlicedCopy, got mismatching slice sizes for A and B. "
           << "Size of A slice was " << n_assignments_A << ", while sice of B slice was "
           << n_assignments_B << "." << endl;
        throw runtime_error(ss.str());
    }

    n_assignments = n_assignments_A;
}

DotInc::DotInc(SignalView A, SignalView X, SignalView Y)
:A(A), X(X), Y(Y){}

ElementwiseInc::ElementwiseInc(SignalView A, SignalView X, SignalView Y)
:A(A), X(X), Y(Y){

    if(A.size1() != Y.size1() || A.size2() != Y.size2() ||
       X.size1() != Y.size1() || X.size2() != Y.size2()){
        broadcast = true;
        A_row_stride = A.size1() > 1 ? 1 : 0;
        A_col_stride = A.size2() > 1 ? 1 : 0;

        X_row_stride = X.size1() > 1 ? 1 : 0;
        X_col_stride = X.size2() > 1 ? 1 : 0;
    }else{
        broadcast = false;
        A_row_stride = 1;
        A_col_stride = 1;

        X_row_stride = 1;
        X_col_stride = 1;

    }
}

NoDenSynapse::NoDenSynapse(
    SignalView input, SignalView output, dtype b)
:input(input), output(output), b(b){}

SimpleSynapse::SimpleSynapse(
    SignalView input, SignalView output, dtype a, dtype b)
:input(input), output(output), a(a), b(b){}

Synapse::Synapse(
    SignalView input, SignalView output, BaseSignal numer, BaseSignal denom)
:input(input), output(output), numer(numer), denom(denom){

    for(int i = 0; i < input.size1(); i++){
        x.push_back(boost::circular_buffer<dtype>(numer.size1()));
        y.push_back(boost::circular_buffer<dtype>(denom.size1()));
    }
}

WhiteNoise::WhiteNoise(
    SignalView output, dtype mean, dtype std, bool do_scale, bool inc, dtype dt)
:output(output), mean(mean), std(std), dist(mean, std),
do_scale(do_scale), inc(inc), dt(dt){
    alpha = do_scale ? 1.0 / dt : 1.0;
}

WhiteSignal::WhiteSignal(SignalView output, BaseSignal coefs)
:output(output), coefs(coefs), idx(0){}

LIF::LIF(
    int n_neurons, dtype tau_rc, dtype tau_ref, dtype min_voltage,
    dtype dt, SignalView J, SignalView output, SignalView voltage,
    SignalView ref_time)
:n_neurons(n_neurons), dt(dt), tau_rc(tau_rc), tau_ref(tau_ref),
min_voltage(min_voltage), dt_inv(1.0 / dt), J(J), output(output),
voltage(voltage), ref_time(ref_time){

    one = ScalarSignal(n_neurons, 1, 1.0);
    dt_vec = ScalarSignal(n_neurons, 1, dt);
}

LIFRate::LIFRate(
    int n_neurons, dtype tau_rc, dtype tau_ref, SignalView J, SignalView output)
:n_neurons(n_neurons), tau_rc(tau_rc), tau_ref(tau_ref), J(J), output(output){}

AdaptiveLIF::AdaptiveLIF(
    int n_neurons, dtype tau_n, dtype inc_n, dtype tau_rc, dtype tau_ref,
    dtype min_voltage, dtype dt, SignalView J, SignalView output, SignalView voltage,
    SignalView ref_time, SignalView adaptation)
:LIF(n_neurons, tau_rc, tau_ref, min_voltage, dt, J, output, voltage, ref_time),
tau_n(tau_n), inc_n(inc_n), adaptation(adaptation){}

AdaptiveLIFRate::AdaptiveLIFRate(
    int n_neurons, dtype tau_n, dtype inc_n, dtype tau_rc, dtype tau_ref, dtype dt,
    SignalView J, SignalView output, SignalView adaptation)
:LIFRate(n_neurons, tau_rc, tau_ref, J, output),
tau_n(tau_n), inc_n(inc_n), dt(dt), adaptation(adaptation){}

RectifiedLinear::RectifiedLinear(int n_neurons, SignalView J, SignalView output)
:n_neurons(n_neurons), J(J), output(output){}

Sigmoid::Sigmoid(int n_neurons, dtype tau_ref, SignalView J, SignalView output)
:n_neurons(n_neurons), tau_ref(tau_ref), tau_ref_inv(1.0 / tau_ref), J(J), output(output){}

Izhikevich::Izhikevich(
    int n_neurons, dtype tau_recovery, dtype coupling, dtype reset_voltage,
    dtype reset_recovery, dtype dt, SignalView J, SignalView output,
    SignalView voltage, SignalView recovery)
:n_neurons(n_neurons), tau_recovery(tau_recovery), coupling(coupling),
reset_voltage(reset_voltage), reset_recovery(reset_recovery), dt(dt), dt_inv(1.0/dt),
J(J), output(output), voltage(voltage), recovery(recovery){

    bias = ScalarSignal(n_neurons, 1, 140);
}

// Function operator overloads
void Reset::operator() (){

    dst = dummy;

    run_dbg(*this);
}

void Copy::operator() (){

    dst = src;

    run_dbg(*this);
}

void SlicedCopy::operator() (){
    if(seq_A.size() > 0 && seq_B.size() > 0){
        if(inc){
            for(int i = 0; i < n_assignments; i++){
                B(seq_B[i] % length_B, 0) += A(seq_A[i] % length_A, 0);
            }
        }else{
            for(int i = 0; i < n_assignments; i++){
                B(seq_B[i] % length_B, 0) = A(seq_A[i] % length_A, 0);
            }
        }

    }else if(seq_A.size() > 0){
        int index_B = start_B;
        if(inc){
            for(int i = 0; i < n_assignments; i++){
                B(index_B % length_B, 0) += A(seq_A[i] % length_A, 0);
                index_B += step_B;
            }
        }else{
            for(int i = 0; i < n_assignments; i++){
                B(index_B % length_B, 0) = A(seq_A[i] % length_A, 0);
                index_B += step_B;
            }
        }

    }else if(seq_B.size() > 0){
        int index_A = start_A;
        if(inc){
            for(int i = 0; i < n_assignments; i++){
                B(seq_B[i] % length_B, 0) += A(index_A % length_A, 0);
                index_A += step_A;
            }
        }else{
            for(int i = 0; i < n_assignments; i++){
                B(seq_B[i] % length_B, 0) = A(index_A % length_A, 0);
                index_A += step_A;
            }
        }

    }else{
        int index_A = start_A, index_B = start_B;
        if(inc){
            for(int i = 0; i < n_assignments; i++){
                B(index_B % length_B, 0) += A(index_A % length_A, 0);
                index_A += step_A;
                index_B += step_B;
            }
        }else{
            for(int i = 0; i < n_assignments; i++){
                B(index_B % length_B, 0) = A(index_A % length_A, 0);
                index_A += step_A;
                index_B += step_B;
            }
        }

    }

    run_dbg(*this);
}

void DotInc::operator() (){
    axpy_prod(A, X, Y, false);

    run_dbg(*this);
}

void ElementwiseInc::operator() (){
    if(broadcast){
        int A_i = 0, A_j = 0, X_i = 0, X_j = 0;

        for(int Y_i = 0; Y_i < Y.size1(); Y_i++){
            A_j = 0;
            X_j = 0;

            for(int Y_j = 0;Y_j < Y.size2(); Y_j++){
                Y(Y_i, Y_j) += A(A_i, A_j) * X(X_i, X_j);
                A_j += A_col_stride;
                X_j += X_col_stride;
            }

            A_i += A_row_stride;
            X_i += X_row_stride;
        }

    }else{
        Y += element_prod(A, X);
    }

    run_dbg(*this);
}

void NoDenSynapse::operator() (){
    output = b * input;

    run_dbg(*this);
}

void SimpleSynapse::operator() (){
    output *= -a;
    output += b * input;

    run_dbg(*this);
}

void Synapse::operator() (){
    for(int i = 0; i < input.size1(); i++){

        x[i].push_front(input(i, 0));

        output(i, 0) = 0.0;

        for(int j = 0; j < x[i].size(); j++){
            output(i, 0) += numer(j, 0) * x[i][j];
        }

        for(int j = 0; j < y[i].size(); j++){
            output(i, 0) -= denom(j, 0) * y[i][j];
        }

        y[i].push_front(output(i, 0));
    }

    run_dbg(*this);
}

void WhiteNoise::operator() (){
    if(inc){
        for(int i = 0; i < output.size1(); i++){
            output(i, 0) += alpha * dist(rng);
        }
    }else{
        for(int i = 0; i < output.size1(); i++){
            output(i, 0) = alpha * dist(rng);
        }
    }

    run_dbg(*this);
}

void WhiteSignal::operator() (){
    for(int i = 0; i < output.size1(); i++){
        output(i, 0) = coefs(idx % coefs.size1(), i);
    }

    idx++;

    run_dbg(*this);
}

void LIF::operator() (){
    dV = -expm1(-dt / tau_rc) * (J - voltage);
    voltage += dV;
    for(unsigned i = 0; i < n_neurons; ++i){
        voltage(i, 0) = voltage(i, 0) < min_voltage ? min_voltage : voltage(i, 0);
    }

    ref_time -= dt_vec;

    mult = ref_time;
    mult *= -dt_inv;
    mult += one;

    for(unsigned i = 0; i < n_neurons; ++i){
        mult(i, 0) = mult(i, 0) > 1 ? 1.0 : mult(i, 0);
        mult(i, 0) = mult(i, 0) < 0 ? 0.0 : mult(i, 0);
    }

    dtype overshoot;
    for(unsigned i = 0; i < n_neurons; ++i){
        voltage(i, 0) *= mult(i, 0);
        if(voltage(i, 0) > 1.0){
            output(i, 0) = dt_inv;
            overshoot = (voltage(i, 0) - 1.0) / dV(i, 0);
            ref_time(i, 0) = tau_ref + dt * (1.0 - overshoot);
            voltage(i, 0) = 0.0;
        }
        else
        {
            output(i, 0) = 0.0;
        }
    }

    run_dbg(*this);
}

void LIFRate::operator() (){
    for(unsigned i = 0; i < n_neurons; ++i){
        if(J(i, 0) > 1.0){
            output(i, 0) = 1.0 / (tau_ref + tau_rc * log1p(1.0 / (J(i, 0) - 1.0)));
        }else{
            output(i, 0) = 0.0;
        }
    }

    run_dbg(*this);
}

void AdaptiveLIF::operator() (){
    temp = J;
    J -= adaptation;
    LIF::operator()();
    J = temp;

    adaptation += (dt / tau_n) * (inc_n * output - adaptation);

    run_dbg(*this);
}

void AdaptiveLIFRate::operator() (){
    temp = J;
    J -= adaptation;
    LIFRate::operator()();
    J = temp;

    adaptation += (dt / tau_n) * (inc_n * output - adaptation);

    run_dbg(*this);
}

void RectifiedLinear::operator() (){
    dtype j = 0;
    for(unsigned i = 0; i < n_neurons; ++i){
        j = J(i, 0);
        output(i, 0) = j > 0.0 ? j : 0.0;
    }

    run_dbg(*this);
}

void Sigmoid::operator() (){
    for(unsigned i = 0; i < n_neurons; ++i){
        output(i, 0) = tau_ref_inv / (1.0 + exp(-J(i, 0)));
    }

    run_dbg(*this);
}

void Izhikevich::operator() (){
    for(unsigned i = 0; i < n_neurons; ++i){
        J(i, 0) = J(i, 0) > -30 ? J(i, 0) : -30;
    }

    voltage_squared = 0.04 * element_prod(voltage, voltage);

    dV = 5 * voltage;
    dV += voltage_squared + bias + J - recovery;
    dV *= 1000 * dt;
    voltage += dV;

    for(unsigned i = 0; i < n_neurons; ++i){
        if(voltage(i, 0) >= 30){
            output(i, 0) = dt_inv;
            voltage(i, 0) = reset_voltage;
        }else{
            output(i, 0) = 0.0;
        }
    }

    dU = coupling * voltage;
    dU -= recovery;
    dU *= tau_recovery * 1000 * dt;
    recovery += dU;

    for(unsigned i = 0; i < n_neurons; ++i){
        if(output(i, 0) > 0){
            recovery(i, 0) += reset_recovery;
        }
    }

    run_dbg(*this);
}

// to_string
string Reset::to_string() const {

    stringstream out;
    out << Operator::to_string();
    out << "dst:" << endl;
    out << signal_to_string(dst) << endl;

    return out.str();
}

string Copy::to_string() const  {

    stringstream out;
    out << Operator::to_string();
    out << "dst:" << endl;
    out << signal_to_string(dst) << endl;
    out << "src:" << endl;
    out << signal_to_string(src) << endl;

    return out.str();
}

string SlicedCopy::to_string() const  {

    stringstream out;
    out << Operator::to_string();
    out << "B:" << endl;
    out << signal_to_string(B) << endl;
    out << "A:" << endl;
    out << signal_to_string(A) << endl;

    out << "inc: " << inc << endl;

    out << "start_A: " << start_A << endl;
    out << "stop_A:" << stop_A << endl;
    out << "step_A:" << step_A << endl;

    out << "start_B:" << start_B << endl;
    out << "stop_B:" << stop_B << endl;
    out << "step_B:" << step_B << endl;

    out << "seq_A: " << endl;
    for(int i: seq_A){
        out << i << ", ";
    }
    out << endl;

    out << "seq_B: " << endl;
    for(int i: seq_B){
        out << i << ", ";
    }
    out << endl;

    return out.str();
}

string DotInc::to_string() const{

    stringstream out;
    out << Operator::to_string();
    out << "A:" << endl;
    out << signal_to_string(A) << endl;
    out << "X:" << endl;
    out << signal_to_string(X) << endl;
    out << "Y:" << endl;
    out << signal_to_string(Y) << endl;

    return out.str();
}

string ElementwiseInc::to_string() const{

    stringstream out;
    out << Operator::to_string();
    out << "A:" << endl;
    out << signal_to_string(A) << endl;
    out << "X:" << endl;
    out << signal_to_string(X) << endl;
    out << "Y:" << endl;
    out << signal_to_string(Y) << endl;

    out << "Broadcast: " << broadcast << endl;
    out << "A_row_stride: " << A_row_stride << endl;
    out << "A_col_stride: " << A_col_stride << endl;

    out << "X_row_stride: " << X_row_stride << endl;
    out << "X_col_stride: " << X_col_stride << endl;

    return out.str();
}

string NoDenSynapse::to_string() const{

    stringstream out;
    out << Operator::to_string();
    out << "input:" << endl;
    out << signal_to_string(input) << endl;
    out << "output:" << endl;
    out << signal_to_string(output) << endl;
    out << "b: " << b << endl;

    return out.str();
}


string SimpleSynapse::to_string() const{

    stringstream out;
    out << Operator::to_string();
    out << "input:" << endl;
    out << signal_to_string(input) << endl;
    out << "output:" << endl;
    out << signal_to_string(output) << endl;
    out << "a: " << a << endl;
    out << "b: " << b << endl;

    return out.str();
}

string Synapse::to_string() const{

    stringstream out;
    out << Operator::to_string();
    out << "input:" << endl;
    out << signal_to_string(input) << endl;
    out << "output:" << endl;
    out << signal_to_string(output) << endl;
    out << "numer:" << endl;
    out << numer << endl;
    out << "denom:" << endl;
    out << denom << endl;

    /*
    out << "x & y:" << endl;
    for(int i = 0; i < input.size(); i++){
        out << "i: " << i << endl;

        out << "x.size " << x[i].size() << endl;
        for(int j = 0; j < x[i].size(); j++){
            out << "x[ "<< j << "] = "<< x[i][j] << ", ";
        }
        out << endl;

        out << "y.size " << y[i].size() << endl;
        for(int j = 0; j < y[i].size(); j++){
            out << "y[ "<< j << "] = "<< y[i][j] << ", ";
        }
        out << endl;
    }
    */

    return out.str();
}

string WhiteNoise::to_string() const{

    stringstream out;
    out << Operator::to_string();
    out << "output:" << endl;
    out << signal_to_string(output) << endl;
    out << "mean: " << mean << endl;
    out << "std: " << std << endl;
    out << "do_scale: " << do_scale << endl;
    out << "inc: " << inc << endl;
    out << "dt: " << dt << endl;

    return out.str();
}

string WhiteSignal::to_string() const{

    stringstream out;
    out << Operator::to_string();
    out << "output:" << endl;
    out << signal_to_string(output) << endl;
    out << "coefs:" << endl;
    out << signal_to_string(coefs) << endl;
    out << "idx: " << idx << endl;

    return out.str();
}


string LIF::to_string() const{

    stringstream out;

    out << Operator::to_string();
    out << "J:" << endl;
    out << signal_to_string(J) << endl;
    out << "output:" << endl;
    out << signal_to_string(output) << endl;
    out << "voltage:" << endl;
    out << signal_to_string(voltage) << endl;
    out << "refractory_time:" << endl;
    out << signal_to_string(ref_time) << endl;
    out << "n_neurons: " << n_neurons << endl;;
    out << "tau_rc: " << tau_rc << endl;
    out << "tau_ref: " << tau_ref << endl;
    out << "min_voltage: " << min_voltage << endl;

    return out.str();
}

string LIFRate::to_string() const{

    stringstream out;
    out << Operator::to_string();
    out << "J:" << endl;
    out << signal_to_string(J) << endl;
    out << "output:" << endl;
    out << signal_to_string(output) << endl;
    out << "n_neurons: " << n_neurons << endl;
    out << "tau_rc: " << tau_rc << endl;
    out << "tau_ref: " << tau_ref << endl;

    return out.str();
}

string AdaptiveLIF::to_string() const{

    stringstream out;
    out << LIF::to_string();
    out << "tau_n: " << tau_n << endl;
    out << "inc_n: " << inc_n << endl;
    out << "adaptation: " << endl;
    out << signal_to_string(adaptation) << endl;

    return out.str();
}

string AdaptiveLIFRate::to_string() const{

    stringstream out;
    out << LIFRate::to_string();
    out << "tau_n: " << tau_n << endl;
    out << "inc_n: " << inc_n << endl;
    out << "dt: " << dt << endl;
    out << "adaptation: " << endl;
    out << signal_to_string(adaptation) << endl;

    return out.str();
}

string RectifiedLinear::to_string() const{

    stringstream out;
    out << Operator::to_string();
    out << "n_neurons: " << n_neurons << endl;
    out << "J:" << endl;
    out << signal_to_string(J) << endl;
    out << "output:" << endl;
    out << signal_to_string(output) << endl;

    return out.str();
}

string Sigmoid::to_string() const{

    stringstream out;
    out << Operator::to_string();
    out << "n_neurons: " << n_neurons << endl;
    out << "tau_ref: " << tau_ref << endl;
    out << "J:" << endl;
    out << signal_to_string(J) << endl;
    out << "output:" << endl;
    out << signal_to_string(output) << endl;

    return out.str();
}

string Izhikevich::to_string() const{

    stringstream out;
    out << Operator::to_string();
    out << "n_neurons: " << n_neurons << endl;
    out << "tau_recovery: " << tau_recovery << endl;
    out << "coupling: " << coupling << endl;
    out << "reset_voltage: " << reset_voltage << endl;
    out << "reset_recovery: " << reset_recovery << endl;
    out << "dt: " << dt << endl;

    out << "J:" << endl;
    out << signal_to_string(J) << endl;
    out << "output:" << endl;
    out << signal_to_string(output) << endl;
    out << "voltage:" << endl;
    out << signal_to_string(voltage) << endl;
    out << "recovery:" << endl;
    out << signal_to_string(recovery) << endl;

    return out.str();
}

// reset

void Synapse::reset(unsigned seed){
    for(int i = 0; i < input.size1(); i++){
        for(int j = 0; j < x[i].size(); j++){
            x[i][j] = 0.0;
        }
        for(int j = 0; j < y[i].size(); j++){
            y[i][j] = 0.0;
        }
    }
}

void WhiteNoise::reset(unsigned seed){
    rng.seed(seed);
}

void WhiteSignal::reset(unsigned seed){
    idx = 0;
}

unique_ptr<BaseSignal> python_list_to_signal(string s){
    vector<string> tokens;
    boost::split(tokens, s, boost::is_any_of(","));

    int size1 = boost::lexical_cast<int>(tokens[0]);
    int size2 = boost::lexical_cast<int>(tokens[1]);

    unique_ptr<BaseSignal> result(new BaseSignal(size1, size2));

    try{
        int i = 0;
        for(auto token = tokens.begin()+2; token != tokens.end(); token++){
            (*result)(int(i / size2), i % size2) = boost::lexical_cast<dtype>(*token);
            i++;
        }
    }catch(const boost::bad_lexical_cast& e){
        cout << "Caught bad lexical cast converting list to signal "
                "with error: " << e.what() << endl;
        terminate();
    }

    return result;
}

vector<int> python_list_to_index_vector(string s){
    // Remove surrounding square brackets
    boost::trim_if(s, boost::is_any_of("[]"));

    vector<string> tokens;
    boost::split(tokens, s, boost::is_any_of(","));

    vector<int> result;

    if(s.length() > 0){
        try{
            for(string token: tokens){
                boost::trim(token);
                result.push_back(boost::lexical_cast<int>(token));
            }
        }catch(const boost::bad_lexical_cast& e){
            cout << "Caught bad lexical cast while converting list "
                    "with error: " << e.what() << endl;
            terminate();
        }
    }

    return result;
}

string signal_to_string(const SignalView signal) {

    stringstream ss;

    if(RUN_DEBUG_TEST){
        ss << signal;
    }else{
        ss << "[" << signal.size1() << ", " << signal.size2() << "]";
    }

    return ss.str();
}

string signal_to_string(const BaseSignal signal) {

    stringstream ss;

    if(RUN_DEBUG_TEST){
        ss << signal;
    }else{
        ss << "[" << signal.size1() << ", " << signal.size2() << "]";
    }

    return ss.str();
}


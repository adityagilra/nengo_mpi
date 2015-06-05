#ifndef NENGO_MPI_OPERATOR_HPP
#define NENGO_MPI_OPERATOR_HPP

#include <vector>
#include <string>
#include <sstream>
#include <iostream>
#include <memory>

#include <boost/numeric/ublas/matrix.hpp>
#include <boost/numeric/ublas/matrix_proxy.hpp>
#include <boost/numeric/ublas/storage.hpp>
#include <boost/numeric/ublas/io.hpp>
#include <boost/numeric/ublas/operation.hpp>

#include <boost/circular_buffer.hpp>

#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>

#include "debug.hpp"


using namespace std;

namespace ublas = boost::numeric::ublas;

typedef double dtype;

typedef ublas::unbounded_array<dtype> array_type;
typedef ublas::matrix<dtype> BaseSignal;
typedef ublas::matrix_slice<BaseSignal> SignalView;
typedef ublas::scalar_matrix<dtype> ScalarSignal;

/* Type of keys for various maps in the MpiSimulatorChunk. Keys are typically
 * addresses of python objects, so we need to use long long ints (64 bits). */
typedef long long int key_type;

// Current implementation: Each Operator is essentially a closure.
// At run time, these closures are stored in a list, and we call
// them sequentially each time step. The order they are called in is determined
// by the order they are given to us from python.
//
// Note that the () operator is a virtual function, which comes with some overhead.
// Future optimizations should look at another scheme, either function pointers
// or, ideally, finding some way to make these functions non-pointers and non-virtual.

class Operator{

public:
    virtual string classname() const { return "Operator"; }
    virtual void operator() () = 0;
    virtual string to_string() const{
        return classname() + '\n';
    };

    friend ostream& operator << (ostream &out, const Operator &op){
        out << op.to_string();
        return out;
    }
};

class Reset: public Operator{

public:
    Reset(SignalView dst, dtype value);
    virtual string classname() const { return "Reset"; }

    void operator()();
    virtual string to_string() const;

protected:
    SignalView dst;
    ScalarSignal dummy;
    dtype value;
};

class Copy: public Operator{
public:
    Copy(SignalView dst, SignalView src);
    virtual string classname() const { return "Copy"; }

    void operator()();
    virtual string to_string() const;

protected:
    SignalView dst;
    SignalView src;
};

// Increment signal Y by dot(A,X)
class DotInc: public Operator{
public:
    DotInc(SignalView A, SignalView X, SignalView Y);
    virtual string classname() const { return "DotInc"; }

    void operator()();
    virtual string to_string() const;

protected:
    SignalView A;
    SignalView X;
    SignalView Y;
};


class ElementwiseInc: public Operator{
public:
    ElementwiseInc(SignalView A, SignalView X, SignalView Y);
    virtual string classname() const { return "ElementwiseInc"; }

    void operator()();
    virtual string to_string() const;

protected:
    SignalView A;
    SignalView X;
    SignalView Y;

    bool broadcast;

    // Strides are 0 or 1, to support broadcasting
    int A_row_stride;
    int A_col_stride;

    int X_row_stride;
    int X_col_stride;
};

class NoDenSynapse: public Operator{

public:
    NoDenSynapse(SignalView input, SignalView output, dtype b);

    virtual string classname() const { return "NoDenSynapse"; }

    void operator()();
    virtual string to_string() const;

protected:
    SignalView input;
    SignalView output;

    dtype b;
};

class SimpleSynapse: public Operator{

public:
    SimpleSynapse(SignalView input, SignalView output, dtype a, dtype b);

    virtual string classname() const { return "SimpleSynapse"; }

    void operator()();
    virtual string to_string() const;

protected:
    SignalView input;
    SignalView output;

    dtype a;
    dtype b;
};

class Synapse: public Operator{

public:
    Synapse(
        SignalView input, SignalView output,
        BaseSignal numer, BaseSignal denom);

    virtual string classname() const { return "Synapse"; }

    void operator()();
    virtual string to_string() const;

protected:
    SignalView input;
    SignalView output;

    BaseSignal numer;
    BaseSignal denom;

    vector< boost::circular_buffer<dtype> > x;
    vector< boost::circular_buffer<dtype> > y;
};

class LIF: public Operator{
public:
    LIF(
        int n_neuron, dtype tau_rc, dtype tau_ref, dtype min_voltage,
        dtype dt, SignalView J, SignalView output, SignalView voltage,
        SignalView ref_time);
    virtual string classname() const { return "LIF"; }

    void operator()();
    virtual string to_string() const;

protected:
    dtype dt;
    dtype dt_inv;
    dtype tau_rc;
    dtype tau_ref;
    dtype min_voltage;
    int n_neurons;

    SignalView J;
    SignalView output;
    SignalView voltage;
    SignalView ref_time;

    ScalarSignal dt_vec;
    ScalarSignal one;
    BaseSignal mult;
    BaseSignal dV;
};

class LIFRate: public Operator{
public:
    LIFRate(int n_neurons, dtype tau_rc, dtype tau_ref, SignalView J, SignalView output);
    virtual string classname() const { return "LIFRate"; }

    void operator()();
    virtual string to_string() const;

protected:
    dtype tau_rc;
    dtype tau_ref;
    int n_neurons;

    SignalView J;
    SignalView output;
};

class AdaptiveLIF: public LIF{
public:
    AdaptiveLIF(
        int n_neuron, dtype tau_n, dtype inc_n, dtype tau_rc, dtype tau_ref,
        dtype min_voltage, dtype dt, SignalView J, SignalView output, SignalView voltage,
        SignalView ref_time, SignalView adaptation);
    virtual string classname() const { return "AdaptiveLIF"; }

    void operator()();
    virtual string to_string() const;

protected:
    dtype tau_n;
    dtype inc_n;
    SignalView adaptation;
    BaseSignal temp;
};

class AdaptiveLIFRate: public LIFRate{

public:
    AdaptiveLIFRate(
        int n_neurons, dtype tau_n, dtype inc_n, dtype tau_rc, dtype tau_ref,
        dtype dt, SignalView J, SignalView output, SignalView adaptation);
    virtual string classname() const { return "AdaptiveLIFRate"; }

    void operator()();
    virtual string to_string() const;

protected:
    dtype dt;
    dtype tau_n;
    dtype inc_n;
    SignalView adaptation;
    BaseSignal temp;
};

class RectifiedLinear: public Operator{
public:
    RectifiedLinear(int n_neurons, SignalView J, SignalView output);
    virtual string classname() const { return "RectifiedLinear"; }

    void operator()();
    virtual string to_string() const;

protected:
    int n_neurons;

    SignalView J;
    SignalView output;
};

class Sigmoid: public Operator{
public:
    Sigmoid(int n_neurons, dtype tau_ref, SignalView J, SignalView output);
    virtual string classname() const { return "Sigmoid"; }

    void operator()();
    virtual string to_string() const;

protected:
    int n_neurons;
    dtype tau_ref;
    dtype tau_ref_inv;

    SignalView J;
    SignalView output;
};

class Izhikevich: public Operator{
public:
    Izhikevich(
        int n_neurons, dtype tau_recovery, dtype coupling,
        dtype reset_voltage, dtype reset_recovery, dtype dt,
        SignalView J, SignalView output, SignalView voltage,
        SignalView recovery);
    virtual string classname() const { return "Izhikevich"; }

    void operator()();
    virtual string to_string() const;

protected:
    int n_neurons;
    dtype tau_recovery;
    dtype coupling;
    dtype reset_voltage;
    dtype reset_recovery;
    dtype dt;
    dtype dt_inv;

    SignalView J;
    SignalView output;
    SignalView voltage;
    SignalView recovery;

    BaseSignal dV;
    BaseSignal dU;
    BaseSignal voltage_squared;
    ScalarSignal bias;
};


/* Helper function to extract a BaseSignal from a string. Assumes
 * the data for the BaseSignal is encoded in the string as a python list. */
unique_ptr<BaseSignal> extract_float_list(string s);

#endif
#include <iostream>
#include <boost/numeric/ublas/operation.hpp>

#include "operator.hpp"

ostream& operator << (ostream &out, const Operator &op){
    op.print(out);
    return out;
}

Reset::Reset(Vector* dst, float value)
    :dst(dst), value(value), size(dst->size()){

    dummy = ScalarVector(size, value);
}

void Reset::operator() (){

    (*dst) = dummy;

#ifdef _DEBUG
    cout << *this;
#endif
}

void Reset::print(ostream &out) const {
    out << "Reset:" << endl;
    out << "dst:" << endl;
    out << *dst << endl << endl;
}

Copy::Copy(Vector* dst, Vector* src)
    :dst(dst), src(src){}

void Copy::operator() (){

    *dst = *src;

#ifdef _DEBUG
    cout << *this;
#endif
}

void Copy::print(ostream &out) const  {
    out << "Copy:" << endl;
    out << "dst:" << endl;
    out << *dst << endl;
    out << "src:" << endl;
    out << *src << endl << endl;
}

DotInc::DotInc(Matrix* A, Vector* X, Vector* Y)
    :A(A), X(X), Y(Y), size(Y->size()){}

void DotInc::operator() (){
    axpy_prod(*A, *X, *Y, false);

#ifdef _DEBUG
    cout << *this;
#endif
}

void DotInc::print(ostream &out) const{
    out << "DotInc:" << endl;
    out << "A:" << endl;
    out << *A << endl;
    out << "X:" << endl;
    out << *X << endl;
    out << "Y:" << endl;
    out << *Y << endl << endl;
}

ScalarDotInc::ScalarDotInc(Vector* A, Vector* X, Vector* Y)
    :A(A), X(X), Y(Y), size(Y->size()){}

void ScalarDotInc::operator() (){
    for (unsigned i = 0; i < size; ++i){
        (*Y)[i] += (*A)[0] * (*X)[i];
    }

#ifdef _DEBUG
    cout << *this;
#endif
}

void ScalarDotInc::print(ostream &out) const{
    out << "ScalarDotInc:" << endl;
    out << "A:" << endl;
    out << *A << endl;
    out << "X:" << endl;
    out << *X << endl;
    out << "Y:" << endl;
    out << *Y << endl << endl;
}

ProdUpdate::ProdUpdate(Matrix* A, Vector* X, Vector* B, Vector* Y)
    :A(A), X(X), B(B), Y(Y), size(Y->size()){}

void ProdUpdate::operator() (){
    for (unsigned i = 0; i < size; ++i){
        (*Y)[i] *= (*B)[i];
    }
    axpy_prod(*A, *X, *Y, false);

#ifdef _DEBUG
    cout << *this;
#endif
}

void ProdUpdate::print(ostream &out) const{
    out << "ProdUpdate:" << endl;
    out << "A:" << endl;
    out << *A << endl;
    out << "X:" << endl;
    out << *X << endl;
    out << "B:" << endl;
    out << *B << endl;
    out << "Y:" << endl;
    out << *Y << endl << endl;
}

ScalarProdUpdate::ScalarProdUpdate(Vector* A, Vector* X, Vector* B, Vector* Y)
    :A(A), X(X), B(B), Y(Y), size(Y->size()){}

void ScalarProdUpdate::operator() (){
    for (unsigned i = 0; i < size; ++i){
        (*Y)[i] *= (*B)[i];
    }

    for (unsigned i = 0; i < size; ++i){
        (*Y)[i] += (*A)[0] * (*X)[i];
    }

#ifdef _DEBUG
    cout << *this;
#endif
}

void ScalarProdUpdate::print(ostream &out) const{
    out << "ScalarProdUpdate:" << endl;
    out << "A:" << endl;
    out << *A << endl;
    out << "X:" << endl;
    out << *X << endl;
    out << "B:" << endl;
    out << *B << endl;
    out << "Y:" << endl;
    out << *Y << endl << endl;
}

SimLIF::SimLIF(int n_neurons, float tau_rc, float tau_ref, float dt, Vector* J, Vector* output)
:n_neurons(n_neurons), dt(dt), tau_rc(tau_rc), tau_ref(tau_ref), dt_inv(1.0 / dt), J(J), output(output){
    voltage = ScalarVector(n_neurons, 0);
    refractory_time = ScalarVector(n_neurons, 0);
    one = ScalarVector(n_neurons, 1.0);
    dt_vec = dt * one;
}

void SimLIF::operator() (){
    dV = (dt / tau_rc) * ((*J) - voltage);
    voltage += dV;
    for(unsigned i = 0; i < n_neurons; ++i){
        voltage[i] = voltage[i] < 0 ? 0 : voltage[i];
    }

    refractory_time -= dt_vec;

    mult = (one - refractory_time * dt_inv);

    for(unsigned i = 0; i < n_neurons; ++i){
        mult[i] = mult[i] > 1 ? 1 : mult[i];
        mult[i] = mult[i] < 0 ? 0 : mult[i];
    }

    float overshoot;
    for(unsigned i = 0; i < n_neurons; ++i){
        voltage[i] *= mult[i];
        if (voltage[i] > 1){
            (*output)[i] = 1;
            overshoot = (voltage[i] - 1) / dV[i];
            refractory_time[i] = tau_ref + dt * (1 - overshoot);
            voltage[i] = 0;
        }
        else
        {
            (*output)[i] = 0;
        }
    }

#ifdef _DEBUG
    cout << *this;
#endif
}

void SimLIF::print(ostream &out) const{
    out << "SimLIF:" << endl;
    out << "J:" << endl;
    out << *J << endl;
    out << "output:" << endl;
    out << *output << endl;
    out << "voltage:" << endl;
    out << voltage << endl << endl;
    out << "refractory_time:" << endl;
    out << refractory_time << endl << endl;
}

SimLIFRate::SimLIFRate(int n_neurons, float tau_rc, float tau_ref, float dt, Vector* J, Vector* output)
:n_neurons(n_neurons), dt(dt), tau_rc(tau_rc), tau_ref(tau_ref), J(J), output(output){
}

void SimLIFRate::operator() (){
    for(unsigned i = 0; i < n_neurons; ++i){
        if((*J)[i] > 0){
            (*output)[i] = dt / (tau_ref + tau_rc * log(1.0 / (*J)[i]));
        }else{
            (*output)[i] = 0.0;
        }
    }

#ifdef _DEBUG
    cout << *this;
#endif
}

void SimLIFRate::print(ostream &out) const{
    out << "SimLIFRate:" << endl;
    out << "J:" << endl;
    out << *J << endl;
    out << "output:" << endl;
    out << *output << endl;
}

MPISend::MPISend(){
}

void MPISend::operator() (){

#ifdef _DEBUG
    cout << *this;
#endif
}

void MPISend::print(ostream &out) const{
}

MPIReceive::MPIReceive(){
}

void MPIReceive::operator() (){

#ifdef _DEBUG
    cout << *this;
#endif
}

void MPIReceive::print(ostream &out) const{
}

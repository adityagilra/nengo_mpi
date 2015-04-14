#ifndef NENGO_MPI_CUSTOM_OPS_HPP
#define NENGO_MPI_CUSTOM_OPS_HPP

#include <string>
#include <vector>
#include <map>
#include <exception>
#include <fstream>
#include <sstream>
#include <memory>
#include <cmath>
#include <time.h>

#include "operator.hpp"

using namespace std;

class SpaunStimulus: public Operator{
public:
    SpaunStimulus(
        SignalView output, dtype* time_pointer, vector<string> stim_sequence,
        float present_interval, float present_blanks);

    string classname() const {return "SpaunStimulus"; }

    void operator() ();

    virtual string to_string() const;

protected:
    dtype* time_pointer;

    int num_stimuli;
    vector<string> stim_sequence;
    float present_interval;
    float present_blanks;

    int image_size;
    vector<unique_ptr<BaseSignal>> images;
    SignalView output;

    int previous_index;
};

/*
 * Down-sample the given image, returning an image whose size is new_size.
 * new_size should be < the size of the given image. Currently downsamples
 * by randomly choosing indices without replacement and then sorting. */
unique_ptr<BaseSignal> do_down_sample(unique_ptr<BaseSignal> image, int new_size);

/*
 * Load the image data from a file. Returned map associates a label (e.g. "0", "2", "W")
 * with a vector of images corresponding to that label. Each image is stored as a BaseMatrix
 * with dimensions (image_size, 1). */
map<string, vector<unique_ptr<BaseSignal>>> load_image_data(string filename);

#endif

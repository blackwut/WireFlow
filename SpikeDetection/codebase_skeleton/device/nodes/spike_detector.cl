inline bool spike_detector_function(tuple_t in)
{
    return (fabsf(in.temperature - in.average) > (THRESHOLD * in.average));
}



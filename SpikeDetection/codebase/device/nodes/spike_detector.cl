inline bool spike_detector_function(tuple_t in)
{
    return (fabsf(in.property_value - in.incremental_average) > (THRESHOLD * in.incremental_average));
}

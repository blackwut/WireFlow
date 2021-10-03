inline tuple_t generator_function(const uint size, __private rng_state_t * rng_int, __private rng_state_t * rng_float)
{
    tuple_t out;
    out.device_id = next_int(rng_int) % MAX_KEYS;
    out.temperature = 9.0f + next_float(rng_float);
    out.average = 0.0f;
    return out;
}

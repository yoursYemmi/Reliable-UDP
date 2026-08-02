#include "minitcp.h"
#include <math.h>
#include <windows.h> // Explicitly include windows.h

uint64_t get_current_time_ms(void)
{
    // Fallback to GetTickCount() which is universally supported by all MinGW versions
    // Note: GetTickCount() wraps around after ~49.7 days, which is fine for our simulation
    return (uint64_t)GetTickCount();
}

void init_rtt_tracker(RTTTracker *tracker)
{
    tracker->srtt = 0.0f;
    tracker->rttvar = 0.0f;
    tracker->rto_ms = INITIAL_RTO_MS;
}

void update_rto(RTTTracker *tracker, uint64_t sample_rtt_ms)
{
    float sample = (float)sample_rtt_ms;

    if (tracker->srtt == 0.0f)
    {
        // First measurement (RFC 6298 Section 2.2)
        tracker->srtt = sample;
        tracker->rttvar = sample / 2.0f;
    }
    else
    {
        // Subsequent measurements (RFC 6298 Section 2.3)
        // alpha = 1/8 (0.125), beta = 1/4 (0.25)
        float delta = fabs(tracker->srtt - sample);
        tracker->rttvar = (0.75f * tracker->rttvar) + (0.25f * delta);
        tracker->srtt = (0.875f * tracker->srtt) + (0.125f * sample);
    }

    int calc_rto = (int)(tracker->srtt + (4.0f * tracker->rttvar));

    // Clamp RTO within realistic bounds [MIN_RTO_MS, MAX_RTO_MS]
    if (calc_rto < MIN_RTO_MS)
        calc_rto = MIN_RTO_MS;
    if (calc_rto > MAX_RTO_MS)
        calc_rto = MAX_RTO_MS;

    tracker->rto_ms = calc_rto;
}
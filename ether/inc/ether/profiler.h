#ifndef ETHER_PROFILER_H
#define ETHER_PROFILER_H

/* Start capture on the application thread before initialization. */
void profiler_start(void);

/* Save the history before the iteration loop, then reuse its suffix each frame. */
void profiler_frame_checkpoint(void);
void profiler_frame_begin(void);
void profiler_frame_end(void);

/* Print completed calls recorded after the live trace and stop capture. */
void profiler_finish(void);

#endif

#pragma once

/* Debug hooks: optional CPU instruction trace (DMGEMU_TRACE) and per-frame
   state snapshots (DMGEMU_SNAP).  The real implementations live in
   dbghooks.cpp and are only compiled when DMGEMU_DEBUG_HOOKS is defined;
   in an ordinary build the hooks are empty inline functions that the
   compiler eliminates, so no debug code reaches the release binary.
   Enable with the build script rebuild_dbg.bat
   (passes /p:DMGEMU_EXTRA_DEFINES=DMGEMU_DEBUG_HOOKS).                  */

#include <stdint.h>

#ifdef DMGEMU_DEBUG_HOOKS
void dbg_cpu_trace(uint32_t pc, unsigned opcode, uint32_t clk);
void dbg_snap_frame(uint32_t const* pbuf, int w, int h);
void dbg_log_event(char const* tag, char const* text);
#else
static inline void dbg_cpu_trace(uint32_t pc, unsigned opcode, uint32_t clk)
{ (void)pc; (void)opcode; (void)clk; }
static inline void dbg_snap_frame(uint32_t const* pbuf, int w, int h)
{ (void)pbuf; (void)w; (void)h; }
static inline void dbg_log_event(char const* tag, char const* text)
{ (void)tag; (void)text; }
#endif

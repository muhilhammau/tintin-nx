/* tintin_swp.c -- emulate ARM SWP/SWPB (removed from AArch32 EL0 on Switch).
 * The game's old boost spinlock_pool uses it. Runs in exception context:
 * no logging, no locks, no allocation. */
#include <stdint.h>
#include "exc_handler.h"

volatile uint32_t tt_swp_count;
volatile uint32_t tt_swp_first_type;
volatile uint32_t tt_swp_first_pc;

static int cond_ok(uint32_t cond, uint32_t cpsr) {
  int n = (cpsr >> 31) & 1, z = (cpsr >> 30) & 1;
  int c = (cpsr >> 29) & 1, v = (cpsr >> 28) & 1, r;
  switch (cond >> 1) {
    case 0: r = z; break;
    case 1: r = c; break;
    case 2: r = n; break;
    case 3: r = v; break;
    case 4: r = c && !z; break;
    case 5: r = (n == v); break;
    case 6: r = (n == v) && !z; break;
    default: return 1; /* AL */
  }
  return (cond & 1) ? !r : r;
}

int port_exception_hook(uint32_t type, ThreadExceptionInfo32 *info,
                        ThreadExceptionFrame32 *frame) {
  if (type != EXC_UNDEFINED_INSTRUCTION && type != EXC_INSTRUCTION_ABORT)
    return 0;
  if (info->pstate & 0x20) return 0;            /* Thumb: not ours */
  uint32_t pc = info->pc;
  if ((pc & 3) || !dcr_is_code_addr(pc)) return 0;

  uint32_t insn = *(volatile uint32_t *)pc;
  if ((insn & 0x0FB00FF0u) != 0x01000090u) return 0; /* not SWP/SWPB */
  uint32_t cond = insn >> 28;
  if (cond == 0xF) return 0;

  if (cond_ok(cond, info->pstate)) {
    unsigned rn = (insn >> 16) & 15, rd = (insn >> 12) & 15, rm = insn & 15;
    if (rn == 15 || rd == 15 || rm == 15) return 0;
    uint32_t addr = *rt_exc_reg(info, frame, rn);
    uint32_t val  = *rt_exc_reg(info, frame, rm);
    uint32_t old;
    if (insn & 0x400000u) {                      /* SWPB */
      old = __atomic_exchange_n((volatile uint8_t *)addr, (uint8_t)val,
                                __ATOMIC_SEQ_CST);
    } else {                                     /* SWP */
      if (addr & 3) return 0;
      old = __atomic_exchange_n((volatile uint32_t *)addr, val,
                                __ATOMIC_SEQ_CST);
    }
    *rt_exc_reg(info, frame, rd) = old;
  }
  if (!tt_swp_count) { tt_swp_first_type = type; tt_swp_first_pc = pc; }
  tt_swp_count++;
  info->pc = pc + 4;
  return 1;
}

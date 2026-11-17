#include <stdint.h>

extern uint32_t g_phyFuns[];

void __real_rf_init(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3);
void __wrap_rf_init(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3);

static uint32_t phy_rom_stub(uint32_t a0, uint32_t a1,
                             uint32_t a2, uint32_t a3)
{
  (void)a0;
  (void)a1;
  (void)a2;
  (void)a3;
  return 0;
}

void __wrap_rf_init(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3)
{
  g_phyFuns[352 / sizeof(uint32_t)] = (uint32_t)(uintptr_t)phy_rom_stub;
  g_phyFuns[516 / sizeof(uint32_t)] = (uint32_t)(uintptr_t)phy_rom_stub;
  g_phyFuns[520 / sizeof(uint32_t)] = (uint32_t)(uintptr_t)phy_rom_stub;

  __real_rf_init(a0, a1, a2, a3);
}

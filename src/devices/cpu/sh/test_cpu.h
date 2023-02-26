#ifndef MAME_CPU_TESTCPU_H
#define MAME_CPU_TESTCPU_H

#pragma once

#include "sh2.h"

class test_cpu_device : public sh2_device
{
public:
	test_cpu_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

protected:
	virtual void device_reset() override;
	virtual void debugger_instruction_hook(offs_t curpc) override;
    void state_reset(u64 test_i);

private:
	u64 test_i    = 0;
    u64 vdp1_dst  = 0;
    u64 chunk_src = 0;
    u64 dcx_size  = 0;
};

DECLARE_DEVICE_TYPE(TEST_CPU, test_cpu_device)

#endif // MAME_CPU_TESTCPU_H

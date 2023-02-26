#include <fstream>
#include <iomanip>
#include <iostream>

#include "emu.h"
#include "test_cpu.h"

DEFINE_DEVICE_TYPE(TEST_CPU,  test_cpu_device,  "testcpu",  "TestCPU")

test_cpu_device::test_cpu_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
: sh2_device(mconfig, TEST_CPU, tag, owner, clock, CPU_TYPE_SH2, address_map_constructor(FUNC(sh2_device::sh7604_map), this), 32)
{
	// FIXME: Pass as config option
	set_force_no_drc(true);
	m_isdrc = allow_drc();
}

void test_cpu_device::state_reset(u64 test_i) {
	osd_printf_verbose("test_i=0x%08X\n", test_i);
	set_state_int(SH4_R0,   0x00000000);
	set_state_int(SH4_R1,   0x00000008);
	set_state_int(SH4_R2,   0x00000000);
	set_state_int(SH4_R3,   0x06098CA8);
	set_state_int(SH4_R4,   test_i);
	set_state_int(SH4_R5,   0x06001EDC);
	set_state_int(SH4_R6,   0x00000000);
	set_state_int(SH4_R7,   0x00000000);
	set_state_int(SH4_R8,   0x0000002B);
	set_state_int(SH4_R9,   0x00000020);
	set_state_int(SH4_R10,  0x0000030B);
	set_state_int(SH4_R11,  0x060A6100);
	set_state_int(SH4_R12,  0x060A1630);
	set_state_int(SH4_R13,  0x0000030B);
	set_state_int(SH4_R14,  0x0609BD58);
	set_state_int(SH4_R15,  0x06001EB0);
	set_state_int(SH_SR,    0x00000101);
	set_state_int(SH4_GBR,  0x00000000);
	set_state_int(SH4_VBR,  0x00000000);
	set_state_int(SH4_DBR,  0x00000000);
	set_state_int(SH4_MACH, 0x00000000);
	set_state_int(SH4_MACL, 0x000007D0);
	set_state_int(SH4_PR,   0x0603DDD8);
	set_pc(0x0603d46e);
}

void test_cpu_device::device_reset() {
	sh2_device::device_reset();
	state_reset(0);
}

void test_cpu_device::debugger_instruction_hook(offs_t curpc)
{
	//osd_printf_verbose("PC: 0x%08X\n", pc());
	if (pc() == 0x0603d506) { // jsr r2 (jump to VDP1 handler)
    	if (state_int(SH4_R2) != 0x060516ba) { // not the handler we want
			test_i++;
			state_reset(test_i);
		}
	}
	else if (pc() == 0x060516ba) { // mov.l r14,@-r15=>local_4 (chunk_dcx_to_vdp1() entry)
    	vdp1_dst  = state_int(SH4_R4);
        chunk_src = state_int(SH4_R5);
        dcx_size  = state_int(SH4_R6);
		osd_printf_verbose("0x%08X 0x%08X 0x%08X\n", vdp1_dst, chunk_src, dcx_size);
	}
	else if (pc() == 0x0603d51c) { // rts (chunk_dcx() end)
		// FIXME: How to load as 32bit BE?
		// FIXME: Assuming sizes are multiple of 4...
		memory_share *shr = machine().root_device().memshare("maincpu_share");
		u8 *p = reinterpret_cast<uint8_t *>(shr->ptr());

		// Dump decompressed chunk
		std::stringstream ss;
		ss << "0x" << std::hex << chunk_src;
		std::ofstream f("chunks/" + ss.str(), std::ios::binary | std::ios::out);
		u8 m[4];
		for (int i = 0; i < dcx_size; i += 4) {
			// Destination is cache address, convert to mirrored address
			u8 p3 = p[vdp1_dst - 0x20000000 + i + 3];
			u8 p2 = p[vdp1_dst - 0x20000000 + i + 2];
			u8 p1 = p[vdp1_dst - 0x20000000 + i + 1];
			u8 p0 = p[vdp1_dst - 0x20000000 + i + 0];
			m[0] = p3;
			m[1] = p2;
			m[2] = p1;
			m[3] = p0;
			f.write(reinterpret_cast<const char*>(m), 4);
		}
		f.close();

		if (test_i < 0x800) {
			test_i++;
			state_reset(test_i);
		} else {
			exit(123);
		}
	}
	device_execute_interface::debugger_instruction_hook(curpc);
}
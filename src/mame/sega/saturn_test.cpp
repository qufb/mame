#include <iostream>

#include "emu.h"

#include "bus/generic/carts.h"
#include "bus/generic/slot.h"
#include "cpu/sh/test_cpu.h"

namespace {

class test_state : public driver_device
{
public:
	test_state(const machine_config &mconfig, device_type type, const char *tag)
		: driver_device(mconfig, type, tag)
		, m_maincpu(*this, "maincpu")
		, m_cart(*this, "cartslot")
		, m_cart_region(nullptr)
		, m_bank(*this, "cartbank")
	{ }

	void test_config(machine_config &config);

private:
	virtual void machine_start() override;
	
	DECLARE_DEVICE_IMAGE_LOAD_MEMBER(cart_load);

	void test_map(address_map &map);

	required_device<cpu_device> m_maincpu;
	required_device<generic_slot_device> m_cart;
	memory_region *m_cart_region;
	required_memory_bank m_bank;
};

void test_state::test_map(address_map &map)
{
	map(0x00000000, 0x07ffffff).ram().bankr("cartbank").share("maincpu_share");
	map(0x20000000, 0x27ffffff).ram();
	map(0x40000000, 0x46ffffff).nopw(); // associative purge page
	map(0x60000000, 0x600003ff).nopw(); // cache address array
	map(0xc0000000, 0xc0000fff).ram(); // cache data array, Dragon Ball Z sprites relies on this
}

static INPUT_PORTS_START( test_input )
INPUT_PORTS_END

DEVICE_IMAGE_LOAD_MEMBER(test_state::cart_load)
{
	uint32_t size = m_cart->common_get_size("cartrom");
	m_cart->rom_alloc(size, GENERIC_ROM32_WIDTH, ENDIANNESS_BIG);
	// FIXME: How to load as 32bit BE?
	m_cart->common_load_rom(m_cart->get_rom_base(), size, "cartrom");
	u8 *p = m_cart->get_rom_base();
	for (int i = 0; i < size; i += 4) {
		u8 p3 = p[i + 3];
		u8 p2 = p[i + 2];
		u8 p1 = p[i + 1];
		u8 p0 = p[i + 0];
		p[i + 0] = p3;
		p[i + 1] = p2;
		p[i + 2] = p1;
		p[i + 3] = p0;
	}

	return image_init_result::PASS;
}

void test_state::test_config(machine_config &config)
{
	TEST_CPU(config, m_maincpu, XTAL(14'318'181)*2); // 28.6364 MHz
	m_maincpu->set_addrmap(AS_PROGRAM, &test_state::test_map);

	GENERIC_CARTSLOT(config, m_cart, generic_plain_slot, "test_cart");
	m_cart->set_width(GENERIC_ROM32_WIDTH);
	m_cart->set_endian(ENDIANNESS_BIG);
	m_cart->set_device_load(FUNC(test_state::cart_load));
	m_cart->set_must_be_loaded(true);
}

void test_state::machine_start() {
	std::string region_tag;
	m_cart_region = memregion(region_tag.assign(m_cart->tag()).append(GENERIC_ROM_REGION_TAG).c_str());
	m_bank->configure_entry(0, m_cart_region->base());
	m_bank->set_entry(0);
}

ROM_START( saturn_test )
ROM_END

} // anonymous namespace

//    year, name,        parent,  compat, machine,     input,      class,      init,       company, fullname, flags
CONS( 2000, saturn_test, 0,       0,      test_config, test_input, test_state, empty_init, "Test",  "Test",   MACHINE_NO_SOUND_HW )

// license:BSD-3-Clause
// copyright-holders:QUFB
/**********************************************************************

    MIDI audio functions of the AP2010 LSI

**********************************************************************/

#ifndef MAME_MACHINE_AP2010MIDI_H
#define MAME_MACHINE_AP2010MIDI_H

#pragma once

#include <queue>

#include "machine/midistream.h"

class ap2010midi_device :  public device_t
{
public:
	// construction/destruction
	ap2010midi_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock = 0);

	auto txd_handler() { return m_txd_handler.bind(); }

	void write(uint8_t data);

	void write_midi_clock(int state);
	void output_txd(int v);
	void write_txc(int state);

protected:
	ap2010midi_device(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, uint32_t clock);

	// device-level overrides
	virtual void device_start() override;
	virtual void device_reset() override;

private:
	devcb_write_line m_txd_handler;

	enum midi_state : uint8_t
	{
		STATE_START,
		STATE_DATA,
		STATE_STOP
	};
	enum midi_status : uint8_t
	{
		STATUS_IDLE = (1U<<0),
		STATUS_HAS_EVENTS = (1U<<1),
		STATUS_WRITING_DATA = (1U<<2)
	};

	uint8_t m_status;
	uint8_t m_tdr;

	int m_divide;
	int m_bits;
	int m_stopbits;

	int m_txc;
	int m_txd;
	int m_tx_state;
	int m_tx_bits;
	int m_tx_shift;
	int m_tx_counter;
	std::queue<uint8_t> m_midi_events;
};

// device type definition
DECLARE_DEVICE_TYPE(AP2010MIDI, ap2010midi_device)

#endif // MAME_MACHINE_AP2010MIDI_H

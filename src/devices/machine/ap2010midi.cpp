// license:BSD-3-Clause
// copyright-holders:QUFB
/**********************************************************************

    MIDI audio functions of the AP2010 LSI

**********************************************************************/

#include "emu.h"

#include "ap2010midi.h"

#define VERBOSE (0)
#include "logmacro.h"

DEFINE_DEVICE_TYPE(AP2010MIDI, ap2010midi_device, "ap2010midi", "AP2010 MIDI")

ap2010midi_device::ap2010midi_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: ap2010midi_device(mconfig, AP2010MIDI, tag, owner, clock)
{
}

ap2010midi_device::ap2010midi_device(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, uint32_t clock)
	: device_t(mconfig, type, tag, owner, clock)
	, m_txd_handler(*this)
	, m_status(0)
	, m_tdr(0)
	, m_divide(0)
	, m_txc(0)
	, m_txd(0)
	, m_tx_state(0)
	, m_tx_counter(0)
{
}

void ap2010midi_device::device_start()
{
	save_item(NAME(m_status));
	save_item(NAME(m_tdr));

	save_item(NAME(m_divide));
	save_item(NAME(m_bits));
	save_item(NAME(m_stopbits));

	save_item(NAME(m_txc));
	save_item(NAME(m_txd));
	save_item(NAME(m_tx_state));
	save_item(NAME(m_tx_bits));
	save_item(NAME(m_tx_shift));
	save_item(NAME(m_tx_counter));
}

void ap2010midi_device::device_reset()
{
	m_tdr = 0;
	m_status = STATUS_IDLE;
	m_divide = 1;
	m_bits = 8;
	m_stopbits = 1;
	m_tx_state = STATE_START;

	output_txd(1);
}

void ap2010midi_device::write(uint8_t data)
{
	LOG("%s write = %02x\n", tag(), data);
	m_midi_events.push(data & 0xff);
}

void ap2010midi_device::output_txd(int txd)
{
	if (m_txd != txd)
	{
		LOG("%s output_txd %d %d\n", tag(), m_txd, txd);
		m_txd = txd;

		m_txd_handler(m_txd);
	}
}

void ap2010midi_device::write_txc(int state)
{
	if (!(m_status & (STATUS_HAS_EVENTS | STATUS_WRITING_DATA)) && !m_midi_events.empty()) {
		m_tdr = m_midi_events.front();
		m_midi_events.pop();

		m_status |= STATUS_HAS_EVENTS;
	}

	if (m_txc != state)
	{
		m_txc = state;

		if (!state && m_divide > 0)
		{
			m_tx_counter++;

			switch (m_tx_state)
			{
			case STATE_START:
				m_tx_counter = 0;

				if (m_status & STATUS_HAS_EVENTS)
				{
					LOG("%s TX DATA %x\n", tag(), m_tdr);

					m_status = STATUS_WRITING_DATA;

					m_tx_state = STATE_DATA;
					m_tx_shift = m_tdr;
					m_tx_bits = 0;

					LOG("%s TX START BIT\n", tag());

					output_txd(0);
				}
				else
				{
					output_txd(1); // TX idle
				}
				break;

			case STATE_DATA:
				if (m_tx_counter == m_divide)
				{
					m_tx_counter = 0;

					if (m_tx_bits < m_bits)
					{
						output_txd((m_tx_shift >> m_tx_bits) & 1);

						m_tx_bits++;

						LOG("%s: TX DATA BIT %d %d\n", tag(), m_tx_bits, m_txd);
					}
					else
					{
						m_tx_state = STATE_STOP;
						m_tx_bits = 0;

						output_txd(1);
					}
				}
				break;

			case STATE_STOP:
				if (m_tx_counter == m_divide)
				{
					m_tx_counter = 0;

					m_tx_bits++;

					LOG("%s TX STOP BIT %d\n", tag(), m_tx_bits);

					if (m_tx_bits == m_stopbits)
					{
						m_status &= STATUS_HAS_EVENTS;
						m_status |= STATUS_IDLE;

						m_tx_state = STATE_START;
					}
				}
				break;
			}
		}
	}
}
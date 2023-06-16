// license:BSD-3-Clause
// copyright-holders:QUFB
/**********************************************************************

    PCM audio functions of the AP2010 LSI

**********************************************************************/

#include "emu.h"

#include "ap2010pcm.h"

#define VERBOSE (0)
#include "logmacro.h"

// device type definition
DEFINE_DEVICE_TYPE(AP2010PCM, ap2010pcm_device, "ap2010pcm", "AP2010 PCM")

ap2010pcm_device::ap2010pcm_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: device_t(mconfig, AP2010PCM, tag, owner, clock)
	, device_sound_interface(mconfig, *this)
	, m_fifo()
	, m_stream(nullptr)
{ }

void ap2010pcm_device::device_start()
{
	m_regs = make_unique_clear<uint32_t[]>(0x40/4);
	save_pointer(NAME(m_regs), 0x40/4);
	memset(m_regs.get(), 0, 0x40);

	m_sample_rate = 8000;
	save_item(NAME(m_sample_rate));

	m_stream = stream_alloc(0, 1, m_sample_rate);
}

void ap2010pcm_device::device_post_load()
{
	m_stream->set_sample_rate(m_sample_rate);
}

void ap2010pcm_device::sound_stream_update(sound_stream &stream, std::vector<read_stream_view> const &inputs, std::vector<write_stream_view> &outputs)
{
	auto &buffer = outputs[0];
	buffer.fill(0);

	int16_t sample = 0;
	uint16_t sample_empty_count = 0;
	uint32_t fifo_size = m_fifo.size();
	uint32_t fifo_fast_size = m_fifo_fast.size();
	for (size_t i = 0; i < buffer.samples(); i++)
	{
		if (!m_fifo_fast.empty()) {
			sample = m_fifo_fast.front();
			m_fifo_fast.pop();
		} else if (!m_fifo.empty()) {
			sample = m_fifo.front();
			m_fifo.pop();
		} else {
			sample = 0;
			sample_empty_count++;
		}

		buffer.put_int(i, sample * m_volume, 32768);
	}
	if (sample_empty_count > 0) {
		LOG("pcm 0s = %d (had %d + fast %d, needed %d)\n", sample_empty_count, fifo_size, fifo_fast_size, buffer.samples());
	}
}

uint32_t ap2010pcm_device::reg_r(offs_t offset)
{
	offset &= 0x3f;
	if (offset == 0) {
		// PCM data (0x5001000c) only received when 0x50010000 & 1 != 0;
		// PCM parameters (0x50010010, 0x50010018) only received when 0x50010000 & 4 != 0;
		return (m_regs[0x4/4] != 0) ? 0x0f : 0;
	}

	return m_regs[offset];
}

void ap2010pcm_device::reg_w(offs_t offset, uint32_t data, uint32_t mem_mask)
{
	offset &= 0x3f;
	COMBINE_DATA(&m_regs[offset]);

	m_stream->update();

	uint16_t sample = 0;
	switch (offset) {
		case 0x4/4:
			if ((data & 0x79) == 0x79) {
				m_sample_rate = 8000 * (1 + ((data & 2) >> 1));
				m_stream->set_sample_rate(m_sample_rate);

				// When a new stream starts, stop playback of previous stream
				m_fifo = {};

				LOG("pcm stream start, rate = %d\n", m_sample_rate);
			}
			break;
		case 0xc/4:
			if (ACCESSING_BITS_16_31) {
				sample = (data & 0xffff0000U) >> 16;
				if (sample != 0) {
					m_fifo.push(sample);
				}
			}
			if (ACCESSING_BITS_0_15) {
				sample = (data & 0x0000ffffU);
				if (sample != 0) {
					m_fifo.push(sample);
				}
			}
			break;
		// These samples are always played first
		case 0x10/4:
			if (ACCESSING_BITS_16_31) {
				sample = (data & 0xffff0000U) >> 16;
				if (sample != 0) {
					m_fifo_fast.push(sample);
				}
			}
			if (ACCESSING_BITS_0_15) {
				sample = (data & 0x0000ffffU);
				if (sample != 0) {
					m_fifo_fast.push(sample);
				}
			}
			break;
		// Volume control. When video output is disabled, it's possible to adjust volume
		// using the 2 touch areas on the bottom-left of the Storyware. Range 0..345
		case 0x18/4:
			m_volume = ((data & 0x1ff00000U) >> 20) / 345.0f;
			if (m_volume > 1.0f) {
				m_volume = 1.0f;
			}
			LOG("pcm vol = %08x -> %d", data, m_volume);
			break;
	}
}

// license:BSD-3-Clause
// copyright-holders:QUFB
/**********************************************************************

    PCM audio functions of the AP2010 LSI

    According to the Advanced Pico Beena's 2005-04-05 press release, it supports
    CELP with sample rates 8kHz and 16kHz. It is used as output for OGG files,
    which are decoded by the BIOS to signed 16-bit big endian PCM.

**********************************************************************/

#ifndef MAMESOUND_AP2010PCM_H
#define MAMESOUND_AP2010PCM_H

#pragma once

#include <queue>

class ap2010pcm_device : public device_t, public device_sound_interface
{
public:
	ap2010pcm_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock = 0);

	uint32_t reg_r(offs_t offset);
	void reg_w(offs_t offset, uint32_t data, uint32_t mem_mask = ~0);

protected:
	// device_t implementation
	virtual void device_start() override;
	virtual void device_post_load() override;

	// device_sound_interface implementation
	virtual void sound_stream_update(sound_stream &stream, std::vector<read_stream_view> const &inputs, std::vector<write_stream_view> &outputs) override;

private:
	std::unique_ptr<uint32_t[]> m_regs;

	float m_volume;
	uint32_t m_sample_rate;
	std::queue<int16_t> m_fifo, m_fifo_fast;
	sound_stream* m_stream;
};

DECLARE_DEVICE_TYPE(AP2010PCM, ap2010pcm_device)

#endif // MAMESOUND_AP2010PCM_H

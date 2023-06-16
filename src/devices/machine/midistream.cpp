// license:BSD-3-Clause
// copyright-holders:QUFB
/*********************************************************************

    midistream.c

    MIDI device that receives events on-demand as a stream of uint32_t bytes.

*********************************************************************/

#include "emu.h"

#include "midistream.h"

#define VERBOSE (0)
#include "logmacro.h"

DEFINE_DEVICE_TYPE(MIDI_STREAM, midi_stream_device, "midi_stream", "MIDI stream device")

namespace {

} // anonymous namespace

midi_stream_device::midi_stream_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: device_t(mconfig, MIDI_STREAM, tag, owner, clock)
	, m_write_handler(*this)
	, m_timer(nullptr)
	, m_timer_start_time(attotime::zero)
	, m_divisor(0)
	, m_last_type(0)
	, m_last_tick(0)
	, m_last_usec_per_quarter(1'000'000)
	, m_last_time(attotime::zero)
	, m_iterator(m_list.begin())
	, m_parser()
{
}

midi_stream_device::~midi_stream_device()
{
}

void midi_stream_device::device_start()
{
	m_timer = timer_alloc(FUNC(midi_stream_device::midi_update), this);
	m_timer->enable(false);
}

void midi_stream_device::device_reset()
{
	all_notes_off();
	clear();
}

TIMER_CALLBACK_MEMBER(midi_stream_device::midi_update)
{
	midi_event *event = current_event();
	if (event != nullptr)
	{
		// After a reset, the timer will still be relative to machine start time, so
		// we must discount that time
		attotime curtime = m_timer->expire() - m_timer_start_time;

		// If it's time to process the current event, do it and advance
		if (curtime >= event->time())
		{
			LOG("curtime = %lf, event time = %lf\n", curtime.as_double(), event->time().as_double());

			for (uint8_t curbyte : event->data())
			{
				m_write_handler(curbyte);
			}
			event = advance_event();
		}
	}
}

void midi_stream_device::clear()
{
	m_list.clear();
	m_last_event_i = 0;
	m_last_type = 0;
	m_last_tick = 0;
	m_last_usec_per_quarter = 1'000'000;
	m_last_time = attotime::zero;
	m_iterator = m_list.begin();
	m_parser.clear();
}

void midi_stream_device::update_divisor(uint32_t divisor)
{
	m_timer->adjust(attotime::zero, 0, attotime::from_hz(31250));
	m_timer->enable();
	m_timer_start_time = m_timer->expire();

	if ((divisor & 0x8000) != 0)
		throw midi_parser::error("SMPTE timecode time division not supported.");
	if (divisor == 0)
		throw midi_parser::error("Invalid time divisor of 0.");

	m_divisor = divisor & 0xffff;

	// Assuming that changing a divisor implies starting a new track
	clear();
}

midi_stream_device::midi_parser::midi_parser() :
	m_stream(1024), // Expecting several writes from an uninterrupted event stream
	m_offset(0)
{
}

//-------------------------------------------------
//  rewind - back up by the given number of bytes
//-------------------------------------------------

midi_stream_device::midi_parser &midi_stream_device::midi_parser::rewind(uint32_t count)
{
	count = std::min(count, m_offset);
	m_offset -= count;
	return *this;
}

//-------------------------------------------------
//  variable - Converts from variable-length quantity to u32.
//  These numbers are represented 7 bits per byte, most significant bits first.
//  All bytes except the last have bit 7 set, and the last byte has bit 7 clear.
//  If the number is in range 0..127, it is thus represented exactly
//  as one byte.
//-------------------------------------------------

uint32_t midi_stream_device::midi_parser::variable()
{
	uint64_t parsed = 0;
	uint32_t result = 0;
	for (int which = 0; which < 4; which++)
	{
		uint8_t curbyte = byte();
		result = (result << 7) | (curbyte & 0x7f);
		if ((curbyte & 0x80) == 0)
			return result;

		parsed = ((parsed << 8) & 0xffffffff) | curbyte;
	}

	std::ostringstream msg;
	util::stream_format(msg, "Invalid variable length field: %08x", parsed);
	throw error(std::move(msg).str().c_str());
}

//-------------------------------------------------
//  byte - read 8 bits of data
//-------------------------------------------------

uint8_t midi_stream_device::midi_parser::byte()
{
	check_bounds(1);

	uint8_t result = m_stream[m_offset];
	m_offset++;
	return result;
}

//-------------------------------------------------
//  word_be - read 16 bits of big-endian data
//-------------------------------------------------

uint16_t midi_stream_device::midi_parser::word_be()
{
	check_bounds(2);

	uint16_t result = (m_stream[m_offset] << 8) | m_stream[m_offset + 1];
	m_offset += 2;
	return result;
}

//-------------------------------------------------
//  triple_be - read 24 bits of big-endian data
//-------------------------------------------------

uint32_t midi_stream_device::midi_parser::triple_be()
{
	check_bounds(3);

	uint32_t result = (m_stream[m_offset] << 16) | (m_stream[m_offset + 1] << 8) | m_stream[m_offset + 2];
	m_offset += 3;
	return result;
}

//-------------------------------------------------
//  dword_be - read 32 bits of big-endian data
//-------------------------------------------------

uint32_t midi_stream_device::midi_parser::dword_be()
{
	check_bounds(4);

	uint32_t result = (m_stream[m_offset] << 24)
		| (m_stream[m_offset + 1] << 16)
		| (m_stream[m_offset + 2] << 8)
		| (m_stream[m_offset + 3]);
	m_offset += 4;
	return result;
}

//-------------------------------------------------
//  check_bounds - check to see if we have at least
//  'length' bytes left to consume; if not,
//  throw an error
//-------------------------------------------------

void midi_stream_device::midi_parser::check_bounds(uint32_t length)
{
	if (m_offset + length > m_stream.size()) {
		std::ostringstream msg;
		util::stream_format(msg,
				"Out of bounds: offset %d + length %d > stream size %d\n",
				m_offset,
				length,
				m_stream.size());
		throw error(std::move(msg).str().c_str());
	}
}

//-------------------------------------------------
//  passthrough_event - data already contains event
//  parsed, so we transmit as many bytes as expected
//  for the event type
//-------------------------------------------------

void midi_stream_device::passthrough_event(uint32_t data)
{
	uint8_t type = (data >> 24) & 0xff;
	uint8_t byte2 = (data >> 16) & 0xff;
	uint8_t byte3 = (data >> 8) & 0xff;
	switch(type & 0xf0) {
		case 0x80:
		case 0x90:
		case 0xa0:
		case 0xb0:
		case 0xe0:
			m_write_handler(type);
			m_write_handler(byte2);
			m_write_handler(byte3);
			break;
		case 0xc0:
		case 0xd0:
			m_write_handler(type);
			m_write_handler(byte2);
			break;
		default:
			osd_printf_error("%s unhandled type = %08x\n", tag(), data);
	}
}

//-------------------------------------------------
//  all_notes_off - sends control change events to
//  mute all notes for all channels
//-------------------------------------------------

void midi_stream_device::all_notes_off()
{
	for (uint8_t channel = 0; channel < 0x10; channel++)
	{
		m_write_handler(0xb0 | channel);
		m_write_handler(123);
		m_write_handler(0);
	}
}

//-------------------------------------------------
//  parse - Buffer and process event data
//-------------------------------------------------

std::error_condition midi_stream_device::parse(uint32_t data)
{
	try
	{
		// We might be pushing stray bytes here (e.g. null bytes after end of track event),
		// but we can only identify and handle them while parsing events.
		m_parser.push((data >> 24) & 0xff);
		m_parser.push((data >> 16) & 0xff);
		m_parser.push((data >>  8) & 0xff);
		m_parser.push((data >>  0) & 0xff);

		parse_buffered_data(m_parser);
		return std::error_condition();
	}
	catch (midi_parser::error &err)
	{
		LOG("parse error: %s\n", err.description());
		clear();
		return image_error::UNSPECIFIED;
	}
}

//-------------------------------------------------
//  parse_buffered_data - Parse events from buffered data.
//  If not enough bytes have been seen for a new event, we
//  keep track of the starting index of those incomplete bytes,
//  so that we can resume parsing after more bytes are buffered.
//-------------------------------------------------

uint32_t midi_stream_device::parse_buffered_data(midi_parser &buffer)
{
	uint32_t current_tick = m_last_tick;
	uint32_t last_offset = buffer.offset();
	try {
		while (!buffer.eob())
		{
			// Parse the time delta
			uint32_t delta = buffer.variable();
			current_tick += delta;
			midi_event event = midi_event(current_tick);

			// Handle running status
			uint8_t type = buffer.byte();
			if (BIT(type, 7) != 0)
				m_last_type = type;
			else
			{
				type = m_last_type;
				buffer.rewind(1);
			}

			// Determine the event class
			uint8_t eclass = type >> 4;
			if (eclass != 15)
			{
				// Simple events: all but program change and aftertouch have a second parameter
				event.append(type);
				event.append(buffer.byte());
				if (eclass != 12 && eclass != 13)
					event.append(buffer.byte());
			}
			else if (type != 0xff)
			{
				// Handle non-meta events
				event.append(type);
				uint32_t meta_length = buffer.variable();
				for (size_t i = 0; i < meta_length; i++) {
					event.append(buffer.byte());
				}
			}
			else
			{
				// Handle meta-events
				uint8_t type = buffer.byte();
				LOG("meta event type = %02x\n", type);

				// End of data?
				if (type == 0x2f) {
					// Clear stray bytes
					while (!buffer.eob())
						event.append(buffer.byte());
				}

				uint32_t meta_length = buffer.variable();
				if (type == 0x51)
				{
					// Tempo events
					uint32_t usec_per_quarter = buffer.triple_be();
					LOG("new tempo %lf\n", usec_per_quarter);
					if (usec_per_quarter != 0) {
						LOG("tempo %lf -> %lf\n", m_last_usec_per_quarter, usec_per_quarter);
						m_last_usec_per_quarter = usec_per_quarter;
					}
				}
				else
				{
					for (size_t i = 0; i < meta_length; i++) {
						event.append(buffer.byte());
					}
				}
			}

			// Update the elapsed time
			attotime tick_time = attotime::from_usec(m_last_usec_per_quarter) / m_divisor;
			m_last_time += tick_time * (current_tick - m_last_tick);
			event.set_time(m_last_time);
			LOG("current_tick %08x, last_tick %08x, time = %lf\n", current_tick, m_last_tick, m_last_time.as_double());

			// Successfully parsed full event; save it for playback
			m_list.push_back(event);
			m_last_tick = current_tick;
			LOG("parsed event type = %02x, size = %d (%d - %d)\n", type, buffer.offset() - last_offset, buffer.offset(), last_offset);

			last_offset = buffer.offset();
		}
	} catch (midi_parser::error &err) {
		LOG("stop parse @ %08x (%s)\n", buffer.offset(), err.description());

		// Reset offset to start of event, will be parsed again
		// once more data is available
		buffer.offset(last_offset);
	}

	m_iterator = std::next(m_list.begin(), m_last_event_i);

	return current_tick;
}

// license:BSD-3-Clause
// copyright-holders:QUFB
/*********************************************************************

    midistream.h

    MIDI device that receives events on-demand as a stream of uint32_t bytes.

    It is assumed that a stream contains a single track. Events are
    transmitted as soon as they have been parsed. Logic is similar to
    MIDI file handling present in "MIDI In image device".

*********************************************************************/

#ifndef MAME_MACHINE_MIDI_STREAM_H
#define MAME_MACHINE_MIDI_STREAM_H

#pragma once

#include "diserial.h"

#include <memory>
#include <mutex>
#include <string>
#include <system_error>
#include <utility>


/***************************************************************************
    TYPE DEFINITIONS
***************************************************************************/

class midi_stream_device : public device_t
{
public:
	// construction/destruction
	midi_stream_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock = 0);
	~midi_stream_device();

	typedef device_delegate<void (u8)> write_handler_delegate;
	template <typename... T> void set_write_handler(T &&... args) { m_write_handler.set(std::forward<T>(args)...); m_write_handler.resolve(); }

	// track state updates
	std::error_condition parse(uint32_t data);
	void passthrough_event(uint32_t data);
	void all_notes_off();
	void clear();
	void update_divisor(uint32_t data);

protected:
	// device_t implementation
	virtual void device_start() override;
	virtual void device_reset() override;

	TIMER_CALLBACK_MEMBER(midi_update);

private:
	// internal helper class for parsing
	class midi_parser
	{
	public:
		// minimal error class
		class error
		{
		public:
			error(char const *description) : m_description(description) { }
			char const *description() const { return m_description; }
		private:
			char const *m_description;
		};

		// construction
		midi_parser();

		// internal helpers
		void push(uint8_t data) { m_stream.push_back(data); }
		void clear() { m_stream.clear(); m_offset = 0; }
		bool eob() const { return (m_offset >= m_stream.size()); }
		midi_parser &rewind(uint32_t count);
		midi_parser &reset() { return rewind(m_offset); }
		uint32_t offset() { return m_offset; }
		void offset(uint32_t offset) { m_offset = offset; }

		// read data of various sizes and endiannesses
		uint8_t byte();
		uint16_t word_be();
		uint32_t triple_be();
		uint32_t dword_be();
		uint32_t dword_le();

		// variable-length quantity reader
		uint32_t variable();

	private:
		// internal helper
		void check_bounds(uint32_t length);

		// internal state
		std::vector<u8> m_stream;
		uint32_t m_offset;
	};

	// internal helper class reperesenting an event at a given
	// time containing MIDI data
	class midi_event
	{
	public:
		// constructor
		midi_event(uint32_t tick) : m_tick(tick) { }

		// simple getters
		uint32_t tick() const { return m_tick; }
		attotime const &time() const { return m_time; }
		std::vector<u8> const &data() const { return m_data; }

		// simple setters
		void set_time(attotime const &time) { m_time = time; }

		// append data to the buffer
		midi_event &append(uint8_t byte) { m_data.push_back(byte); return *this; }

	private:
		// internal state
		uint32_t m_tick;
		attotime m_time;
		std::vector<u8> m_data;
	};

	// event parsing helpers
	uint32_t parse_buffered_data(midi_parser &buffer);
	void rewind(attotime const &basetime);
	midi_event *current_event() const { return (m_iterator == m_list.end()) ? nullptr : &(*m_iterator); }
	midi_event *advance_event() {++m_last_event_i; ++m_iterator; return current_event(); }
	attotime const &duration() { return m_list.back().time(); }

	// transmission state
	write_handler_delegate m_write_handler;
	emu_timer *m_timer;
	attotime m_timer_start_time;

	// track state
	uint16_t m_divisor;
	uint8_t m_last_type;
	uint32_t m_last_tick;
	uint32_t m_last_usec_per_quarter;
	attotime m_last_time;
	uint32_t m_last_event_i;
	std::list<midi_event> m_list;
	std::list<midi_event>::iterator m_iterator;
	midi_parser m_parser;
};

// device type definition
DECLARE_DEVICE_TYPE(MIDI_STREAM, midi_stream_device)

// device iterator
typedef device_type_enumerator<midi_stream_device> midi_stream_device_enumerator;

#endif // MAME_MACHINE_MIDI_STREAM_H

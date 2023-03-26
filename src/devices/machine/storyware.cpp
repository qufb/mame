// license:BSD-3-Clause
// copyright-holders:QUFB
/**********************************************************************

    Storyware interface emulation, for use with the Sega Pico / Copera / Beena consoles.

**********************************************************************/

#include "emu.h"
#include "emuopts.h"

#include "crsshair.h"
#include "storyware.h"

DEFINE_DEVICE_TYPE(STORYWARE, storyware_device, "storyware", "Storyware")

storyware_device::storyware_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: device_t(mconfig, STORYWARE, tag, owner, clock)
	, m_booklet(*this, "booklet")
	, m_screen_storyware(*this, "screen_storyware")
	, m_settings_bitmap()
	, m_effective_page(0)
{ }

void storyware_device::device_add_mconfig(machine_config &config)
{
	BOOKLET(config, m_booklet, m_screen_w / 2, m_screen_h);

	SCREEN(config, m_screen_storyware, SCREEN_TYPE_RASTER);
	m_screen_storyware->set_refresh_hz(60);
	m_screen_storyware->set_vblank_time(ATTOSECONDS_IN_USEC(0));
	m_screen_storyware->set_size(m_screen_w, m_screen_h);
	m_screen_storyware->set_visarea(0, m_screen_w-1, 0, m_screen_h-1);
	m_screen_storyware->set_screen_update(FUNC(storyware_device::screen_update));
}

void storyware_device::device_start()
{
	save_item(NAME(m_screen_w));
	save_item(NAME(m_screen_h));

	save_item(NAME(m_effective_page));

	// Load the settings backdrop into a bitmap that is managed along with booklet pages.
	//
	// This bitmap needs to be blended with the screen's bitmap so that the
	// lightgun crosshair is also visible for it. This rules out other alternatives
	// such as specifying the settings backdrop as part of the layout.
	std::string dir;
	std::string settings_name = "settings.png";
	path_iterator path(machine().options().art_path());
	while (path.next(dir)) {
		emu_file file(dir, OPEN_FLAG_READ);
		if (!file.open(settings_name)) {
			m_booklet->load_page(m_settings_bitmap, file, settings_name);

			file.close();
			break;
		}
	}
}

void storyware_device::device_reset()
{
}

float storyware_device::pen_y_mapper(float linear_value)
{
	float adjusted_value = linear_value * 2;
	if (adjusted_value > 1.0f) {
		adjusted_value = 1.0f; // Constrain to top screen.
	}
	return adjusted_value;
}

void storyware_device::update_effective_page(uint32_t effective_page)
{
	m_effective_page = effective_page;
}

uint32_t storyware_device::screen_update(screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect)
{
	// Only show crosshair if the pen is making contact with the Storyware.
	int16_t pen_y = ioport(m_pen_y_tag)->read();
	int16_t pen_y_max = ioport(m_pen_y_tag)->field(0xff)->maxval() / 2;
	bool is_video_connected = !m_video_config_tag.empty() && ioport(m_video_config_tag)->read() == 0;
	machine().crosshair().get_crosshair(0).set_screen((pen_y > pen_y_max) && is_video_connected ? CROSSHAIR_SCREEN_NONE : &screen);

	bitmap.fill(rgb_t::white(), cliprect);

	bitmap_argb32 *left_bitmap = nullptr;
	bitmap_argb32 *right_bitmap = nullptr;
	int8_t page_i = 2 * m_effective_page - 1;
	if (page_i == -1) {
		// If the storyware is closed, then render front cover and settings backdrop.
		right_bitmap = m_booklet->page(0);
		if (m_settings_bitmap.valid()) {
			left_bitmap = &m_settings_bitmap;
		}
	} else {
		// If the storyware is open, show the back of the previous page and the front of the next page.
		// The last effective page can vary across games, but in those cases we always show the back of the last page.
		right_bitmap = m_booklet->page(page_i + 1);
		while (left_bitmap == nullptr) {
			left_bitmap  = m_booklet->page(page_i--);
		}
	}
	for (int y = cliprect.min_y; y <= cliprect.max_y; y++) {
		if (left_bitmap != nullptr) {
			for (int x = cliprect.min_x; x < (cliprect.max_x + 1) / 2; x++) {
				bitmap.pix(y, x) = (*left_bitmap).pix(y, x);
			}
		}
		if (right_bitmap != nullptr) {
			for (int x = (cliprect.max_x + 1) / 2; x <= cliprect.max_x; x++) {
				bitmap.pix(y, x) = (*right_bitmap).pix(y - 1, x);
			}
		}
	}

	return 0;
}

// license:BSD-3-Clause
// copyright-holders:QUFB
/**********************************************************************

    Storyware interface emulation, for use with the Sega Pico / Copera / Beena consoles.

    Given a directory path to the booklet's pages, the device will render them
    on a dedicated screen, according to the current effective page. Uses the same pen input port
    as the main / tablet screen so that movement is kept synchronized between screens.

    To ensure accurate mappings with interactable regions defined in games, images should be cropped
    to page borders and scaled to half the screen's width while preserving the screen height (e.g. 352/240).

**********************************************************************/

#ifndef MAME_MACHINE_STORYWARE_H
#define MAME_MACHINE_STORYWARE_H

#pragma once

#include "imagedev/booklet.h"
#include "rendutil.h"
#include "screen.h"

class storyware_device : public device_t
{
public:
	// construction/destruction
	storyware_device(
				const machine_config &mconfig,
				const char *tag,
				device_t *owner,
				const uint16_t screen_w,
				const uint16_t screen_h,
				const std::string pen_y_tag,
				const std::string video_config_tag,
				uint32_t clock = 0)
		: storyware_device(mconfig, tag, owner, clock) {
			m_screen_w = screen_w;
			m_screen_h = screen_h;
			m_pen_y_tag = pen_y_tag;
			m_video_config_tag = video_config_tag;
		}
	storyware_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock = 0);

	booklet_device *booklet() { return m_booklet.target(); }

	float pen_y_mapper(float linear_value);
	void update_effective_page(uint32_t effective_page);

protected:
	// device_t implementation
	virtual void device_start() override;
	virtual void device_reset() override;
	virtual void device_add_mconfig(machine_config &config) override;

private:
	required_device<booklet_device> m_booklet;
	required_device<screen_device> m_screen_storyware;
	bitmap_argb32 m_settings_bitmap;
	uint16_t m_screen_w;
	uint16_t m_screen_h;

	std::string m_pen_y_tag;
	std::string m_video_config_tag;

	uint32_t m_effective_page;

	uint32_t screen_update(screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect);
};

DECLARE_DEVICE_TYPE(STORYWARE, storyware_device)

#endif // MAME_MACHINE_STORYWARE_H

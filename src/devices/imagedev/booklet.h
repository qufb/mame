// license:BSD-3-Clause
// copyright-holders:QUFB
/**********************************************************************

    Interface to the booklet abstraction code. It represents a sequence of pages,
    where one PNG file is parsed for each page.

**********************************************************************/

#ifndef MAME_MACHINE_BOOKLET_H
#define MAME_MACHINE_BOOKLET_H

#pragma once

#include "fileio.h"
#include "path.h"
#include "rendutil.h"

class booklet_device : public device_t, public device_image_interface
{
public:
	// construction/destruction
	booklet_device(
				const machine_config &mconfig,
				const char *tag,
				device_t *owner,
				const uint16_t w,
				const uint16_t h,
				uint32_t clock = 0)
		: booklet_device(mconfig, tag, owner, clock) {
			m_page_w = w;
			m_page_h = h;
		}
	booklet_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock = 0);

	// device_image_interface implementation
	virtual bool is_readable()  const noexcept override { return true; }
	virtual bool is_writeable() const noexcept override { return false; }
	virtual bool is_creatable() const noexcept override { return false; }
	virtual bool is_reset_on_load() const noexcept override { return false; }
	virtual const char *file_extensions() const noexcept override { return "png"; }
	virtual const char *image_type_name() const noexcept override { return "booklet"; }
	virtual const char *image_brief_type_name() const noexcept override { return "bklt"; }

	void create_pages(std::string booklet_path);
	void create_page(util::random_read &io, std::string_view name);
	void load_page(bitmap_argb32 &resampled_bitmap, util::random_read &io, std::string_view name);
	bitmap_argb32* page(uint8_t page_i);
	bool has_pages();

protected:
	// device_t implementation
	virtual void device_start() override;
	virtual void device_reset() override;
	virtual void device_add_mconfig(machine_config &config) override;

private:
	bitmap_argb32 m_page_bitmaps[14];
	uint16_t m_page_w;
	uint16_t m_page_h;
	int8_t m_page_count;
};

DECLARE_DEVICE_TYPE(BOOKLET, booklet_device)

#endif // MAME_MACHINE_BOOKLET_H

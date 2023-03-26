// license:BSD-3-Clause
// copyright-holders:QUFB
/**********************************************************************

    Interface to the booklet abstraction code.

**********************************************************************/

#include "emu.h"

#include "booklet.h"

DEFINE_DEVICE_TYPE(BOOKLET, booklet_device, "booklet", "Booklet")

booklet_device::booklet_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: device_t(mconfig, BOOKLET, tag, owner, clock)
	, device_image_interface(mconfig, *this)
	, m_page_count(0)
{ }

void booklet_device::device_add_mconfig(machine_config &config)
{
}

void booklet_device::device_start()
{
	save_item(NAME(m_page_w));
	save_item(NAME(m_page_h));
	save_item(NAME(m_page_count));
}

void booklet_device::device_reset()
{
}

void booklet_device::create_pages(std::string booklet_path)
{
	file_enumerator path(booklet_path);
	std::vector<std::string> names;
	for (osd::directory::entry const *dir = path.next(); dir; dir = path.next()) {
		std::string name(dir->name);
		if (core_filename_ends_with(name, ".png")) {
			names.push_back(name);
		}
	}

	// Since directory entries aren't iterated in order,
	// ensure their names are sorted before pages are allocated.
	std::sort(names.begin(), names.end());

	for (std::string name : names) {
		emu_file file(booklet_path, OPEN_FLAG_READ);
		if (!file.open(name)) {
			create_page(file, name);

			file.close();
		}
	}
}

void booklet_device::create_page(util::random_read &io, std::string_view name)
{
	load_page(m_page_bitmaps[m_page_count], io, name);
	m_page_count++;
}

void booklet_device::load_page(bitmap_argb32 &resampled_bitmap, util::random_read &io, std::string_view name)
{
	osd_printf_verbose("Loading page '%s'...\n", name);

	bitmap_argb32 page_bitmap;
	render_load_png(page_bitmap, io);
	if (!page_bitmap.valid()) {
		osd_printf_warning("Skipped invalid page '%s'.\n", name);
		return;
	}

	int32_t expected_ratio_w = m_page_w;
	int32_t expected_ratio_h = m_page_h;
	util::reduce_fraction(expected_ratio_w, expected_ratio_h);

	int32_t page_ratio_w = page_bitmap.width();
	int32_t page_ratio_h = page_bitmap.height();
	util::reduce_fraction(page_ratio_w, page_ratio_h);
	if (page_ratio_w != expected_ratio_w || page_ratio_h != expected_ratio_h) {
		osd_printf_warning(
			"Page '%s' has ratio %d:%d, expected ratio %d:%d. Pen mappings will likely be inaccurate.\n",
			name,
			page_ratio_w,
			page_ratio_h,
			expected_ratio_w,
			expected_ratio_h);
	}

	resampled_bitmap.allocate(m_page_w, m_page_h);
	render_resample_argb_bitmap_hq(
		resampled_bitmap,
		page_bitmap,
		render_color{ 1.0F, 1.0F, 1.0F, 1.0F },
		false);
}

bitmap_argb32* booklet_device::page(uint8_t page_i)
{
	if (page_i > m_page_count) {
		return nullptr;
	}

	return m_page_bitmaps[page_i].valid() ? &m_page_bitmaps[page_i] : nullptr;
}

bool booklet_device::has_pages() {
	return m_page_count > 0;
}

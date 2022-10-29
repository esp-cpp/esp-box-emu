#include "hal/spi_types.h"
#include "spi_host_cxx.hpp"

#include "display.hpp"
#include "st7789.hpp"

#include "spi_lcd.h"

static constexpr size_t display_width = 320;
static constexpr size_t display_height = 240;
static constexpr size_t pixel_buffer_size = display_width*NUM_ROWS_IN_FRAME_BUFFER;
std::unique_ptr<idf::SPIMaster> master;
std::shared_ptr<idf::SPIDevice> lcd_;
std::shared_ptr<espp::Display> display;

// for gnuboy
uint16_t* displayBuffer[2];
struct fb
{
	uint8_t *ptr;
	int w, h;
	int pelsize;
	int pitch;
	int indexed;
	struct
	{
		int l, r;
	} cc[4];
	int yuv;
	int enabled;
	int dirty;
};
struct fb fb;
struct obj
{
	uint8_t y;
	uint8_t x;
	uint8_t pat;
	uint8_t flags;
};
struct lcd
{
	uint8_t vbank[2][8192];
	union
	{
		uint8_t mem[256];
		struct obj obj[40];
	} oam;
	uint8_t pal[128];
};
static struct lcd lcd;
int frame = 0;

// TODO: see if IRAM_ATTR improves the display refresh frequency
// create the lcd_write function
extern "C" void lcd_write(const uint8_t *data, size_t length, uint16_t user_data) {
    if (length == 0) {
        // oddly the esp-idf-cxx spi driver asserts if we try to send 0 data...
        return;
    }
    // NOTE: we could simply provide user_data as context to the function
    // NOTE: if we don't call get() to block for the transaction, then the
    // transaction will go out scope and fail.
    lcd_->transfer(data, data+length, nullptr,
                  [](void* ud) {
                      uint32_t flags = (uint32_t)ud;
                      if (flags & (uint32_t)espp::Display::Signal::FLUSH) {
                          lv_disp_t * disp = _lv_refr_get_disp_refreshing();
                          lv_disp_flush_ready(disp->driver);
                      }
                  },
                  (void*)user_data).get();
}

#define U16x2toU32(m,l) ((((uint32_t)(l>>8|(l&0xFF)<<8))<<16)|(m>>8|(m&0xFF)<<8))

extern "C" uint16_t make_color(uint8_t r, uint8_t g, uint8_t b) {
    return lv_color_make(r,g,b).full;
}

extern "C" void set_pixel(const uint16_t x, const uint16_t y, const uint16_t color) {
    uint32_t xv,yv;
    xv = U16x2toU32(x,x);
    yv = U16x2toU32(y,y);
    // Send command for column address set
    espp::St7789::send_command((uint8_t)espp::St7789::Command::caset);
    // send data for column address set
    espp::St7789::send_data((uint8_t*)&xv, 4);
    // send command for row address set
    espp::St7789::send_command((uint8_t)espp::St7789::Command::raset);
    // send data for row address set
    espp::St7789::send_data((uint8_t*)&yv, 4);
    // send command for ram write
    espp::St7789::send_command((uint8_t)espp::St7789::Command::ramwr);
    // now send the actual color data
    espp::St7789::send_data((uint8_t *)&color, 2);
}

extern "C" uint16_t* get_vram0() {
    return displayBuffer[0];
}

extern "C" uint16_t* get_vram1() {
    return displayBuffer[1];
}

extern "C" void delay_us(size_t num_us) {
    using namespace std::chrono_literals;
    std::this_thread::sleep_for(1us * num_us);
}

extern "C" void lcd_set_drawing_frame(const uint16_t xs, const uint16_t ys, const uint16_t width, const uint16_t height) {
    espp::St7789::set_drawing_area(xs, ys, xs+width, ys+height);
    espp::St7789::send_command((uint8_t)espp::St7789::Command::ramwr);
}

extern "C" uint16_t reorder_color(uint16_t color) {
    return (color & 0xFF << 8) | (color >> 8);
}

extern "C" void lcd_continue_writing(const uint8_t *buffer, size_t buffer_length) {
    // espp::St7789::send_command((uint8_t)espp::St7789::Command::ramwr);
    // espp::St7789::send_command((uint8_t)espp::St7789::Command::ramwrc);
    // send command for memory write continue
    // now send the actual color data
    espp::St7789::send_data(buffer, buffer_length);
}

extern "C" void lcd_write_frame(const uint16_t xs, const uint16_t ys, const uint16_t width, const uint16_t height, const uint8_t * data){
    if (data) {
        // have data, fill the area with the color data
        lv_area_t area {
            .x1 = (lv_coord_t)(xs),
            .y1 = (lv_coord_t)(ys),
            .x2 = (lv_coord_t)(xs+width-1),
            .y2 = (lv_coord_t)(ys+height-1)};
        espp::St7789::fill(nullptr, &area, (lv_color_t*)data);
    } else {
        // don't have data, so clear the area (set to 0)
        espp::St7789::clear(xs, ys, width, height);
    }
}

static bool initialized = false;
extern "C" void lcd_init() {
    if (initialized) {
        return;
    }
    master = std::make_unique<idf::SPIMaster>(idf::SPINum(SPI2_HOST),
                                              idf::MOSI(6),
                                              idf::MISO(18),
                                              idf::SCLK(7),
                                              idf::SPI_DMAConfig::AUTO(),
                                              idf::SPITransferSize(pixel_buffer_size * sizeof(lv_color_t)));
    lcd_ = master->create_dev(idf::CS(5), idf::Frequency::MHz(60));
    // initialize the controller
    espp::St7789::initialize({
            .lcd_write = lcd_write,
            .reset_pin = (gpio_num_t)48,
            .data_command_pin = (gpio_num_t)4,
            .backlight_pin = (gpio_num_t)45,
            .backlight_on_value = true,
            .invert_colors = true,
            .mirror_x = true,
            .mirror_y = true,
        });
    // initialize the display / lvgl
    using namespace std::chrono_literals;
    display = std::make_shared<espp::Display>(espp::Display::AllocatingConfig{
            .width = display_width,
            .height = display_height,
            .pixel_buffer_size = pixel_buffer_size,
            .flush_callback = espp::St7789::flush,
            .update_period = 10ms,
            .double_buffered = true,
            .allocation_flags = MALLOC_CAP_8BIT | MALLOC_CAP_DMA,
            //.allocation_flags = MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM, // MALLOC_CAP_INTERNAL, MALLOC_CAP_DMA
            .rotation = espp::Display::Rotation::LANDSCAPE,
            .software_rotation_enabled = true,
        });
    initialized = true;
    // for gnuboy
    displayBuffer[0] = display->vram0();
    displayBuffer[1] = display->vram1();
    memset(&fb, 0, sizeof(fb));
    // got these from https://github.com/OtherCrashOverride/go-play/blob/master/gnuboy-go/main/main.c
    fb.w = 160;
    fb.h = 144;
    fb.pelsize = 2;
    fb.pitch = fb.w * fb.pelsize;
    fb.indexed = 0;
    fb.ptr = (uint8_t*)displayBuffer[0];
    fb.enabled = 1;
    fb.dirty = 0;
}

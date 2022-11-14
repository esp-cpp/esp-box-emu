#include "hal/spi_types.h"
#include "driver/spi_master.h"

#include "display.hpp"
#include "st7789.hpp"

#include "spi_lcd.h"

static spi_device_handle_t spi;

static constexpr size_t display_width = 320;
static constexpr size_t display_height = 240;
static constexpr size_t pixel_buffer_size = display_width*NUM_ROWS_IN_FRAME_BUFFER;
std::shared_ptr<espp::Display> display;

// static constexpr size_t frame_buffer_size = (((256 + 8) * 2) * 240);
static constexpr size_t frame_buffer_size = (((320) * 2) * 240);
static uint8_t *frame_buffer0;
static uint8_t *frame_buffer1;

//This function is called (in irq context!) just before a transmission starts. It will
//set the D/C line to the value indicated in the user field.
void lcd_spi_pre_transfer_callback(spi_transaction_t *t)
{
    // int dc=(int)t->user;
    // gpio_set_level(PIN_NUM_DC, dc);
}

//This function is called (in irq context!) just before a transmission starts. It will
//set the D/C line to the value indicated in the user field.
void lcd_spi_post_transfer_callback(spi_transaction_t *t)
{
    // int dc=(int)t->user;
    // gpio_set_level(PIN_NUM_DC, dc);
    uint32_t flags = (uint32_t)t->user;
    if (flags & (uint32_t)espp::Display::Signal::FLUSH) {
        lv_disp_t * disp = _lv_refr_get_disp_refreshing();
        lv_disp_flush_ready(disp->driver);
    }
}


static bool polling_transmit = true;
extern "C" void lcd_set_polling_transmit() {
    polling_transmit = true;
}
extern "C" void lcd_set_queued_transmit() {
    polling_transmit = false;
}

// create the lcd_write function
static const int spi_queue_size = 10;
static spi_transaction_t ts_[spi_queue_size];
static size_t ts_index = 0;
extern "C" void IRAM_ATTR lcd_write(const uint8_t *data, size_t length, uint16_t user_data) {
    if (length == 0) {
        return;
    }
    esp_err_t ret;
    spi_transaction_t *t = &ts_[ts_index];
    memset(t, 0, sizeof(*t));
    t->length = length * 8;
    t->tx_buffer = data;
    // as seen in the post_transfer_callback (above) this is only used to
    // determine whether or not to flush the lvgl system.
    t->user = (void*)user_data;
    if (polling_transmit) {
        ret=spi_device_polling_transmit(spi, t);
    } else {
        ret=spi_device_queue_trans(spi, t, portMAX_DELAY);
    }
    if (ret != ESP_OK) {
        fmt::print("Could not write to lcd: {} '{}'\n", ret, esp_err_to_name(ret));
        fmt::print("          Biggest /     Free /    Total\n"
                   "DRAM  : [{:8d} / {:8d} / {:8d}]\n"
                   "PSRAM : [{:8d} / {:8d} / {:8d}]\n",
                   heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
                   heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                   heap_caps_get_total_size(MALLOC_CAP_INTERNAL),
                   heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM),
                   heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
                   heap_caps_get_total_size(MALLOC_CAP_SPIRAM));
    }
    ts_index++;
    if (ts_index >= spi_queue_size) ts_index = 0;
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
    return display->vram0();
}

extern "C" uint16_t* get_vram1() {
    return display->vram1();
}

extern "C" uint8_t* get_frame_buffer0() {
    return frame_buffer0;
}

extern "C" uint8_t* get_frame_buffer1() {
    return frame_buffer1;
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
    esp_err_t ret;
    spi_bus_config_t buscfg={
        .mosi_io_num=GPIO_NUM_6,
        .miso_io_num=-1,
        .sclk_io_num=GPIO_NUM_7,
        .quadwp_io_num=-1,
        .quadhd_io_num=-1,
        .max_transfer_sz=pixel_buffer_size * sizeof(lv_color_t)
    };
    static spi_device_interface_config_t devcfg={
        .mode=0,
        .clock_speed_hz=60*1000*1000,
        .spics_io_num=GPIO_NUM_5,
        .queue_size=spi_queue_size,
        .pre_cb=lcd_spi_pre_transfer_callback,
        .post_cb=lcd_spi_post_transfer_callback,
    };
    //Initialize the SPI bus
    ret=spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
    ESP_ERROR_CHECK(ret);
    //Attach the LCD to the SPI bus
    ret=spi_bus_add_device(SPI2_HOST, &devcfg, &spi);
    ESP_ERROR_CHECK(ret);
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
            .update_period = 5ms,
            .double_buffered = true,
            // .allocation_flags = MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM,
            .allocation_flags = MALLOC_CAP_8BIT | MALLOC_CAP_DMA,
            .rotation = espp::Display::Rotation::LANDSCAPE,
            .software_rotation_enabled = true,
        });

    frame_buffer0 = (uint8_t*)heap_caps_malloc(frame_buffer_size, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
    frame_buffer1 = (uint8_t*)heap_caps_malloc(frame_buffer_size, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
    initialized = true;
}

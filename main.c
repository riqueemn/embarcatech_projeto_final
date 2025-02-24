#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "ws2812.pio.h"

#define JOYSTICK_X 26
#define JOYSTICK_Y 27
#define LED_R 13
#define LED_G 12
#define LED_B 11
#define BUZZER 10
#define WS2812_PIN 7
#define NUM_PIXELS 25
#define IS_RGBW false

uint32_t led_buffer[NUM_PIXELS];  // Buffer de cores para DMA
PIO pio = pio0;
uint sm;
uint dma_chan;

// Função para converter RGB em GRB (formato do WS2812)
static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)(g) << 16) | ((uint32_t)(r) << 8) | (uint32_t)(b);
}

// Configuração do DMA para transferir os dados para a PIO
void setup_dma() {
    dma_chan = dma_claim_unused_channel(true);
    dma_channel_config c = dma_channel_get_default_config(dma_chan);
    
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
    channel_config_set_dreq(&c, pio_get_dreq(pio, sm, true));

    dma_channel_configure(
        dma_chan, &c,
        &pio->txf[sm],   // Destino: FIFO da PIO
        led_buffer,      // Fonte: buffer de LEDs
        NUM_PIXELS,      // Número de transferências
        false            // Não iniciar ainda
    );
}

// Dispara a transferência do DMA
void start_dma_transfer() {
    dma_channel_set_read_addr(dma_chan, led_buffer, true);
}

// Preenche o buffer de LEDs com um mapa de setas
void draw_arrow(const uint8_t *arrow_map) {
    uint32_t red = urgb_u32(4, 0, 0);
    uint32_t off = urgb_u32(0, 0, 0);

    for (int i = 0; i < NUM_PIXELS; i++) {
        led_buffer[i] = arrow_map[i] ? red : off;
    }
    
    start_dma_transfer(); // Inicia a transferência via DMA
}

// Mapas das setas (1 = LED aceso, 0 = LED apagado)
const uint8_t arrow_left[25] = {
    0, 0, 1, 0, 0,
    0, 0, 1, 0, 0,
    1, 1, 1, 1, 1,
    0, 1, 1, 1, 0,
    0, 0, 1, 0, 0
};

const uint8_t arrow_right[25] = {
    0, 0, 1, 0, 0,
    0, 0, 1, 1, 0,
    1, 1, 1, 1, 1,
    0, 0, 1, 1, 0,
    0, 0, 1, 0, 0
};

const uint8_t arrow_down[25] = {
    0, 0, 1, 0, 0,
    0, 1, 1, 1, 0,
    1, 1, 1, 1, 1,
    0, 0, 1, 0, 0,
    0, 0, 1, 0, 0
};

const uint8_t arrow_up[25] = {
    0, 0, 1, 0, 0,
    0, 1, 1, 0, 0,
    1, 1, 1, 1, 1,
    0, 1, 1, 0, 0,
    0, 0, 1, 0, 0
};

// Inicializa o joystick
void init_joystick() {
    adc_init();
    adc_gpio_init(JOYSTICK_X);
    adc_gpio_init(JOYSTICK_Y);
}

// Lê os valores do joystick
uint16_t read_joystick_x() {
    adc_select_input(0);
    return adc_read();
}

uint16_t read_joystick_y() {
    adc_select_input(1);
    return adc_read();
}

// Configura LEDs
void init_leds() {
    gpio_set_function(LED_R, GPIO_FUNC_PWM);
    gpio_set_function(LED_G, GPIO_FUNC_PWM);
    gpio_set_function(LED_B, GPIO_FUNC_PWM);
    
    uint slice_r = pwm_gpio_to_slice_num(LED_R);
    uint slice_g = pwm_gpio_to_slice_num(LED_G);
    uint slice_b = pwm_gpio_to_slice_num(LED_B);
    
    pwm_set_wrap(slice_r, 255);
    pwm_set_wrap(slice_g, 255);
    pwm_set_wrap(slice_b, 255);
    
    pwm_set_enabled(slice_r, true);
    pwm_set_enabled(slice_g, true);
    pwm_set_enabled(slice_b, true);
}

void set_led_brightness(uint8_t r, uint8_t g, uint8_t b) {
    pwm_set_gpio_level(LED_R, r);
    pwm_set_gpio_level(LED_G, g);
    pwm_set_gpio_level(LED_B, b);
}

// Controla as cores dos LEDs
void set_led_color(bool r, bool g, bool b) {
    gpio_put(LED_R, r);
    gpio_put(LED_G, g);
    gpio_put(LED_B, b);
}

// Configura o buzzer
void init_buzzer() {
    gpio_set_function(BUZZER, GPIO_FUNC_PWM);
    uint slice = pwm_gpio_to_slice_num(BUZZER);
    pwm_set_wrap(slice, 12500);
    pwm_set_enabled(slice, true);
}

// Beep do buzzer
void beep(uint duration_ms) {
    uint slice = pwm_gpio_to_slice_num(BUZZER);
    pwm_set_gpio_level(BUZZER, 6250);
    sleep_ms(duration_ms);
    pwm_set_gpio_level(BUZZER, 0);
}

int main() {
    stdio_init_all();
    init_joystick();
    init_leds();
    init_buzzer();

    // Inicializa PIO para WS2812
    sm = pio_claim_unused_sm(pio, true);
    uint offset = pio_add_program(pio, &ws2812_program);
    ws2812_program_init(pio, sm, offset, WS2812_PIN, 800000, IS_RGBW);
    
    setup_dma(); // Configura DMA

    while (true) {
        uint16_t x = read_joystick_x();
        uint16_t y = read_joystick_y();

        if (y > 3000) {  // Para cima
            set_led_brightness(15, 0, 0);
            draw_arrow(arrow_right);
            beep(100);
        } else if (y < 1000) {  // Para baixo
            set_led_brightness(0, 15, 0);
            draw_arrow(arrow_up);
            beep(100);
        } else if (x > 3000) {  // Direita
            set_led_brightness(0, 0, 15);
            draw_arrow(arrow_left);
            beep(100);
        } else if (x < 1000) {  // Esquerda
            set_led_brightness(15, 15, 0);
            draw_arrow(arrow_down);
            beep(100);
        } else {
            set_led_color(false, false, false);
        }

        sleep_ms(100);
    }
}

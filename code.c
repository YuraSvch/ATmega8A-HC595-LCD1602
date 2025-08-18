#define F_CPU 8000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>

#define LATCH_PIN PB2

// Біти виходу 74HC595
#define Q_RS 0
#define Q_E  1
#define Q_D4 2
#define Q_D5 3
#define Q_D6 4
#define Q_D7 5

static inline void spi_init(void) {
    // PB2(SS), PB3(MOSI), PB5(SCK) - виходи; PB4(MISO) - вхід
    DDRB |= (1<<LATCH_PIN) | (1<<PB3) | (1<<PB5);
    DDRB &= ~(1<<PB4);

    // Початковий стан SS=0
    PORTB &= ~(1<<LATCH_PIN);

    SPCR = (1<<SPE) | (1<<MSTR) | (1<<SPR1);   // SPR1=1, SPR0=0 -> /64
    SPSR = 0;
}

static inline void shift595_write(uint8_t v) {
    //SS низький під час зсуву
    PORTB &= ~(1<<LATCH_PIN);
    SPDR = v;
    while(!(SPSR & (1<<SPIF)));
    // Копіювання у вихідні тригери по фронту STCP
    PORTB |=  (1<<LATCH_PIN);
    _delay_us(1);
    PORTB &= ~(1<<LATCH_PIN);
}

// Збираємо байт для 74HC595: RS, E, D4..D7
static inline uint8_t pack_595(uint8_t rs, uint8_t e, uint8_t nibble4) {
    // nibble4: молодші 4 біти -> на Q2..Q5
    uint8_t v = 0;
    v |= (rs & 1) << Q_RS;
    v |= (e  & 1) << Q_E;
    v |= (nibble4 & 0x0F) << Q_D4; // Q2..Q5
    return v;
}

static inline void lcd_write4(uint8_t rs, uint8_t nibble) {
    // E=0: виставляємо дані й RS
    shift595_write(pack_595(rs, 0, nibble));
    // Строб E: 1 -> 0
    shift595_write(pack_595(rs, 1, nibble));
    _delay_us(1);  // ширина імпульсу E
    shift595_write(pack_595(rs, 0, nibble));
    _delay_us(40); // час виконання коротких інструкцій/запису даних
}

static inline void lcd_cmd(uint8_t cmd) {
    lcd_write4(0, (cmd >> 4) & 0x0F);
    lcd_write4(0,  cmd       & 0x0F);

    if (cmd == 0x01 || cmd == 0x02) {
        _delay_ms(2);
    }
}

static inline void lcd_data(uint8_t data) {
    lcd_write4(1, (data >> 4) & 0x0F);
    lcd_write4(1,  data       & 0x0F);
}

static void lcd_init(void) {
    _delay_ms(20); 

    // Початкові 8-бітні "пінги" (лише старший нібл)
    lcd_write4(0, 0x03); _delay_ms(5);
    lcd_write4(0, 0x03); _delay_us(150);
    lcd_write4(0, 0x03); _delay_us(150);

    // Перехід у 4-бітний режим
    lcd_write4(0, 0x02); _delay_us(150);

    // Набір функцій: 4-біти, 2 лінії, 5x8
    lcd_cmd(0x28);
    // Дисплей вимкнено
    lcd_cmd(0x08);
    // Очищення
    lcd_cmd(0x01);
    lcd_cmd(0x06);
    // Дисплей увімкнено, курсор вимкнено, блимання вимкнено
    lcd_cmd(0x0C);
}

int main(void) {
    spi_init();
    lcd_init();

    // Виводимо "Hello" у (0,0)
    const char *s = "Hello";
    while (*s) {
        lcd_data((uint8_t)*s++);
    }

    while (1) {
        // Залишаємо(безкінечно оновлюємо) напис
    }
}

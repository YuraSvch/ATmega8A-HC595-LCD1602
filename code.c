#define F_CPU 8000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>

/* ===== ATmega8A SPI pins =====
   MOSI=PB3 - SER(14) 74HC595
   SCK =PB5 - SHCP(11)
   SS  =PB2 - STCP(12) (Latch)
   74HC595: OE(13) - GND, MR(10) - VCC, VCC(16) - +5V, GND(8) - GND
   LCD: RW - GND, V0 через потенціометр
*/

#define SPI_PORT PORTB
#define SPI_DDR  DDRB
#define SPI_MOSI PB3
#define SPI_SCK  PB5
#define SPI_LCH  PB2   // STCP / Latch

/* ===== LCD ? 74HC595 mapping =====
   RS=Q0, E=Q1, D4..D7=Q2..Q5
*/
#define LCD_RS_Q 0
#define LCD_E_Q  1
#define LCD_D4_Q 2
#define LCD_D5_Q 3
#define LCD_D6_Q 4
#define LCD_D7_Q 5
#define LCD_DATA_MASK ((1u<<LCD_D4_Q)|(1u<<LCD_D5_Q)|(1u<<LCD_D6_Q)|(1u<<LCD_D7_Q))

static volatile uint8_t BUS = 0;

static inline void spi_init(void){
    SPI_DDR |= (1<<SPI_MOSI) | (1<<SPI_SCK) | (1<<SPI_LCH); // виходи
    SPI_PORT |= (1<<SPI_LCH);                               // LATCH=1
    // SPI: Enable, Master, Mode0, MSB-first, SCK=F_CPU/64
    SPCR = (1<<SPE) | (1<<MSTR) | (1<<SPR1);
    SPSR = 0;
}

/* Лачимо ЗАВЖДИ після повної передачі байта (виходи змінюються атомарно) */
static inline void sh595_apply(void){
    uint8_t val = BUS & ~( (1u<<6) | (1u<<7) );   // Q6,Q7 завжди 0 (незадіяні)
    SPI_PORT &= ~(1<<SPI_LCH);                    // LATCH=0 (не оновлювати під час зсуву)
    SPDR = val;                                   // MSB-first ? біт i лягає у Qi
    while(!(SPSR & (1<<SPIF))){}
    SPI_PORT |= (1<<SPI_LCH);                     // LATCH=1 (оновити виходи разом)
}

static inline void lcd_pulse_E(void){
    BUS |=  (1u<<LCD_E_Q); sh595_apply(); _delay_us(1);  // E=1
    BUS &= ~(1u<<LCD_E_Q); sh595_apply(); _delay_us(40); // E=0 + пауза між ніблів
}

/* RS ? дані D4..D7 ? імпульс E */
static inline void lcd_write_nibble(uint8_t nib, uint8_t rs){
    if(rs) BUS |=  (1u<<LCD_RS_Q);
    else   BUS &= ~(1u<<LCD_RS_Q);

    BUS &= ~LCD_DATA_MASK;                 // очистити D4..D7
    if(nib & 0x01) BUS |= (1u<<LCD_D4_Q);  // b0 -> D4
    if(nib & 0x02) BUS |= (1u<<LCD_D5_Q);  // b1 -> D5
    if(nib & 0x04) BUS |= (1u<<LCD_D6_Q);  // b2 -> D6
    if(nib & 0x08) BUS |= (1u<<LCD_D7_Q);  // b3 -> D7

    sh595_apply();           // дані стабільні при E=0
    _delay_us(1);            // t_AS
    lcd_pulse_E();           // захопити нібл
}

static inline void lcd_cmd(uint8_t c){
    lcd_write_nibble(c>>4, 0);
    lcd_write_nibble(c&0x0F, 0);
    if(c==0x01 || c==0x02) _delay_ms(2);   // довгі команди
}
static inline void lcd_data(uint8_t d){
    lcd_write_nibble(d>>4, 1);
    lcd_write_nibble(d&0x0F, 1);
}
static inline void lcd_goto(uint8_t x, uint8_t y){
    lcd_cmd((y?0xC0:0x80)+x);
}

static inline void lcd_init(void){
    spi_init();
    BUS = 0; sh595_apply();
    _delay_ms(50);

    // Вхід у 4-бітний: верхні ніблі 0x3,0x3,0x3,0x2 (з запасом по паузах)
    lcd_write_nibble(0x03,0); _delay_ms(5);
    lcd_write_nibble(0x03,0); _delay_us(150);
    lcd_write_nibble(0x03,0); _delay_us(150);
    lcd_write_nibble(0x02,0); _delay_us(150);

    // База
    lcd_cmd(0x28);  // 4-біт, 2 рядки, 5x8
    lcd_cmd(0x0C);  // дисплей ON, курсор OFF
    lcd_cmd(0x06);  // автоінкремент
    lcd_cmd(0x01);  // очистка
    _delay_ms(2);
}

int main(void){
    lcd_init();

    lcd_goto(0,0);
    const char *s = "Hello Volodymyr";
    while(*s) lcd_data((uint8_t)*s++);

    while(1){}
}

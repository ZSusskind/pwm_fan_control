static const uint8_t pwm_period = 79; // 16 MHz / 25 KHz / 8 (prescalar value) - 1
static const uint8_t min_duty_cycle = 10;

static const uint8_t shutoff_temp = 40;
static const uint8_t low_temp = 46;
static const uint8_t high_temp = 80;

static bool data_rx = false;
static uint8_t missed_count = 0;
static bool alarm = false;
static bool shutoff = false;

ISR(TIMER1_COMPA_vect) {
    if (!data_rx) { // Haven't received new data since the last timer interrupt
        missed_count++; // yes, this can eventually overflow - it doesn't really matter
        if (missed_count == 3) {
            alarm = true;
        }
    } else {
        missed_count = 0;
    }
    data_rx = false;
}

void set_duty_cycle(uint8_t duty_cycle) {
    //if (duty_cycle < min_duty_cycle) duty_cycle = min_duty_cycle;
    if (duty_cycle > 100) duty_cycle = 100;
    uint16_t period = pwm_period + 1;
    period = (period * duty_cycle) / 100;
    OCR0B = period;
}

void setup() {
    cli();

    pinMode(PD5, OUTPUT);
    pinMode(LED_BUILTIN, OUTPUT);

    // Configure Timer 0 as a PWM with 25kHz frequency
    // Fast PWM: count from BOTTOM to TOP; TOP is OCR0A with WGM2:0 = 7
    // Non-inverting compare match: clear OC0B on match with OCR0B; set at BOTTOM
    // Signal will be on PD5
    TCCR0A = 0x23; // OC0A disabled, OC0B will be set on match and cleared on BOTTOM; WGM0[1:0] = 2'b11
    TCCR0B = 0x08; // WGM0[2] = 1'b1 - this gives WGM0 == 7 for fast PWM using OCR0A as TOP
    TCNT0 = 0;
    OCR0A = pwm_period;
    set_duty_cycle(100); // Default to 100% duty cycle
    TCCR0B |= 0x2; // Set prescaler to 8; enable timer
    TIMSK0 = 0; // No interrupts
   
    // Configure Timer 1 as a standard timer with period 3s  
    TCCR1A = 0;
    TCCR1B = 0;
    TCNT1 = 0;
    OCR1A = 15624; // Corresponds to 1s combined with prescalar value of 1024
    TCCR1B |= (1 << WGM12); // Reset timer on match
    TIMSK1 = (1 << OCIE1A); // Interrupt on match

    sei();
    
    TCCR1B |= 0x5; // Set prescaler to 1024; enable timer

    Serial.begin(115200);
}

void loop() {
    static uint8_t char_idx = 0;
    static char char_buffer[3];

    if (alarm) { // Something is wrong
        //digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
        set_duty_cycle(100); // Set fans to max!
        shutoff = false;
        alarm = false;
    }

    bool new_data = false;
    uint16_t temperature;
    while (Serial.available() > 0) {
        char data = Serial.read();
        Serial.write(data);
        if (data == '\n') {
            if (char_idx > 0) {
                temperature = 0;
                for (uint8_t i = 0; i < char_idx; i++) {
                    temperature *= 10;
                    temperature += char_buffer[i] - 0x30;
                }
                new_data = true;
                data_rx = true;
                digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
                char_idx = 0;
            } else {
                alarm = true;
            }
        } else if (('0' <= data) && (data <= '9')) {
            if (char_idx < 3) {
                char_buffer[char_idx++] = data;
            } else {
                alarm == true;
                char_idx = 0;
            }
        } else {
            alarm = true;
            char_idx = 0;
        }
    }

    if (new_data) {
        Serial.print("New temperature of ");
        Serial.print(temperature);
        Serial.print("\n");
        if (temperature <= low_temp) {
            if (shutoff || (temperature <= shutoff_temp)) {
                Serial.print("Low temperature shutoff\n");
                shutoff = true;
                set_duty_cycle(0);          
            } else {
                set_duty_cycle(min_duty_cycle);
            }
        } else if (temperature < high_temp) {
            shutoff = false;
            uint16_t temp_gap = temperature - low_temp;
            uint16_t boost_cycle = ((100 - min_duty_cycle) * temp_gap) / (high_temp - low_temp);
            uint8_t duty_cycle = min_duty_cycle + boost_cycle;
            set_duty_cycle(duty_cycle);
            //Serial.print("Duty cycle of ");
            //Serial.print(duty_cycle);
            //Serial.print("\n");
        } else {
            shutoff = false;
            set_duty_cycle(100);
        }
    }
}

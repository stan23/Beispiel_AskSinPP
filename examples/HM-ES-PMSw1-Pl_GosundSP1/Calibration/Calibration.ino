#include <Arduino.h>
#include <HLW8012.h>

#define EI_NOTEXTERNAL
#include <EnableInterrupt.h>

#define SERIAL_BAUDRATE                 57600

#define CALIB_DEFINED_LOAD_W            40.0
#define CALIB_DEFINED_VOLTAGE_V         230.0

// GPIOs
#define RELAY_PIN                       12
#define SEL_PIN                         17
#define CF1_PIN                         10
#define CF_PIN                          11

// Check values every 10 seconds
#define UPDATE_TIME                     5000

// Set SEL_PIN to HIGH to sample current
// This is the case for Itead's Sonoff POW, where a
// the SEL_PIN drives a transistor that pulls down
// the SEL pin in the HLW8012 when closed
#define CURRENT_MODE                    LOW

// These are the nominal values for the resistors in the circuit
#define CURRENT_RESISTOR                0.001
#define VOLTAGE_RESISTOR_UPSTREAM       ( 2 * 100000 ) // Real: 2280k
#define VOLTAGE_RESISTOR_DOWNSTREAM     ( 1000 ) // Real 1.009k

HLW8012 hlw8012;

// When using interrupts we have to call the library entry point
// whenever an interrupt is triggered
void ICACHE_RAM_ATTR hlw8012_cf1_interrupt() {
  hlw8012.cf1_interrupt();
}
void ICACHE_RAM_ATTR hlw8012_cf_interrupt() {
  hlw8012.cf_interrupt();
}

// Library expects an interrupt on both edges
void setInterrupts() {
  if ( digitalPinToInterrupt(CF1_PIN) == NOT_AN_INTERRUPT ) enableInterrupt(CF1_PIN, hlw8012_cf1_interrupt, FALLING); else attachInterrupt(digitalPinToInterrupt(CF1_PIN), hlw8012_cf1_interrupt, FALLING);
  if ( digitalPinToInterrupt(CF_PIN) == NOT_AN_INTERRUPT ) enableInterrupt(CF_PIN, hlw8012_cf_interrupt, FALLING); else attachInterrupt(digitalPinToInterrupt(CF_PIN), hlw8012_cf_interrupt, FALLING);
//  attachInterrupt(CF1_PIN, hlw8012_cf1_interrupt, FALLING);
//  attachInterrupt(CF_PIN, hlw8012_cf_interrupt, FALLING);
}

void calibrate() {

  // Let some time to register values
  unsigned long timeout = millis();
  while ((millis() - timeout) < 10000) {
    delay(1);
  }

  hlw8012.expectedActivePower(CALIB_DEFINED_LOAD_W);
  hlw8012.expectedVoltage(CALIB_DEFINED_VOLTAGE_V);
  hlw8012.expectedCurrent(CALIB_DEFINED_LOAD_W / CALIB_DEFINED_VOLTAGE_V);

  // Show corrected factors
  Serial.print("[HLW] New current multiplier : "); Serial.println(hlw8012.getCurrentMultiplier());
  Serial.print("[HLW] New voltage multiplier : "); Serial.println(hlw8012.getVoltageMultiplier());
  Serial.print("[HLW] New power multiplier   : "); Serial.println(hlw8012.getPowerMultiplier());

}

void setup() {

  // Init serial port and clean garbage
  Serial.begin(SERIAL_BAUDRATE);
  Serial.println();
  Serial.println();

  // Close the relay to switch on the load
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);

  // Initialize HLW8012
  // void begin(unsigned char cf_pin, unsigned char cf1_pin, unsigned char sel_pin, unsigned char currentWhen = HIGH, bool use_interrupts = false, unsigned long pulse_timeout = PULSE_TIMEOUT);
  // * cf_pin, cf1_pin and sel_pin are GPIOs to the HLW8012 IC
  // * currentWhen is the value in sel_pin to select current sampling
  // * set use_interrupts to true to use interrupts to monitor pulse widths
  // * leave pulse_timeout to the default value, recommended when using interrupts
  hlw8012.begin(CF_PIN, CF1_PIN, SEL_PIN, CURRENT_MODE, true);

  // These values are used to calculate current, voltage and power factors as per datasheet formula
  // These are the nominal values for the Sonoff POW resistors:
  // * The CURRENT_RESISTOR is the 1milliOhm copper-manganese resistor in series with the main line
  // * The VOLTAGE_RESISTOR_UPSTREAM are the 5 470kOhm resistors in the voltage divider that feeds the V2P pin in the HLW8012
  // * The VOLTAGE_RESISTOR_DOWNSTREAM is the 1kOhm resistor in the voltage divider that feeds the V2P pin in the HLW8012
  hlw8012.setResistors(CURRENT_RESISTOR, VOLTAGE_RESISTOR_UPSTREAM, VOLTAGE_RESISTOR_DOWNSTREAM);

  // Show default (as per datasheet) multipliers
  Serial.print("[HLW] Default current multiplier : "); Serial.println(hlw8012.getCurrentMultiplier());
  Serial.print("[HLW] Default voltage multiplier : "); Serial.println(hlw8012.getVoltageMultiplier());
  Serial.print("[HLW] Default power multiplier   : "); Serial.println(hlw8012.getPowerMultiplier());
  Serial.println();

  setInterrupts();

}

char buffer[50];

void loop() {

  static unsigned long last = millis();

  // This UPDATE_TIME should be at least twice the interrupt timeout (2 second by default)
  if ((millis() - last) > UPDATE_TIME) {

    last = millis();
    Serial.print("[HLW] Active Power (W)    : "); Serial.println(hlw8012.getActivePower());
    Serial.print("[HLW] Voltage (V)         : "); Serial.println(hlw8012.getVoltage());
    Serial.print("[HLW] Current (A)         : "); Serial.println(hlw8012.getCurrent());
    Serial.print("[HLW] Apparent Power (VA) : "); Serial.println(hlw8012.getApparentPower());
    Serial.print("[HLW] Power Factor (%)    : "); Serial.println((int) (100 * hlw8012.getPowerFactor()));
    Serial.print("[HLW] Agg. energy (Ws)    : "); Serial.println(hlw8012.getEnergy());
    Serial.println();
    calibrate();

  }

}

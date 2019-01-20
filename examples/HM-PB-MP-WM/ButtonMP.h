//- -----------------------------------------------------------------------------------------------------------------------
// AskSin++
// 2016-10-31 papa Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
// 2018-08-20 jp112sdl Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//- -----------------------------------------------------------------------------------------------------------------------
#ifndef __BUTTON_H__
#define __BUTTON_H__


#include <Pins.h>
#include <Debug.h>
#include <Activity.h>
#include <Led.h>
#include <AlarmClock.h>
#include <Message.h>
#include <Button.h>
#include <Radio.h>
#include <BatterySensor.h>

#if ARDUINO_ARCH_AVR or ARDUINO_ARCH_ATMEGA32
typedef uint8_t WiringPinMode;
#endif

namespace as {

template <uint8_t OFFSTATE = HIGH, uint8_t ONSTATE = LOW, WiringPinMode MODE = INPUT_PULLUP>
class StateButton: public Alarm {

#define DEBOUNCETIME millis2ticks(50)

  public:
    enum States {
      invalid = 0,
      none = 1,
      released = 2,
      pressed = 3,
      debounce = 4,
      longpressed = 5,
      longreleased = 6,
    };

    class CheckAlarm : public Alarm {
      public:
        StateButton& sb;
        CheckAlarm (StateButton& _sb) : Alarm(0), sb(_sb) {}
        ~CheckAlarm () {}
        virtual void trigger(__attribute__((unused)) AlarmClock& clock) {
          sb.check();
        }
    };

    class MultiPressAlarm : public Alarm {
      public:
        StateButton& sb;
        MultiPressAlarm (StateButton& _sb) : Alarm(0), sb(_sb) {}
        ~MultiPressAlarm () {}
        virtual void trigger(__attribute__((unused)) AlarmClock& clock) {
          sb.multi();
        }
    };

  protected:
    uint8_t  stat     : 3;
    uint8_t  pinstate : 1;
    uint8_t  pin;
    uint16_t longpresstime;
    CheckAlarm ca;
    MultiPressAlarm ma;
    uint8_t presscount;
    uint8_t mul;

  public:
    StateButton() :
      Alarm(0), stat(none), pinstate(OFFSTATE), pin(0), longpresstime(millis2ticks(400)), ca(*this), ma(*this), presscount(0), mul(0)  {
    }
    virtual ~StateButton() {
    }

    void setLongPressTime(uint16_t t) {
      longpresstime = t;
    }

    uint8_t getPin () {
      return pin;
    }

    virtual void multi (uint8_t mul) {
      mul = presscount;
      presscount = 0;
    }

    uint8_t multi() const {
      multi(presscount);
      return mul;
    }

    virtual void trigger(AlarmClock& clock) {
      uint8_t  nextstate = invalid;
      uint16_t nexttick = 0;
      switch ( state() ) {
        case released:
          ma.set(millis2ticks(MULTIPRESSTIME));
          sysclock.cancel(ma);
          sysclock.add(ma);
        case longreleased:
          nextstate = none;
          break;

        case debounce:
          nextstate = pressed;
          if (pinstate == ONSTATE) {
            // set timer for detect longpressed
            nexttick = longpresstime - DEBOUNCETIME;
          } else {
            nextstate = released;
            nexttick = DEBOUNCETIME;
          }
          break;

        case pressed:
        case longpressed:
          if ( pinstate == ONSTATE) {
            nextstate = longpressed;
            nexttick = longpresstime;
          }
          break;
      }
      // reactivate alarm if needed
      if ( nexttick != 0 ) {
        tick = nexttick;
        clock.add(*this);
      }
      // trigger the state change
      if ( nextstate != invalid ) {
        state(nextstate);
      }
    }

    virtual void state(uint8_t s) {
      switch (s) {
        case released: DPRINTLN(F(" released")); break;
        case pressed: DPRINTLN(F(" pressed")); break;
        case debounce: DPRINTLN(F(" debounce")); break;
        case longpressed: DPRINTLN(F(" longpressed")); break;
        case longreleased: DPRINTLN(F(" longreleased")); break;
        default: DPRINTLN(F("")); break;
      }
      stat = s;
    }

    uint8_t state() const {
      return stat;
    }

    void irq () {
      sysclock.cancel(ca);
      // use alarm to run code outside of interrupt
      sysclock.add(ca);
    }

    void check() {
      uint8_t ps = digitalRead(pin);
      if ( pinstate != ps ) {
        pinstate = ps;
        uint16_t nexttick = 0;
        uint8_t  nextstate = state();
        switch ( state() ) {
          case none:
            nextstate = debounce;
            nexttick = DEBOUNCETIME;
            break;

          case pressed:
            presscount++;
            sysclock.cancel(ma);
          case longpressed:
            if (pinstate == OFFSTATE) {
              nextstate = state() == pressed ? released : longreleased;
              nexttick = DEBOUNCETIME;
            }
            break;
          default:
            break;
        }
        if ( nexttick != 0 ) {
          sysclock.cancel(*this);
          tick = nexttick;
          sysclock.add(*this);
        }
        if ( nextstate != state () ) {
          state(nextstate);
        }
      }
    }

    void init(uint8_t pin) {
      this->pin = pin;
      pinMode(pin, MODE);
    }
};

// define standard button switches to GND
typedef StateButton<HIGH, LOW, INPUT_PULLUP> Button;

template <class DEVTYPE, uint8_t OFFSTATE = HIGH, uint8_t ONSTATE = LOW, WiringPinMode MODE = INPUT_PULLUP>
class ConfigButton : public StateButton<OFFSTATE, ONSTATE, MODE> {
    DEVTYPE& device;
  public:
    typedef StateButton<OFFSTATE, ONSTATE, MODE> ButtonType;

    ConfigButton (DEVTYPE& dev, uint8_t longpresstime = 3) : device(dev) {
      this->setLongPressTime(seconds2ticks(longpresstime));
    }
    virtual ~ConfigButton () {}
    virtual void state (uint8_t s) {
      uint8_t old = ButtonType::state();
      ButtonType::state(s);
      if ( s == ButtonType::released ) {
        device.startPairing();
      }
      else if ( s == ButtonType::longpressed ) {
        if ( old == ButtonType::longpressed ) {
          device.reset(); // long pressed again - reset
        }
        else {
          device.led().set(LedStates::key_long);
        }
      }
    }
};

#define buttonISR(btn,pin) class btn##ISRHandler { \
    public: \
      static void isr () { btn.irq(); } \
  }; \
  btn.init(pin); \
  if( digitalPinToInterrupt(pin) == NOT_AN_INTERRUPT ) \
    enableInterrupt(pin,btn##ISRHandler::isr,CHANGE); \
  else \
    attachInterrupt(digitalPinToInterrupt(pin),btn##ISRHandler::isr,CHANGE);

}

#endif

#include <Adafruit_TinyUSB.h>

// HID report descriptor using TinyUSB's template
// Single Report (no ID) descriptor
uint8_t const desc_hid_report[] = {
  TUD_HID_REPORT_DESC_GAMEPAD()
};

// USB HID object.
Adafruit_USBD_HID usb_hid;

// Report payload defined in src/class/hid/hid.h
// - For Gamepad Button Bit Mask see  hid_gamepad_button_bm_t
// - For Gamepad Hat    Bit Mask see  hid_gamepad_hat_t
hid_gamepad_report_t gp;

// Define the GPIO pins connected to the D-pad
const int upPin = D0;
const int downPin = D1;
const int leftPin = D2;
const int rightPin = D3;

// Define the GPIO pins connected to the buttons
const uint8_t buttonPins[] = { D4, D5, D6, D7, D8, D9, D10, D11, D12, D13, D14, D15 };

uint32_t previousButtons = 0;
uint8_t previousStick = 0;

uint32_t currentButtons = 0;
uint8_t currentStick = 0;

void setup() {
  // Manual begin() is required on core without built-in support e.g. mbed rp2040
  if (!TinyUSBDevice.isInitialized()) {
    TinyUSBDevice.begin(0);
  }

  // Set up HID
  usb_hid.setPollInterval(2);
  usb_hid.setReportDescriptor(desc_hid_report, sizeof(desc_hid_report));
  usb_hid.setStringDescriptor("TinyUSB HID Arcade Controls");
  usb_hid.begin();

  // If already enumerated, additional class driver begin()
  // e.g msc, hid, midi won't take effect until re-enumeration
  if (TinyUSBDevice.mounted()) {
    TinyUSBDevice.detach();
    delay(10);
    TinyUSBDevice.attach();
  }

  pinMode(upPin, INPUT_PULLUP);
  pinMode(downPin, INPUT_PULLUP);
  pinMode(leftPin, INPUT_PULLUP);
  pinMode(rightPin, INPUT_PULLUP);

  // Configure button pins as inputs with pull-up resistors
  for (int i = 0; i < sizeof(buttonPins) / sizeof(buttonPins[0]); i++) {
    pinMode(buttonPins[i], INPUT_PULLUP);
  }
  // initialize digital pin LED_BUILTIN as an output.
  pinMode(LED_BUILTIN, OUTPUT);
}

void process_hid() {
  // skip if hid is not ready e.g still transferring previous report
  if (!usb_hid.ready()) return;

  // used to avoid send multiple consecutive zero report for keyboard
  static bool reportSent = false;

  currentButtons = getButtonMask();
  currentStick = getDpadDirection();
  gp.buttons = currentButtons;
  gp.hat = currentStick;

  // if any input then send report
  if (currentButtons != 0 || currentStick != 0) {
    usb_hid.sendReport(0, &gp, sizeof(gp));
    digitalWrite(LED_BUILTIN, HIGH);

    reportSent = true;
    // save current values for next loop
    previousButtons = currentButtons;
    previousStick = currentStick;

  } else {
    // if there is no input
    if (reportSent) {
      // and a none-zero report was sent
      // then send a zero report
      usb_hid.sendReport(0, &gp, sizeof(gp));
      digitalWrite(LED_BUILTIN, LOW);
      // set false so another zero report will not be sent
      reportSent = false;
    }
  }
}

// Function to set the bits of the button mask
uint32_t getButtonMask() {
  uint32_t buttonMask = 0;

  for (int i = 0; i < sizeof(buttonPins) / sizeof(buttonPins[0]); i++) {
    if (digitalRead(buttonPins[i]) == LOW) {
      // Set the corresponding bit in the button mask
      buttonMask |= (1 << i);
    }
  }

  return buttonMask;
}

// Function to get the current D-pad direction
hid_gamepad_hat_t getDpadDirection() {
  bool up = digitalRead(upPin) == LOW;
  bool down = digitalRead(downPin) == LOW;
  bool left = digitalRead(leftPin) == LOW;
  bool right = digitalRead(rightPin) == LOW;

  if (up && left) {
    return GAMEPAD_HAT_UP_LEFT;
  } else if (up && right) {
    return GAMEPAD_HAT_UP_RIGHT;
  } else if (down && left) {
    return GAMEPAD_HAT_DOWN_LEFT;
  } else if (down && right) {
    return GAMEPAD_HAT_DOWN_RIGHT;
  } else if (up) {
    return GAMEPAD_HAT_UP;
  } else if (down) {
    return GAMEPAD_HAT_DOWN;
  } else if (left) {
    return GAMEPAD_HAT_LEFT;
  } else if (right) {
    return GAMEPAD_HAT_RIGHT;
  } else {
    return GAMEPAD_HAT_CENTERED;
  }
}

void loop() {
#ifdef TINYUSB_NEED_POLLING_TASK
  // Manual call tud_task since it isn't called by Core's background
  TinyUSBDevice.task();
#endif

  // not enumerated()/mounted() yet: nothing to do
  if (!TinyUSBDevice.mounted()) {
    return;
  }

  // poll gpio once each 10 ms
  static uint32_t ms = 0;
  if (millis() - ms > 10) {
    ms = millis();
    process_hid();
  }
}

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
  // used to avoid send multiple consecutive zero report for keyboard
  static bool keyPressedPreviously = false;

  gp.buttons = setButtonMask();
  gp.hat = getDpadDirection();

  // skip if hid is not ready e.g still transferring previous report
  if (!usb_hid.ready()) return;

  // if any input was LOW count will be >0
  if (gp.buttons != 0 || gp.hat != 0) {
    keyPressedPreviously = true;
    usb_hid.sendReport(0, &gp, sizeof(gp));
    digitalWrite(LED_BUILTIN, HIGH);

  } else {
    // Send All-zero report to indicate there is no keys pressed
    // Most of the time, it is, though we don't need to send zero report
    // every loop(), only a key is pressed in previous loop()
    if (keyPressedPreviously) {
      keyPressedPreviously = false;
      usb_hid.sendReport(0, &gp, sizeof(gp));
      digitalWrite(LED_BUILTIN, LOW);
    }
  }
}

// Function to set the bits of the button mask
uint32_t setButtonMask() {
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
